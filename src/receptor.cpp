#include <Arduino.h>
#include <arduinoFFT.h>
#include <math.h>

#define RXD2 16
#define TXD2 17

// ================= CONFIG =================
const uint8_t  FRAME_HEADER = 0xAA;
const double   Fs           = 8000.0;
const uint16_t N            = 128;
const uint32_t TIMEOUT_MS   = 100;
const uint32_t UART_BAUD    = 460800;
const int      dacPin       = 25;

HardwareSerial MySerial(2);

// =========================================================
//  DOBLE BUFFER + SINCRONIZACIÓN ENTRE NÚCLEOS
//  Core 1 (loop): recibe UART -> IFFT -> llena buffer inactivo
//  Core 0 (task): reproduce buffer activo continuamente
//
//  Mientras Core 0 reproduce el buffer A, Core 1 llena B.
//  Cuando B está listo, se hace swap y se repite.
//  Arregla clicks
// =========================================================
static uint8_t  bufA[N];
static uint8_t  bufB[N];
static uint8_t* playBuf = bufA;   // Core 0 lee de acá
static uint8_t* fillBuf = bufB;   // Core 1 escribe acá
static volatile bool bufferReady = false;   // Core 1 avisa que fillBuf está listo
static volatile bool swapRequest = false;   // Core 0 pide swap cuando termina el frame

// Proteger el swap de punteros
static SemaphoreHandle_t swapMutex;

// =========================================================
//  ESTADO IFFT
// =========================================================
static double ifftReal[N];
static double ifftImag[N];
ArduinoFFT<double> IFFT(ifftReal, ifftImag, N, Fs);

struct SpectralCoeff {
  uint16_t index;
  float    mag;
  float    phase;
};
static SpectralCoeff coeffs[64];

// =========================================================
//  TAREA DE REPRODUCCIÓN — Core 0
//  Reproduce playBuf en loop. Al terminar cada frame de N
//  muestras, si bufferReady==true hace el swap de punteros.
// =========================================================
static uint8_t lastSample = 128;
static const uint8_t FADE = 32;  

void taskPlay(void* pvParameters) {
  const uint32_t period_us = (uint32_t)llround(1e6 / Fs);

  // Esperar a que llegue el primer frame
  while (!bufferReady) {
    dacWrite(dacPin, 128);
    delayMicroseconds(period_us);
  }

  for (;;) {
    // Hacer swap si Core 1 preparó un nuevo buffer
    if (bufferReady) {
      if (xSemaphoreTake(swapMutex, 0) == pdTRUE) {
        uint8_t* tmp = playBuf;
        playBuf     = fillBuf;
        fillBuf     = tmp;
        bufferReady = false;
        xSemaphoreGive(swapMutex);
      }
    }

    // Reproducir el frame completo sample a sample
    uint32_t t = micros();
    for (uint16_t i = 0; i < N; i++) {
      uint8_t sample = playBuf[i];

      // Crossfader
      if (i < FADE) {
        float alpha = (float)i / (float)FADE;
        sample = (uint8_t)(lastSample * (1.0f - alpha) + playBuf[i] * alpha);
      }

      dacWrite(dacPin, sample);

      uint32_t elapsed = (uint32_t)(micros() - t);
      if (elapsed >= period_us) {
        t = micros();
      } else {
        while ((uint32_t)(micros() - t) < period_us) {}
        t += period_us;
      }
    }
    lastSample = playBuf[N - 1];
  }
}

// =========================================================
//  FUNCIONES DE RECEPCIÓN Y PROCESAMIENTO
// =========================================================
bool waitByte(uint8_t* out, uint32_t timeout_ms) {
  uint32_t start = millis();
  while (!MySerial.available()) {
    if (millis() - start > timeout_ms) return false;
  }
  *out = (uint8_t)MySerial.read();
  return true;
}

uint8_t calcChecksum(uint8_t* data, uint16_t len) {
  uint8_t cs = 0;
  for (uint16_t i = 0; i < len; i++) cs ^= data[i];
  return cs;
}

void reconstructAndIFFT(SpectralCoeff* c, uint8_t count) {
  for (uint16_t i = 0; i < N; i++) {
    ifftReal[i] = 0.0;
    ifftImag[i] = 0.0;
  }

  for (uint8_t i = 0; i < count; i++) {
    const uint16_t k = c[i].index;
    if (k == 0 || k >= (N / 2)) continue;

    const double mag = (double)c[i].mag;
    const double ph  = (double)c[i].phase;

    ifftReal[k] = mag * cos(ph);
    ifftImag[k] = mag * sin(ph);

    const uint16_t k2 = (uint16_t)(N - k);
    ifftReal[k2] =  ifftReal[k];
    ifftImag[k2] = -ifftImag[k];
  }

  IFFT.compute(FFT_REVERSE);

  for (uint16_t i = 0; i < N; i++) {
    ifftReal[i] /= (double)N;
  }
}

// Escribe el resultado de la IFFT en el fillBuf
void prepareFillBuffer() {
  double maxAbs = 1.0;
  for (uint16_t i = 0; i < N; i++) {
    double a = fabs(ifftReal[i]);
    if (a > maxAbs) maxAbs = a;
  }

  const double gain = 120.0 / maxAbs;

  // Escribir en fillBuf 
  for (uint16_t i = 0; i < N; i++) {
    int s = (int)lround(128.0 + ifftReal[i] * gain);
    if (s < 0)   s = 0;
    if (s > 255) s = 255;
    fillBuf[i] = (uint8_t)s;
  }
}

void processCoeffs(SpectralCoeff* c, uint8_t count) {
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint < 500) return;
  lastPrint = millis();

  Serial.printf("Frame recibido: %u coeficientes\n", count);
  for (uint8_t i = 0; i < count; i++) {
    double freqHz = ((double)c[i].index * Fs) / N;
    Serial.printf("  Bin %2u -> %6.1f Hz | Mag %.2f | Phase %.3f\n",
                  c[i].index, freqHz, c[i].mag, c[i].phase);
  }
}

// =========================================================
//  SETUP — lanza la tarea de reproducción en Core 0
// =========================================================
void setup() {
  Serial.begin(115200);

  // El task de audio (Core 0) hace espera activa a microsegundos para mantener Fs.
  // Eso puede impedir que corra IDLE0 y disparar el Task Watchdog.
  // Solución simple: desactivar WDT del Core 0.
  disableCore0WDT();

  MySerial.begin(UART_BAUD, SERIAL_8N1, RXD2, TXD2);
  dacWrite(dacPin, 128);

  // Inicializar buffers con silencio
  memset(bufA, 128, N);
  memset(bufB, 128, N);

  swapMutex = xSemaphoreCreateMutex();

  // Lanzar tarea de audio en Core 0
  // Core 1 es el que corre loop() por defecto
  xTaskCreatePinnedToCore(
    taskPlay,    // función
    "taskPlay",  // nombre
    4096,        // stack (bytes)
    NULL,        // parámetro
    24,          // prioridad (alta para no perder muestras)
    NULL,        // handle
    0            // Core 0
  );

  Serial.println("=== RECEPTOR IFFT + DAC listo (dual core) ===");
  Serial.printf("Fs=%.1f Hz | N=%u | UART=%lu\n", Fs, N, UART_BAUD);
}

// =========================================================
//  LOOP — Core 1: recepción y procesamiento
// =========================================================
void loop() {
  if (!MySerial.available()) return;

  if (MySerial.read() != FRAME_HEADER) return;

  uint8_t K = 0;
  if (!waitByte(&K, TIMEOUT_MS)) {
    Serial.println("[ERR] Timeout leyendo K");
    return;
  }

  if (K == 0 || K > 63) {
    Serial.printf("[ERR] K invalido: %u\n", K);
    return;
  }

  // Frame v2:
  // totalEnergy (4) + K*(10) + origSamples (N) + checksum (1)
  uint16_t payloadLen = 4 + (uint16_t)K * 10 + N + 1;
  static uint8_t raw[4 + 63 * 10 + N + 1];

  for (uint16_t i = 0; i < payloadLen; i++) {
    if (!waitByte(&raw[i], TIMEOUT_MS)) {
      Serial.printf("[ERR] Timeout en byte %u\n", i);
      return;
    }
  }

  // Verificar checksum
  uint8_t headerBuf[2] = { FRAME_HEADER, K };
  uint8_t cs = calcChecksum(headerBuf, 2);
  cs ^= calcChecksum(raw, payloadLen - 1);

  if (cs != raw[payloadLen - 1]) {
    Serial.printf("[ERR] Checksum invalido. Esperado 0x%02X, recibido 0x%02X\n",
                  cs, raw[payloadLen - 1]);
    return;
  }

  // Deserializar
  for (uint8_t i = 0; i < K; i++) {
    uint16_t offset = 4 + (uint16_t)i * 10; // saltar totalEnergy
    coeffs[i].index = ((uint16_t)raw[offset] << 8) | raw[offset + 1];
    memcpy(&coeffs[i].mag,   &raw[offset + 2], 4);
    memcpy(&coeffs[i].phase, &raw[offset + 6], 4);
  }

  processCoeffs(coeffs, K);
  reconstructAndIFFT(coeffs, K);

  // Esperar a que Core 0 haya consumido el buffer antes de escribir
  // (bufferReady == false significa que el swap ya ocurrió)
  uint32_t waitStart = millis();
  while (bufferReady && (millis() - waitStart < 50)) {
    vTaskDelay(1);
  }

  prepareFillBuffer();

  // Avisar a Core 0 que el nuevo buffer está listo
  bufferReady = true;
}