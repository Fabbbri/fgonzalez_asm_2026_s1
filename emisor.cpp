#include <Arduino.h>

#define TXD2 17
#define RXD2 16

HardwareSerial MySerial(2);

// Tamaño de datos (ejemplo FFT comprimida)
const int DATA_SIZE = 64;

uint8_t fft_data[DATA_SIZE];

void setup() {
    Serial.begin(115200);
    MySerial.begin(115200, SERIAL_8N1, RXD2, TXD2);

    Serial.println("ESP32 Emisor listo");
}

void loop() {
    // Simular datos FFT (0–255)
    for (int i = 0; i < DATA_SIZE; i++) {
        fft_data[i] = random(0, 256);
    }

    // ---- Protocolo ----
    MySerial.write(0xAA);                 // HEADER
    MySerial.write(DATA_SIZE);            // SIZE
    MySerial.write(fft_data, DATA_SIZE);  // DATA

    Serial.println("Datos enviados");

    delay(500);
}