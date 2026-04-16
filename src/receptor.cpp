#include <Arduino.h>

#define RXD2 16
#define TXD2 17

HardwareSerial MySerial(2);

const uint8_t  FRAME_HEADER = 0xAA;
const double   Fs           = 256.0;
const uint16_t N            = 128;
const uint32_t TIMEOUT_MS   = 100;  // timeout por byte

// ─── Estructura idéntica al transmisor ───────────────────────────
struct SpectralCoeff {
  uint16_t index;
  float    mag;
};

SpectralCoeff coeffs[64];  // máximo posible

// ─── Prototipos ──────────────────────────────────────────────────
bool     waitByte(uint8_t* out, uint32_t timeout_ms);
uint8_t  calcChecksum(uint8_t* data, uint16_t len);
void     processCoeffs(SpectralCoeff* c, uint8_t count);

// ─────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  MySerial.begin(115200, SERIAL_8N1, RXD2, TXD2);
  Serial.println("=== ESP32 Receptor listo ===");
}

// ─────────────────────────────────────────────────────────────────
void loop() {
  if (!MySerial.available()) return;

  // 1) Buscar header
  if (MySerial.read() != FRAME_HEADER) return;

  // 2) Leer K con timeout
  uint8_t K = 0;
  if (!waitByte(&K, TIMEOUT_MS)) {
    Serial.println("[ERR] Timeout leyendo K");
    return;
  }

  if (K == 0 || K > 63) {
    Serial.printf("[ERR] K inválido: %u\n", K);
    return;
  }

  // 3) Calcular tamaño del payload: K × 6 bytes + 1 checksum
  uint16_t payloadLen = (uint16_t)K * 6 + 1;
  uint8_t  raw[payloadLen];

  // Leer byte a byte con timeout
  for (uint16_t i = 0; i < payloadLen; i++) {
    if (!waitByte(&raw[i], TIMEOUT_MS)) {
      Serial.printf("[ERR] Timeout en byte %u\n", i);
      return;
    }
  }

  // 4) Verificar checksum
  // El checksum se calculó sobre [header, K, payload_sin_checksum]
  uint8_t headerBuf[2] = { FRAME_HEADER, K };
  uint8_t cs = calcChecksum(headerBuf, 2);
  cs ^= calcChecksum(raw, payloadLen - 1);  // excluye el byte de checksum

  if (cs != raw[payloadLen - 1]) {
    Serial.printf("[ERR] Checksum inválido. Esperado: 0x%02X | Recibido: 0x%02X\n",
                  cs, raw[payloadLen - 1]);
    return;
  }

  // 5) Deserializar coeficientes
  for (uint8_t i = 0; i < K; i++) {
    uint16_t offset    = i * 6;
    coeffs[i].index    = ((uint16_t)raw[offset] << 8) | raw[offset + 1];

    // Reconstruir float desde 4 bytes (little-endian)
    uint8_t fb[4] = { raw[offset+2], raw[offset+3], raw[offset+4], raw[offset+5] };
    memcpy(&coeffs[i].mag, fb, 4);
  }

  // 6) Procesar
  processCoeffs(coeffs, K);
}

// ─────────────────────────────────────────────────────────────────
// Espera un byte con timeout. Retorna false si se agota el tiempo.
bool waitByte(uint8_t* out, uint32_t timeout_ms) {
  uint32_t start = millis();
  while (!MySerial.available()) {
    if (millis() - start > timeout_ms) return false;
  }
  *out = (uint8_t)MySerial.read();
  return true;
}

// ─────────────────────────────────────────────────────────────────
uint8_t calcChecksum(uint8_t* data, uint16_t len) {
  uint8_t cs = 0;
  for (uint16_t i = 0; i < len; i++) cs ^= data[i];
  return cs;
}

// ─────────────────────────────────────────────────────────────────
void processCoeffs(SpectralCoeff* c, uint8_t count) {
  Serial.printf("─── Frame recibido: %u coeficientes ───\n", count);
  for (uint8_t i = 0; i < count; i++) {
    double freqHz = ((double)c[i].index * Fs) / N;
    Serial.printf("  Bin %3u → %6.2f Hz | Mag: %.2f\n",
                  c[i].index, freqHz, c[i].mag);
  }
}