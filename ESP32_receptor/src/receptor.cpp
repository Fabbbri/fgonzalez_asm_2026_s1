#include <Arduino.h>

#define TXD2 17
#define RXD2 16

HardwareSerial MySerial(2);

const int DATA_SIZE = 64;

uint8_t buffer[DATA_SIZE];

void setup() {
    Serial.begin(115200);
    MySerial.begin(115200, SERIAL_8N1, RXD2, TXD2);

    Serial.println("ESP32 Receptor listo");
}

void loop() {
    if (MySerial.available()) {
        
        // Buscar HEADER
        if (MySerial.read() == 0xAA) {

            while (!MySerial.available());
            int size = MySerial.read();

            if (size == DATA_SIZE) {

                int received = MySerial.readBytes(buffer, DATA_SIZE);

                if (received == DATA_SIZE) {
                    Serial.println("Paquete recibido:");

                    for (int i = 0; i < DATA_SIZE; i++) {
                        Serial.print(buffer[i]);
                        Serial.print(" ");
                    }
                    Serial.println();
                }
            }
        }
    }
}