#include <Arduino.h>

// ---------------- PINES ----------------

// Potenciómetro de posición.
// En el ESP32 es mejor usar pines ADC como GPIO34, 35, 36 o 39.
const int potPin = 34;

// Pines del puente H para controlar dirección del motor.
const int motorIn1 = 26;
const int motorIn2 = 27;

// Pin PWM para controlar velocidad del motor.
const int motorPwm = 25;

// Botones para seleccionar los pisos.
// Se conectan a GND y se usan con INPUT_PULLUP.
const int numPisos = 5;
const int botones[numPisos] = {14, 12, 13, 32, 33};

// ---------------- PWM ----------------

const int pwmChannel = 0;
const int pwmFreq = 20000;
const int pwmResolution = 8;   // 8 bits: valores de 0 a 255

// ---------------- MUESTREO ----------------

// Se usa fs = 80 Hz, entonces Ts = 12.5 ms.
const unsigned long Ts_us = 12500;
unsigned long tiempoAnterior = 0;

// ---------------- REFERENCIAS DE PISOS ----------------

// Valores de ejemplo del ADC para cada piso.
// Estos se deben cambiar cuando midan el potenciómetro real.
int pisosADC[numPisos] = {
  500,    // Piso 1
  1200,   // Piso 2
  1900,   // Piso 3
  2700,   // Piso 4
  3500    // Piso 5
};

int pisoDeseado = 0;
int referencia = pisosADC[0];

// ---------------- PID ----------------

// Valores iniciales. Luego se ajustan probando la planta real.
float Kp = 0.35;
float Ki = 0.02;
float Kd = 0.08;

float errorAnterior = 0.0;
float integral = 0.0;

// Límites para evitar valores demasiado grandes.
const int pwmMax = 255;
const int pwmMin = 70;
const int zonaMuerta = 35;

unsigned long tiempoSerial = 0;

// Lee el potenciómetro y promedia varias muestras para reducir ruido.
int leerPotenciometro() {
  int suma = 0;

  for (int i = 0; i < 8; i++) {
    suma += analogRead(potPin);
  }

  return suma / 8;
}

// Revisa si se presionó algún botón de piso.
void leerBotones() {
  for (int i = 0; i < numPisos; i++) {
    if (digitalRead(botones[i]) == LOW) {
      pisoDeseado = i;
      referencia = pisosADC[i];
    }
  }
}

// Detiene el motor.
void detenerMotor() {
  digitalWrite(motorIn1, LOW);
  digitalWrite(motorIn2, LOW);
  ledcWrite(pwmChannel, 0);
}

// Mueve el motor según el signo de la salida PID.
void moverMotor(float salida) {
  int pwm = abs((int)salida);

  if (pwm > pwmMax) {
    pwm = pwmMax;
  }

  // PWM mínimo para vencer un poco la fricción del motor.
  if (pwm > 0 && pwm < pwmMin) {
    pwm = pwmMin;
  }

  if (salida > 0) {
    // Subir
    digitalWrite(motorIn1, HIGH);
    digitalWrite(motorIn2, LOW);
    ledcWrite(pwmChannel, pwm);
  } 
  else if (salida < 0) {
    // Bajar
    digitalWrite(motorIn1, LOW);
    digitalWrite(motorIn2, HIGH);
    ledcWrite(pwmChannel, pwm);
  } 
  else {
    detenerMotor();
  }
}

// Calcula la salida del PID discreto.
float calcularPID(int posicion, float Ts) {
  float error = referencia - posicion;

  // Si está cerca del piso deseado, se detiene.
  if (abs(error) <= zonaMuerta) {
    integral = 0.0;
    errorAnterior = error;
    return 0.0;
  }

  integral += error * Ts;

  // Anti-windup simple para que la integral no crezca demasiado.
  if (integral > 3000) {
    integral = 3000;
  } 
  else if (integral < -3000) {
    integral = -3000;
  }

  float derivada = (error - errorAnterior) / Ts;

  float salida = Kp * error + Ki * integral + Kd * derivada;

  errorAnterior = error;

  if (salida > pwmMax) {
    salida = pwmMax;
  } 
  else if (salida < -pwmMax) {
    salida = -pwmMax;
  }

  return salida;
}

void setup() {
  Serial.begin(115200);

  pinMode(potPin, INPUT);

  pinMode(motorIn1, OUTPUT);
  pinMode(motorIn2, OUTPUT);

  for (int i = 0; i < numPisos; i++) {
    pinMode(botones[i], INPUT_PULLUP);
  }

  // Configuración PWM del ESP32.
  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(motorPwm, pwmChannel);

  detenerMotor();

  tiempoAnterior = micros();

  Serial.println("Control PID del ascensor iniciado");
}

void loop() {
  // Los botones se revisan todo el tiempo, sin bloquear el programa.
  leerBotones();

  unsigned long tiempoActual = micros();

  // El PID solo se calcula cuando ya pasó el periodo de muestreo.
  if (tiempoActual - tiempoAnterior >= Ts_us) {
    float Ts = (tiempoActual - tiempoAnterior) / 1000000.0;
    tiempoAnterior = tiempoActual;

    int posicion = leerPotenciometro();
    float salidaPID = calcularPID(posicion, Ts);

    moverMotor(salidaPID);

    // Mensajes para revisar valores en el monitor serial.
    // Tampoco se usa delay, solo se revisa el tiempo con millis().
    if (millis() - tiempoSerial >= 200) {
      tiempoSerial = millis();

      Serial.print("Piso deseado: ");
      Serial.print(pisoDeseado + 1);

      Serial.print(" | Ref: ");
      Serial.print(referencia);

      Serial.print(" | Posicion: ");
      Serial.print(posicion);

      Serial.print(" | Error: ");
      Serial.print(referencia - posicion);

      Serial.print(" | Salida PID: ");
      Serial.println(salidaPID);
    }
  }
}