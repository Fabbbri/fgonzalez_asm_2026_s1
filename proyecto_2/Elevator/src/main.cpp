#include <Arduino.h>

// ---------------- PINES ----------------

// Potenciometro: extremo a 3.3V, extremo a GND, centro a GPIO34.
const int potPin = 34;

// L298N canal A.
const int motorIn1 = 26;
const int motorIn2 = 27;
const int motorPwm = 25; // ENA sin jumper

// ---------------- PWM ----------------

const int pwmChannel = 0;
const int pwmFreq = 1000;
const int pwmResolution = 8; // 0 a 255

// ---------------- PISOS ----------------

const int numPisos = 5;
const int pisosADC[numPisos] = {
    20,   // Piso 1
    960,  // Piso 2
    1930, // Piso 3
    2640, // Piso 4
    3370  // Piso 5
};

int pisoActualObjetivo = 0;
int referencia = pisosADC[0];
bool movimientoAutomatico = false;

// Ajustes simples de movimiento.
const int toleranciaLlegada = 35;
const int errorParaIrLento = 220;
const int pwmRapido = 230;
const int pwmLento = 130;

unsigned long tiempoSerial = 0;

int leerPotenciometro() {
  int suma = 0;

  for (int i = 0; i < 8; i++) {
    suma += analogRead(potPin);
  }

  return suma / 8;
}

void detenerMotor() {
  digitalWrite(motorIn1, LOW);
  digitalWrite(motorIn2, LOW);
  ledcWrite(pwmChannel, 0);
}

void subirMotor(int pwm) {
  digitalWrite(motorIn1, HIGH);
  digitalWrite(motorIn2, LOW);
  ledcWrite(pwmChannel, pwm);
}

void bajarMotor(int pwm) {
  digitalWrite(motorIn1, LOW);
  digitalWrite(motorIn2, HIGH);
  ledcWrite(pwmChannel, pwm);
}

void imprimirAyuda() {
  Serial.println();
  Serial.println("=== Ascensor simple ===");
  Serial.println("1-5: ir al piso indicado");
  Serial.println("u: subir manual");
  Serial.println("d: bajar manual");
  Serial.println("s: detener");
  Serial.println("h: ayuda");
  Serial.println();
}

void seleccionarPiso(int piso) {
  pisoActualObjetivo = piso;
  referencia = pisosADC[piso];
  movimientoAutomatico = true;

  Serial.print("Objetivo: piso ");
  Serial.print(piso + 1);
  Serial.print(" | ADC ref: ");
  Serial.println(referencia);
}

void leerComandoSerial() {
  if (!Serial.available()) {
    return;
  }

  char comando = Serial.read();

  if (comando >= '1' && comando <= '5') {
    seleccionarPiso(comando - '1');
    return;
  }

  switch (comando) {
    case 'u':
    case 'U':
      movimientoAutomatico = false;
      subirMotor(pwmLento);
      Serial.println("Manual: subir");
      break;

    case 'd':
    case 'D':
      movimientoAutomatico = false;
      bajarMotor(pwmLento);
      Serial.println("Manual: bajar");
      break;

    case 's':
    case 'S':
      movimientoAutomatico = false;
      detenerMotor();
      Serial.println("Motor detenido");
      break;

    case 'h':
    case 'H':
      imprimirAyuda();
      break;
  }
}

void controlarAscensor(int posicion) {
  if (!movimientoAutomatico) {
    return;
  }

  int error = referencia - posicion;
  int errorAbs = abs(error);

  if (errorAbs <= toleranciaLlegada) {
    detenerMotor();
    movimientoAutomatico = false;

    Serial.print("Llegue al piso ");
    Serial.print(pisoActualObjetivo + 1);
    Serial.print(" | Posicion: ");
    Serial.println(posicion);
    return;
  }

  int pwm = (errorAbs < errorParaIrLento) ? pwmLento : pwmRapido;

  if (error > 0) {
    subirMotor(pwm);
  } else {
    bajarMotor(pwm);
  }
}

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetPinAttenuation(potPin, ADC_11db);

  pinMode(potPin, INPUT);
  pinMode(motorIn1, OUTPUT);
  pinMode(motorIn2, OUTPUT);

  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(motorPwm, pwmChannel);

  detenerMotor();
  imprimirAyuda();
}

void loop() {
  leerComandoSerial();

  int posicion = leerPotenciometro();
  controlarAscensor(posicion);

  if (millis() - tiempoSerial >= 300) {
    tiempoSerial = millis();

    Serial.print("Piso objetivo: ");
    Serial.print(pisoActualObjetivo + 1);
    Serial.print(" | Ref: ");
    Serial.print(referencia);
    Serial.print(" | Pot ADC: ");
    Serial.print(posicion);
    Serial.print(" | Error: ");
    Serial.print(referencia - posicion);
    Serial.print(" | Auto: ");
    Serial.println(movimientoAutomatico ? "SI" : "NO");
  }
}
