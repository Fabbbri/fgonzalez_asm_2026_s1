#include <Arduino.h>
#include <arduinoFFT.h>
#include <math.h>

// ================= SELECTOR DE SEÑAL =================
// Descomente la que quiere usar, solo una a la vez

#define SIGNAL_SINE       // Tono senoidal puro (500 Hz)
// #define SIGNAL_SQUARE  // Onda cuadrada (500 Hz)
// #define SIGNAL_COMPOSITE  // Señal compuesta (500 Hz)
// Nota: pines de diagnóstico (Pico) se definen más abajo.
// =====================================================

// ================= CONFIG =================
const int      dacPin            = 25;
const int      adcPin            = 34;
const uint16_t SAMPLES           = 128;
const double   Fs                = 8000.0;
const double   FUND_HZ           = 500.0;   // frecuencia base para todas las señales
const double   ENERGY_THRESHOLD  = 0.95;
const uint8_t  MAX_BINS          = 64;
const uint8_t  FRAME_HEADER      = 0xAA;
const uint32_t UART_BAUD_R1      = 460800;
const uint32_t UART_BAUD_R2      = 115200;
const float    HAMMING_COMPENSATION = 1.0f / 0.54f;

// DIAGNÓSTICO DE CABLEADO ESP32->PICO
// 0: modo normal (FFT + frames)
// 1: test UART (Serial1 escribe 0x55 continuamente)
// 2: test GPIO (toggle de un GPIO normal, sin UART)
// Útil si en la Pico ves RX=0 / edges=0.
// Para validar primero la conexión UART con la Pico, usa 1.
// Luego vuelve a 0 para operación normal.
#define PICO_WIRE_TEST_MODE 0

// Solo para PICO_WIRE_TEST_MODE=2
// Recomendación: evita pines de strapping (0, 2, 4, 5, 12, 15).
// GPIO23 suele estar siempre disponible en Dev boards.
#define PICO_TEST_GPIO 23

// Solo para PICO_WIRE_TEST_MODE=2
// Hacerlo lento ayuda a que MicroPython/Thonny lo detecte sin perder edges.
// 250 ms => 2 Hz (HIGH 250 ms, LOW 250 ms)
#define PICO_GPIO_TEST_HALF_PERIOD_MS 250

// UARTs
// - Serial2 (UART2): Receptor 1 (ESP32)  RX=16, TX=17 (como ya estaba)
// - Serial1 (UART1): Receptor 2 (Pico)   RX=21 (opcional), TX=22
static const int UART_R1_RX = 16;
static const int UART_R1_TX = 17;
static const int UART_R2_RX = 21;
static const int UART_R2_TX = 22;

double vReal[SAMPLES];  // Parte real de las muestras
double vImag[SAMPLES];  // Parte Imaginarioas
double vPhase[SAMPLES / 2]; //Fase de cada prueba
ArduinoFFT<double> FFT(vReal, vImag, SAMPLES, Fs); 

// Almacena coeficientes espectrales
struct SpectralCoeff {
  uint16_t index;
  float    mag;
  float    phase;
};

SpectralCoeff selected[MAX_BINS];
uint8_t selectedCount = 0;

static float totalEnergyAll = 0.0f;

// Tabla precalculada de la señal elegida
uint8_t signalTable[SAMPLES];

void     buildSignalTable();
void     computeFFT();
uint8_t  selectByEnergy(double threshold);
void     transmitFrame(SpectralCoeff* coeffs, uint8_t count, float totalEnergy);
uint8_t  calcChecksum(uint8_t* data, uint16_t len);
void     printSpectralSummary(SpectralCoeff* coeffs, uint8_t count);

// Tabla discreta de una señal periodica
void buildSignalTable() {
  for (int i = 0; i < SAMPLES; i++) {
    double angle = 2.0 * PI * FUND_HZ * i / Fs;
    double wave  = 0.0;

#if defined(SIGNAL_SINE)
    // Senoide pura 
    wave = 120.0 * sin(angle);

#elif defined(SIGNAL_SQUARE)
    // Onda cuadrada 
    wave = ((i % 16) < 8) ? 120.0 : -120.0;

#elif defined(SIGNAL_COMPOSITE)
    // Señal compuesta con 3 armónicos 
    wave = 80.0 * sin(angle)
         + 25.0 * sin(2.0 * angle)   // 1000 Hz
         + 10.0 * sin(3.0 * angle);  // 1500 Hz

#else
  #error "Debes definir una señal: SIGNAL_SINE, SIGNAL_SQUARE o SIGNAL_COMPOSITE"
#endif

    signalTable[i] = (uint8_t)(127.5 + wave);
  }

  // Imprimir qué señal se está usando
  Serial.print("Señal activa: ");
#if defined(SIGNAL_SINE)
  Serial.printf("SENOIDAL pura — %.0f Hz\n", FUND_HZ);
  Serial.println("  Esperado: 1 bin dominante");
#elif defined(SIGNAL_SQUARE)
  Serial.printf("CUADRADA — %.0f Hz\n", FUND_HZ);
  Serial.println("  Esperado: bins en 500, 1500, 2500, 3500 Hz (armónicos impares)");
#elif defined(SIGNAL_COMPOSITE)
  Serial.printf("COMPUESTA — %.0f + %.0f + %.0f Hz\n", FUND_HZ, FUND_HZ*2, FUND_HZ*3);
  Serial.println("  Esperado: 3 bins dominantes");
#endif
}

// Inicia puertos seriales. Monitoreo por consola y envio al receptor
void setup() {
  Serial.begin(115200);
  Serial2.begin(UART_BAUD_R1, SERIAL_8N1, UART_R1_RX, UART_R1_TX);
  Serial1.begin(UART_BAUD_R2, SERIAL_8N1, UART_R2_RX, UART_R2_TX);

#if PICO_WIRE_TEST_MODE == 2
  pinMode(PICO_TEST_GPIO, OUTPUT);
#endif

  delay(500);

  buildSignalTable();

  Serial.println("=== TRANSMISOR FFT listo ===");
  Serial.printf("Fs=%.1f Hz | N=%u | Res=%.2f Hz/bin | UART=%lu\n",
                Fs, SAMPLES, Fs / SAMPLES, UART_BAUD_R1);
  Serial.printf("UART Receptor 2 (Pico)=%lu\n", UART_BAUD_R2);
  Serial.println("Conecta GPIO25 -> GPIO34 (con resistencia 1k)");
}

void loop() {
#if PICO_WIRE_TEST_MODE == 1
  // Test UART: conectar ESP32 GPIO22 (TX Serial1) -> Pico GP5
  // 0x55 = 01010101 (bueno para ver edges)
  Serial1.write((uint8_t)0x55);
  delay(1);
  return;
#elif PICO_WIRE_TEST_MODE == 2
  // Test GPIO: conectar ESP32 PICO_TEST_GPIO -> Pico GPx (cualquiera) + GND común
  // Objetivo: ver transiciones reales en la Pico (ideal: contar edges con IRQ).
  static bool level = false;
  static uint32_t toggles = 0;
  static uint32_t lastPrintMs = 0;

  level = !level;
  digitalWrite(PICO_TEST_GPIO, level ? HIGH : LOW);
  toggles++;

  const uint32_t nowMs = millis();
  if ((uint32_t)(nowMs - lastPrintMs) >= 1000u) {
    lastPrintMs = nowMs;
    Serial.printf("[GPIO_TEST] pin=%d level=%d toggles=%lu\n", PICO_TEST_GPIO, level ? 1 : 0, (unsigned long)toggles);
  }

  delay(PICO_GPIO_TEST_HALF_PERIOD_MS);
  return;
#endif

  const uint32_t period_us = (uint32_t)llround(1e6 / Fs);
  uint32_t t = micros();
  uint32_t skipped = 0;

  // Para cada muestra, escribe al DAC, lee la respuesta en el ADC, se repseta el timing
  for (uint16_t i = 0; i < SAMPLES; i++) {
    dacWrite(dacPin, signalTable[i]);
    vReal[i] = (double)analogRead(adcPin);
    vImag[i] = 0.0;

    uint32_t elapsed = (uint32_t)(micros() - t);
    if (elapsed >= period_us) {
      skipped++;
      t = micros();
    } else {
      while ((uint32_t)(micros() - t) < period_us) {}
      t += period_us;
    }
  }

  if (skipped > 0) {
    Serial.printf("[WARN] %u muestras con timing tardío\n", skipped);
  }

  //Calcula FFT
  computeFFT();

  // Selecciona bins mas energeticos
  selectedCount = selectByEnergy(ENERGY_THRESHOLD);

  // Se transmite al receptor
  transmitFrame(selected, selectedCount, totalEnergyAll);

  // Se imprime en consola
  printSpectralSummary(selected, selectedCount);
}

// Calculo de FFT, se elimina la parte CD
void computeFFT() {
  FFT.dcRemoval();
  FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(FFT_FORWARD);

  for (uint16_t i = 0; i < SAMPLES / 2; i++) {
    vPhase[i] = (float)atan2(vImag[i], vReal[i]);
  }

  FFT.complexToMagnitude();

  for (uint16_t i = 1; i < SAMPLES / 2; i++) {
    vReal[i] *= HAMMING_COMPENSATION;
  }
}

// Calcula los bins mas significativos en la parte energetica
uint8_t selectByEnergy(double threshold) {
  const uint8_t N2 = SAMPLES / 2;

  for (uint8_t i = 0; i < N2 - 1; i++) {
    selected[i].index = i + 1;
    selected[i].mag   = (float)vReal[i + 1];
    selected[i].phase = vPhase[i + 1];
  }

  uint8_t total = N2 - 1;

  double totalEnergy = 0.0;
  for (uint8_t i = 0; i < total; i++) {
    totalEnergy += (double)selected[i].mag * selected[i].mag;
  }
  totalEnergyAll = (float)totalEnergy;

  for (uint8_t i = 1; i < total; i++) {
    SpectralCoeff key = selected[i];
    int8_t j = i - 1;
    while (j >= 0 && selected[j].mag < key.mag) {
      selected[j + 1] = selected[j];
      j--;
    }
    selected[j + 1] = key;
  }

  double accumulated = 0.0;
  uint8_t k = 0;
  while (k < total) {
    accumulated += (double)selected[k].mag * selected[k].mag;
    k++;
    if (accumulated / totalEnergy >= threshold) break;
  }

  return k;
}

// Envio de datos
// Frame v2:
// [0]    0xAA
// [1]    K
// [2..5] totalEnergy (float32 LE)
// [..]   K * (index u16 BE + mag f32 LE + phase f32 LE)
// [..]   origSamples (SAMPLES bytes, uint8)
// [end]  checksum XOR de todos los bytes previos
void transmitFrame(SpectralCoeff* coeffs, uint8_t count, float totalEnergy) {
  static uint8_t buf[2 + 4 + 10 * MAX_BINS + SAMPLES + 1];
  uint16_t idx = 0;

  buf[idx++] = FRAME_HEADER;
  buf[idx++] = count;

  // totalEnergy
  uint8_t* te = (uint8_t*)&totalEnergy;
  buf[idx++] = te[0];
  buf[idx++] = te[1];
  buf[idx++] = te[2];
  buf[idx++] = te[3];

  for (uint8_t i = 0; i < count; i++) {
    buf[idx++] = (uint8_t)(coeffs[i].index >> 8);
    buf[idx++] = (uint8_t)(coeffs[i].index & 0xFF);

    uint8_t* fm = (uint8_t*)&coeffs[i].mag;
    buf[idx++] = fm[0]; buf[idx++] = fm[1];
    buf[idx++] = fm[2]; buf[idx++] = fm[3];

    uint8_t* fp = (uint8_t*)&coeffs[i].phase;
    buf[idx++] = fp[0]; buf[idx++] = fp[1];
    buf[idx++] = fp[2]; buf[idx++] = fp[3];
  }

  // Referencia: señal original (tabla de la señal generada)
  for (uint16_t i = 0; i < SAMPLES; i++) {
    buf[idx++] = signalTable[i];
  }

  buf[idx++] = calcChecksum(buf, idx);

  // Duplicar por ambas UART
  Serial2.write(buf, idx);
  Serial1.write(buf, idx);
}

// Detección de errores basicos de UART
uint8_t calcChecksum(uint8_t* data, uint16_t len) {
  uint8_t cs = 0;
  for (uint16_t i = 0; i < len; i++) cs ^= data[i];
  return cs;
}

// Impresión en el monitor serial
void printSpectralSummary(SpectralCoeff* coeffs, uint8_t count) {
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint < 500) return;
  lastPrint = millis();

  const uint8_t totalBins = SAMPLES / 2 - 1;
  float ratio = 100.0f * count / totalBins;
  Serial.printf("Frame: %u/%u bins (%.1f%% usados, %.1f%% comprimido)\n",
                count, totalBins, ratio, 100.0f - ratio);

  for (uint8_t i = 0; i < count; i++) {
    double freqHz = ((double)coeffs[i].index * Fs) / SAMPLES;
    Serial.printf("  Bin %2u -> %6.1f Hz | Mag %.2f | Phase %.3f\n",
                  coeffs[i].index, freqHz, coeffs[i].mag, coeffs[i].phase);
  }
}