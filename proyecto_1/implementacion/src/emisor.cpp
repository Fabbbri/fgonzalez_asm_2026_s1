#include <Arduino.h>
#include <arduinoFFT.h>
#include <math.h>

// ================= SELECTOR DE SEÑAL =================
// Descomente la que quiere usar, solo una a la vez

// Señales portadas desde `proyecto_1/simulation.py` (simple/compuesta/voz)
// Nota: acá se generan con el esquema actual del sistema (N=128, Fs=8000)
// para NO modificar el frame ni el `receptor.cpp`.
#define SIGNAL_PY_SIMPLE
// #define SIGNAL_PY_COMPOSITE
// #define SIGNAL_PY_VOICE

// Señales legacy (previas):
// #define SIGNAL_SINE
// #define SIGNAL_SQUARE
// #define SIGNAL_COMPOSITE

// Nota: pines de diagnóstico (Pico) se definen más abajo.
// =====================================================

// ================= CONFIG =================
const int      dacPin            = 25;
const int      adcPin            = 34;
constexpr uint16_t SAMPLES       = 128;
const double   Fs                = 8000.0;
const double   FUND_HZ           = 500.0;   // frecuencia base para señales legacy
const double   ENERGY_THRESHOLD  = 0.95;
const uint8_t  MAX_BINS          = 64;
const uint8_t  FRAME_HEADER      = 0xAA;
const uint32_t UART_BAUD_R1      = 460800;
const uint32_t UART_BAUD_R2      = 115200;
const float    HAMMING_COMPENSATION = 1.0f / 0.54f;

// Escala pico para mapear señal float -> DAC (0..255, centro 128)
constexpr float DAC_PEAK = 120.0f;

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

// Referencia enviada al receptor para métricas (0..255, centrado en 128)
// Se calcula desde las mismas muestras usadas para la FFT (ADC) luego de:
// dcRemoval + ventana Hamming + ganancia (similar al receptor).
static uint8_t refU8[SAMPLES];

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

static inline uint8_t clampU8(int x) {
  if (x < 0) return 0;
  if (x > 255) return 255;
  return (uint8_t)x;
}

// ================= RNG (ruido gaussiano estilo np.random.randn) =================
static uint32_t rngState = 0xC0FFEE01u;

static inline uint32_t xorshift32() {
  // xorshift32 clásico (rápido, suficiente para generar ruido en test)
  uint32_t x = rngState;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rngState = x;
  return x;
}

static inline float randUniform01() {
  // (0,1) abierto en 0 para evitar log(0)
  uint32_t r = xorshift32();
  // 24 bits de mantisa
  float u = ((r >> 8) + 1.0f) * (1.0f / 16777217.0f);
  return u;
}

static inline float randn() {
  // Box-Muller: N(0,1)
  const float u1 = randUniform01();
  const float u2 = randUniform01();
  const float r = sqrtf(-2.0f * logf(u1));
  const float theta = 2.0f * (float)PI * u2;
  return r * cosf(theta);
}

// Tabla discreta de una señal periodica
void buildSignalTable() {
  float raw[SAMPLES];

  // Fs usados en `proyecto_1/simulation.py` (referencias para mapear frecuencias)
  constexpr float FS_PY_SIMPLE    = 128.0f;
  constexpr float FS_PY_COMPOSITE = 1024.0f;
  constexpr float FS_PY_VOICE     = 2048.0f;

  for (uint16_t i = 0; i < SAMPLES; i++) {
    const float t = (float)i / (float)Fs;
    const float t01 = (SAMPLES > 1) ? ((float)i / (float)(SAMPLES - 1)) : 0.0f;
    float x = 0.0f;

#if defined(SIGNAL_PY_SIMPLE)
    // X_SIMPLE = sin(2*pi*5*t)
  // En Python: FS_SIMPLE=128. Para que no quede casi DC con Fs=8000 y N=128,
  // mapeamos f_py -> f_hw: f_hw = f_py * Fs / FS_PY_SIMPLE.
  x = sinf(2.0f * (float)PI * (5.0f * (float)Fs / FS_PY_SIMPLE) * t);

#elif defined(SIGNAL_PY_COMPOSITE)
    // X_COMPLEJO = 0.7*sin(2*pi*5*t) + 0.5*sin(2*pi*20*t) + 0.3*sin(2*pi*60*t) + 0.2*randn()
    // En Python: FS_COMPLEJO=1024. Mapeo f_py -> f_hw: f_hw = f_py * Fs / FS_PY_COMPOSITE.
    x = 0.7f * sinf(2.0f * (float)PI * (5.0f  * (float)Fs / FS_PY_COMPOSITE) * t)
      + 0.5f * sinf(2.0f * (float)PI * (20.0f * (float)Fs / FS_PY_COMPOSITE) * t)
      + 0.3f * sinf(2.0f * (float)PI * (60.0f * (float)Fs / FS_PY_COMPOSITE) * t)
      + 0.2f * randn();

#elif defined(SIGNAL_PY_VOICE)
    // X_VOZ = exp(-3*t) * (sin(2*pi*120*t) + 0.5*sin(2*pi*250*t) + 0.3*sin(2*pi*400*t)) + 0.05*randn()
    {
      // En Python: FS_VOZ=2048. Mapeo f_py -> f_hw: f_hw = f_py * Fs / FS_PY_VOICE.
      // Para conservar la forma de la envolvente en un frame de 128 muestras, aplicamos exp(-3*t01).
      const float env = expf(-3.0f * t01);
      x = env * (sinf(2.0f * (float)PI * (120.0f * (float)Fs / FS_PY_VOICE) * t)
               + 0.5f * sinf(2.0f * (float)PI * (250.0f * (float)Fs / FS_PY_VOICE) * t)
               + 0.3f * sinf(2.0f * (float)PI * (400.0f * (float)Fs / FS_PY_VOICE) * t))
        + 0.05f * randn();
    }

#elif defined(SIGNAL_SINE)
    // Legacy: senoide pura (FUND_HZ)
    x = sinf(2.0f * (float)PI * (float)FUND_HZ * t);

#elif defined(SIGNAL_SQUARE)
    // Legacy: cuadrada (aprox) a FUND_HZ
    x = (sinf(2.0f * (float)PI * (float)FUND_HZ * t) >= 0.0f) ? 1.0f : -1.0f;

#elif defined(SIGNAL_COMPOSITE)
    // Legacy: compuesta 1x + 2x + 3x FUND_HZ
    {
      const float a = 2.0f * (float)PI * (float)FUND_HZ * t;
      x = 1.0f * sinf(a) + 0.3125f * sinf(2.0f * a) + 0.125f * sinf(3.0f * a);
    }

#else
  #error "Debes definir una señal: SIGNAL_PY_SIMPLE, SIGNAL_PY_COMPOSITE, SIGNAL_PY_VOICE o alguna legacy"
#endif

    raw[i] = x;
  }

  float maxAbs = 1.0f;
  for (uint16_t i = 0; i < SAMPLES; i++) {
    const float a = fabsf(raw[i]);
    if (a > maxAbs) maxAbs = a;
  }
  const float gain = DAC_PEAK / maxAbs;

  for (uint16_t i = 0; i < SAMPLES; i++) {
    const int s = (int)lroundf(128.0f + raw[i] * gain);
    signalTable[i] = clampU8(s);
  }

  // Imprimir qué señal se está usando
  Serial.print("Señal activa: ");
#if defined(SIGNAL_PY_SIMPLE)
  Serial.println("PY SIMPLE: sin(2π·5t)");
  Serial.println("  (f mapeada desde Fs=128 -> Fs=8000 para evitar DC)");
#elif defined(SIGNAL_PY_COMPOSITE)
  Serial.println("PY COMPUESTA: 0.7·sin(5) + 0.5·sin(20) + 0.3·sin(60) + ruido");
  Serial.println("  (f mapeadas desde Fs=1024 -> Fs=8000)");
#elif defined(SIGNAL_PY_VOICE)
  Serial.println("PY VOZ: exp(-3t)·(sin(120)+0.5·sin(250)+0.3·sin(400)) + ruido");
  Serial.println("  (f mapeadas desde Fs=2048 -> Fs=8000; envolvente usa t01)");
#elif defined(SIGNAL_SINE)
  Serial.printf("LEGACY SENOIDAL — %.0f Hz\n", FUND_HZ);
  Serial.println("  Esperado: 1 bin dominante");
#elif defined(SIGNAL_SQUARE)
  Serial.printf("LEGACY CUADRADA — %.0f Hz\n", FUND_HZ);
  Serial.println("  Esperado: armónicos impares");
#elif defined(SIGNAL_COMPOSITE)
  Serial.printf("LEGACY COMPUESTA — %.0f + %.0f + %.0f Hz\n", FUND_HZ, FUND_HZ*2, FUND_HZ*3);
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

  // Construir referencia coherente con lo que se reconstruye (señal ya ventaneada)
  // Escalamos a 0..255 con centro 128, usando un gain por bloque.
  double maxAbs = 1.0;
  for (uint16_t i = 0; i < SAMPLES; i++) {
    double a = fabs(vReal[i]);
    if (a > maxAbs) maxAbs = a;
  }
  const double gain = 120.0 / maxAbs;
  for (uint16_t i = 0; i < SAMPLES; i++) {
    int s = (int)lround(128.0 + vReal[i] * gain);
    refU8[i] = clampU8(s);
  }

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

  // Referencia: señal coherente con la FFT (ADC + dcRemoval + Hamming + escalado)
  for (uint16_t i = 0; i < SAMPLES; i++) {
    buf[idx++] = refU8[i];
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