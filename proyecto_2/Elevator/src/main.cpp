#include <Arduino.h>
#include <math.h>

// ---------------- PINES ARDUINO UNO / NANO ----------------

// HC-SR04 inferior: sensor abajo mirando hacia la cabina.
const int trigPin = 5;
const int echoPin = 6;

// L298N canal A.
const int motorIn1 = 7;
const int motorIn2 = 8;
const int motorPwm = 9; // ENA sin jumper, pin PWM

// ---------------- PISOS ----------------

const int numPisos = 5;

// Calibrar estos valores viendo la distancia que imprime el monitor serial.
// Si el sensor esta abajo, la distancia aumenta cuando el elevador sube.
const float pisosCm[numPisos] = {
    4.0,  // Piso 1
    12.0, // Piso 2
    20.0, // Piso 3
    28.0, // Piso 4
    36.0  // Piso 5
};

int pisoActualObjetivo = 0;
float referenciaCm = pisosCm[0];
bool movimientoAutomatico = false;

// Ajustes simples de movimiento.
const float toleranciaLlegadaCm = 1.5;
const float errorParaIrLentoCm = 5.0;
const int pwmMin = 0;
const int pwmMax = 255;
const int pasoVelocidad = 10;
const int aumentoRapido = 80;

int velocidadMotor = 130;

unsigned long tiempoSerial = 0;

float medirDistanciaUnaVezCm() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duracion = pulseIn(echoPin, HIGH, 30000);

  if (duracion == 0) {
    return -1.0;
  }

  return duracion * 0.0343 / 2.0;
}

float leerDistanciaCm() {
  float suma = 0.0;
  int lecturasValidas = 0;

  for (int i = 0; i < 5; i++) {
    float distancia = medirDistanciaUnaVezCm();

    if (distancia > 0.0) {
      suma += distancia;
      lecturasValidas++;
    }

    delay(8);
  }

  if (lecturasValidas == 0) {
    return -1.0;
  }

  return suma / lecturasValidas;
}

void detenerMotor() {
  digitalWrite(motorIn1, LOW);
  digitalWrite(motorIn2, LOW);
  analogWrite(motorPwm, 0);
}

void subirMotor(int pwm) {
  digitalWrite(motorIn1, HIGH);
  digitalWrite(motorIn2, LOW);
  analogWrite(motorPwm, constrain(pwm, pwmMin, pwmMax));
}

void bajarMotor(int pwm) {
  digitalWrite(motorIn1, LOW);
  digitalWrite(motorIn2, HIGH);
  analogWrite(motorPwm, constrain(pwm, pwmMin, pwmMax));
}

void imprimirAyuda() {
  Serial.println();
  Serial.println("=== Ascensor simple Arduino + HC-SR04 ===");
  Serial.println("1-5: ir al piso indicado");
  Serial.println("u: subir manual");
  Serial.println("d: bajar manual");
  Serial.println("s: detener");
  Serial.println("+: subir velocidad");
  Serial.println("-: bajar velocidad");
  Serial.println("h: ayuda");
  Serial.println();
}

void imprimirVelocidad() {
  Serial.print("Velocidad PWM: ");
  Serial.print(velocidadMotor);
  Serial.println(" / 255");
}

void ajustarVelocidad(int cambio) {
  velocidadMotor = constrain(velocidadMotor + cambio, pwmMin, pwmMax);
  imprimirVelocidad();
}

void seleccionarPiso(int piso) {
  pisoActualObjetivo = piso;
  referenciaCm = pisosCm[piso];
  movimientoAutomatico = true;

  Serial.print("Objetivo: piso ");
  Serial.print(piso + 1);
  Serial.print(" | Ref: ");
  Serial.print(referenciaCm, 1);
  Serial.println(" cm");
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
      subirMotor(velocidadMotor);
      Serial.println("Manual: subir");
      break;

    case 'd':
    case 'D':
      movimientoAutomatico = false;
      bajarMotor(velocidadMotor);
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

    case '+':
      ajustarVelocidad(pasoVelocidad);
      break;

    case '-':
      ajustarVelocidad(-pasoVelocidad);
      break;
  }
}

void controlarAscensor(float distanciaCm) {
  if (!movimientoAutomatico) {
    return;
  }

  if (distanciaCm < 0.0) {
    detenerMotor();
    movimientoAutomatico = false;
    Serial.println("Sin lectura del HC-SR04. Motor detenido por seguridad.");
    return;
  }

  float error = referenciaCm - distanciaCm;
  float errorAbs = fabs(error);

  if (errorAbs <= toleranciaLlegadaCm) {
    detenerMotor();
    movimientoAutomatico = false;

    Serial.print("Llegue al piso ");
    Serial.print(pisoActualObjetivo + 1);
    Serial.print(" | Distancia: ");
    Serial.print(distanciaCm, 1);
    Serial.println(" cm");
    return;
  }

  int pwm = velocidadMotor;

  if (errorAbs >= errorParaIrLentoCm) {
    pwm = constrain(velocidadMotor + aumentoRapido, pwmMin, pwmMax);
  }

  if (error > 0.0) {
    subirMotor(pwm);
  } else {
    bajarMotor(pwm);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(motorIn1, OUTPUT);
  pinMode(motorIn2, OUTPUT);
  pinMode(motorPwm, OUTPUT);

  digitalWrite(trigPin, LOW);
  detenerMotor();
  imprimirAyuda();
}

void loop() {
  leerComandoSerial();

  float distanciaCm = leerDistanciaCm();
  controlarAscensor(distanciaCm);

  if (millis() - tiempoSerial >= 300) {
    tiempoSerial = millis();

    Serial.print("Piso objetivo: ");
    Serial.print(pisoActualObjetivo + 1);
    Serial.print(" | Ref: ");
    Serial.print(referenciaCm, 1);
    Serial.print(" cm | Distancia: ");

    if (distanciaCm < 0.0) {
      Serial.print("sin lectura");
    } else {
      Serial.print(distanciaCm, 1);
      Serial.print(" cm");
    }

    Serial.print(" | Error: ");

    if (distanciaCm < 0.0) {
      Serial.print("N/A");
    } else {
      Serial.print(referenciaCm - distanciaCm, 1);
      Serial.print(" cm");
    }

    Serial.print(" | Auto: ");
    Serial.print(movimientoAutomatico ? "SI" : "NO");
    Serial.print(" | PWM: ");
    Serial.println(velocidadMotor);
  }
}
