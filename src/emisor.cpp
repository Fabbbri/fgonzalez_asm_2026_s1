#include <Arduino.h>
#include <arduinoFFT.h>
#include <math.h>

// ─── Configuración ────────────────────────────────────────────────
const int      dacPin            = 25;
const int      adcPin            = 34;
const uint16_t SAMPLES           = 128;
const double   FREQ              = 2.0;
const double   Fs                = FREQ * SAMPLES;   // 256 Hz
const double   ENERGY_THRESHOLD  = 0.90;             // conservar 90% energía
const uint8_t  MAX_BINS          = 64;               // SAMPLES/2
const uint8_t  FRAME_HEADER      = 0xAA;

// ─── Buffers FFT ──────────────────────────────────────────────────
double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT(vReal, vImag, SAMPLES, Fs);

// ─── Estructura de coeficiente espectral ──────────────────────────
struct SpectralCoeff {
  uint16_t index;   // número de bin
  float    mag;     // magnitud
};

SpectralCoeff selected[MAX_BINS];  // peor caso: todos los bins
uint8_t       selectedCount = 0;

// ─── Tabla seno ───────────────────────────────────────────────────
uint8_t sineTable[SAMPLES];

// ─── Prototipos ───────────────────────────────────────────────────
void     computeFFT();
uint8_t  selectByEnergy(double threshold);
void     transmitFrame(SpectralCoeff* coeffs, uint8_t count);
uint8_t  calcChecksum(uint8_t* data, uint16_t len);
void     printSpectralSummary(SpectralCoeff* coeffs, uint8_t count);

// ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);   // debug / monitor
  Serial2.begin(115200, SERIAL_8N1, 16, 17);  // TX=17, RX=16 → UART hacia otros MCU

  delay(1000);

  for (int i = 0; i < SAMPLES; i++) {
    float angle  = 2.0f * PI * i / SAMPLES;
    sineTable[i] = (uint8_t)(127.5f + 127.5f * sinf(angle));
  }

  Serial.println("=== Compresión Espectral Adaptativa ===");
  Serial.printf("Fs=%.1f Hz | N=%u | Res=%.2f Hz | Umbral=%.0f%%\n",
                Fs, SAMPLES, Fs / SAMPLES, ENERGY_THRESHOLD * 100.0);
}

// ──────────────────────────────────────────────────────────────────
void loop() {
  // 1) Muestrear
  const uint32_t period_us = (uint32_t)llround(1e6 / Fs);
  uint32_t t = micros();
  for (uint16_t i = 0; i < SAMPLES; i++) {
    dacWrite(dacPin, sineTable[i]);
    vReal[i] = (double)analogRead(adcPin);
    vImag[i] = 0.0;
    while ((uint32_t)(micros() - t) < period_us) {}
    t += period_us;
  }

  // 2) FFT
  computeFFT();

  // 3) Selección adaptativa por energía
  selectedCount = selectByEnergy(ENERGY_THRESHOLD);

  // 4) Transmitir por UART
  transmitFrame(selected, selectedCount);

  // 5) Debug por monitor serie
  printSpectralSummary(selected, selectedCount);

  delay(200);
}

// ──────────────────────────────────────────────────────────────────
void computeFFT() {
  FFT.dcRemoval();
  FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(FFT_FORWARD);
  FFT.complexToMagnitude();
  // vReal[0..N/2-1] ahora contiene magnitudes
}

// ──────────────────────────────────────────────────────────────────
// Selecciona los bins necesarios para cubrir `threshold` de la energía total.
// Ordena por magnitud descendente (insertion sort, N pequeño → OK en MCU).
// Retorna cuántos bins se seleccionaron.
uint8_t selectByEnergy(double threshold) {
  const uint8_t N2 = SAMPLES / 2;

  // --- Copiar bins 1..N/2-1 a selected[] (ignoramos DC = bin 0)
  for (uint8_t i = 0; i < N2 - 1; i++) {
    selected[i].index = i + 1;
    selected[i].mag   = (float)vReal[i + 1];
  }
  uint8_t total = N2 - 1;

  // --- Calcular energía total (suma de magnitudes al cuadrado)
  double totalEnergy = 0.0;
  for (uint8_t i = 0; i < total; i++)
    totalEnergy += (double)selected[i].mag * selected[i].mag;

  // --- Insertion sort descendente por magnitud
  for (uint8_t i = 1; i < total; i++) {
    SpectralCoeff key = selected[i];
    int8_t j = i - 1;
    while (j >= 0 && selected[j].mag < key.mag) {
      selected[j + 1] = selected[j];
      j--;
    }
    selected[j + 1] = key;
  }

  // --- Acumular hasta cubrir el umbral
  double accumulated = 0.0;
  uint8_t k = 0;
  while (k < total) {
    accumulated += (double)selected[k].mag * selected[k].mag;
    k++;
    if (accumulated / totalEnergy >= threshold) break;
  }

  return k;  // número de bins seleccionados
}

// ──────────────────────────────────────────────────────────────────
// Formato del frame:
//  [0xAA][K][index_hi][index_lo][mag b3][mag b2][mag b1][mag b0] x K [checksum]
void transmitFrame(SpectralCoeff* coeffs, uint8_t count) {
  // Construir buffer
  // max size: 1 header + 1 count + 6*count + 1 checksum
  const uint16_t maxBuf = 2 + 6 * MAX_BINS + 1;
  uint8_t buf[maxBuf];
  uint16_t idx = 0;

  buf[idx++] = FRAME_HEADER;
  buf[idx++] = count;

  for (uint8_t i = 0; i < count; i++) {
    // bin index (2 bytes, big-endian)
    buf[idx++] = (uint8_t)(coeffs[i].index >> 8);
    buf[idx++] = (uint8_t)(coeffs[i].index & 0xFF);

    // magnitud (float 4 bytes, little-endian)
    uint8_t* fb = (uint8_t*)&coeffs[i].mag;
    buf[idx++] = fb[0];
    buf[idx++] = fb[1];
    buf[idx++] = fb[2];
    buf[idx++] = fb[3];
  }

  buf[idx++] = calcChecksum(buf, idx);

  Serial2.write(buf, idx);
}

// ──────────────────────────────────────────────────────────────────
// XOR de todos los bytes del buffer
uint8_t calcChecksum(uint8_t* data, uint16_t len) {
  uint8_t cs = 0;
  for (uint16_t i = 0; i < len; i++) cs ^= data[i];
  return cs;
}

// ──────────────────────────────────────────────────────────────────
void printSpectralSummary(SpectralCoeff* coeffs, uint8_t count) {
  Serial.printf("─── Frame: %u bins seleccionados (de %u) ───\n",
                count, SAMPLES / 2 - 1);
  for (uint8_t i = 0; i < count; i++) {
    double freqHz = ((double)coeffs[i].index * Fs) / SAMPLES;
    Serial.printf("  Bin %3u → %6.2f Hz | Mag: %.2f\n",
                  coeffs[i].index, freqHz, coeffs[i].mag);
  }
}