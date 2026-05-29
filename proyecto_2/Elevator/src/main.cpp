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

const int numPisos = 5;

// Botones de pisos con INPUT_PULLUP: un lado al pin y el otro a GND.
const int botonesPiso[numPisos] = {2, 3, 4, 10, 11};
const unsigned long debounceBotonMs = 180;
unsigned long ultimoBotonMs[numPisos] = {0, 0, 0, 0, 0};
int estadoBotonAnterior[numPisos] = {HIGH, HIGH, HIGH, HIGH, HIGH};

// ---------------- PISOS ----------------

// Calibrar estos valores viendo la distancia que imprime el monitor serial.
// Si el sensor esta abajo, la distancia aumenta cuando el elevador sube.
const float pisosCm[numPisos] = {
    4.45,  // Piso 1
    14.18, // Piso 2
    24.05, // Piso 3
    36.20, // Piso 4
    43.50  // Piso 5
};

int pisoActualObjetivo = 0;
float referenciaCm = pisosCm[0];
bool movimientoAutomatico = false;

// Ajustes simples de movimiento.
const float toleranciaLlegadaCm = 0.6;
const float toleranciaLlegadaPiso1Cm = 1.2;
const float toleranciaCruceLlegadaCm = 1.2;
const int lecturasLlegadaNecesarias = 3;
const int pwmMin = 0;
const int pwmMax = 255;
const int pwmMinMovimiento = 75;
const float errorMinimoParaPwmMinCm = 2.0;
const int pasoVelocidad = 10;

// PID digital discreto. Se ejecuta cada intervaloPidMs como periodo de muestreo.
const float kp = 18.0;
const float ki = 0.10;
const float kd = 4.0;
const float integralLimite = 80.0;
const unsigned long intervaloPidMs = 60;

struct ResultadoPID {
  float p;
  float i;
  float d;
  float salida;
};

float integralError = 0.0;
float errorAnterior = 0.0;
unsigned long ultimoPid = 0;
bool pidInicializado = false;

// Esta velocidad se usa solo para movimiento manual con u/d.
int velocidadMotor = pwmMax;
int pwmActual = 0;
int lecturasLlegada = 0;
const char *direccionActual = "DETENIDO";

ResultadoPID ultimoPID = {0.0, 0.0, 0.0, 0.0};

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
  pwmActual = 0;
  direccionActual = "DETENIDO";
}

void subirMotor(int pwm) {
  pwmActual = constrain(pwm, pwmMin, pwmMax);
  direccionActual = "SUBIR";
  digitalWrite(motorIn1, HIGH);
  digitalWrite(motorIn2, LOW);
  analogWrite(motorPwm, pwmActual);
}

void bajarMotor(int pwm) {
  pwmActual = constrain(pwm, pwmMin, pwmMax);
  direccionActual = "BAJAR";
  digitalWrite(motorIn1, LOW);
  digitalWrite(motorIn2, HIGH);
  analogWrite(motorPwm, pwmActual);
}

void imprimirAyuda() {
  Serial.println();
  Serial.println("=== Ascensor simple Arduino + HC-SR04 ===");
  Serial.println("1-5: ir al piso indicado");
  Serial.println("Botones D2,D3,D4,D10,D11: pisos 1-5");
  Serial.println("u: subir manual");
  Serial.println("d: bajar manual");
  Serial.println("s: detener");
  Serial.println("+: subir velocidad manual");
  Serial.println("-: bajar velocidad manual");
  Serial.println("b: ver estado de botones");
  Serial.println("h: ayuda");
  Serial.println();
}

void imprimirVelocidad() {
  Serial.print("Velocidad PWM manual: ");
  Serial.print(velocidadMotor);
  Serial.println(" / 255");
}

void ajustarVelocidad(int cambio) {
  velocidadMotor = constrain(velocidadMotor + cambio, pwmMinMovimiento, pwmMax);
  imprimirVelocidad();
}

void reiniciarPID() {
  integralError = 0.0;
  errorAnterior = 0.0;
  ultimoPid = 0;
  lecturasLlegada = 0;
  pidInicializado = false;
  ultimoPID.p = 0.0;
  ultimoPID.i = 0.0;
  ultimoPID.d = 0.0;
  ultimoPID.salida = 0.0;
}

ResultadoPID calcularPID(float error, float dt) {
  ResultadoPID resultado;

  resultado.p = kp * error;
  resultado.d = kd * (error - errorAnterior) / dt;

  float integralTentativa = integralError + (error * dt);
  integralTentativa = constrain(integralTentativa, -integralLimite, integralLimite);

  float salidaTentativa = resultado.p + (ki * integralTentativa) + resultado.d;
  bool saturadoAlto = salidaTentativa > pwmMax;
  bool saturadoBajo = salidaTentativa < -pwmMax;
  bool errorAyudaSalir = (saturadoAlto && error < 0.0) || (saturadoBajo && error > 0.0);

  // Anti-windup: si la salida esta saturada, no seguimos cargando integral
  // salvo cuando el error ayuda a salir de esa saturacion.
  if ((!saturadoAlto && !saturadoBajo) || errorAyudaSalir) {
    integralError = integralTentativa;
  }

  integralError = constrain(integralError, -integralLimite, integralLimite);

  resultado.i = ki * integralError;
  resultado.salida = resultado.p + resultado.i + resultado.d;

  errorAnterior = error;
  ultimoPID = resultado;

  return resultado;
}

void imprimirEstadoBotones() {
  Serial.print("Botones: ");

  for (int i = 0; i < numPisos; i++) {
    Serial.print("P");
    Serial.print(i + 1);
    Serial.print("=");
    Serial.print(digitalRead(botonesPiso[i]) == LOW ? "PRESIONADO" : "suelto");

    if (i < numPisos - 1) {
      Serial.print(" | ");
    }
  }

  Serial.println();
}

void seleccionarPiso(int piso) {
  pisoActualObjetivo = piso;
  referenciaCm = pisosCm[piso];
  movimientoAutomatico = true;
  reiniciarPID();

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
      reiniciarPID();
      subirMotor(velocidadMotor);
      Serial.println("Manual: subir");
      break;

    case 'd':
    case 'D':
      movimientoAutomatico = false;
      reiniciarPID();
      bajarMotor(velocidadMotor);
      Serial.println("Manual: bajar");
      break;

    case 's':
    case 'S':
      movimientoAutomatico = false;
      reiniciarPID();
      detenerMotor();
      Serial.println("Motor detenido");
      break;

    case 'h':
    case 'H':
      imprimirAyuda();
      break;

    case 'b':
    case 'B':
      imprimirEstadoBotones();
      break;

    case '+':
      ajustarVelocidad(pasoVelocidad);
      break;

    case '-':
      ajustarVelocidad(-pasoVelocidad);
      break;
  }
}

void leerBotonesPisos() {
  unsigned long ahora = millis();

  for (int i = 0; i < numPisos; i++) {
    int estadoActual = digitalRead(botonesPiso[i]);
    bool botonPresionado = estadoBotonAnterior[i] == HIGH && estadoActual == LOW;

    if (botonPresionado && ahora - ultimoBotonMs[i] >= debounceBotonMs) {
      ultimoBotonMs[i] = ahora;
      seleccionarPiso(i);

      Serial.print("Boton: piso ");
      Serial.println(i + 1);
    }

    estadoBotonAnterior[i] = estadoActual;
  }
}

void confirmarLlegada(float distanciaCm) {
  detenerMotor();
  movimientoAutomatico = false;
  reiniciarPID();

  Serial.print("Llegue al piso ");
  Serial.print(pisoActualObjetivo + 1);
  Serial.print(" | Distancia: ");
  Serial.print(distanciaCm, 1);
  Serial.println(" cm");
}

void controlarAscensor(float distanciaCm) {
  if (!movimientoAutomatico) {
    return;
  }

  if (distanciaCm < 0.0) {
    detenerMotor();
    movimientoAutomatico = false;
    reiniciarPID();
    Serial.println("Sin lectura del HC-SR04. Motor detenido por seguridad.");
    return;
  }

  // Sensor abajo: la distancia aumenta al subir.
  // Error positivo => la referencia esta mas arriba => el motor debe subir.
  // Error negativo => la referencia esta mas abajo => el motor debe bajar.
  float error = referenciaCm - distanciaCm;
  float errorAbs = fabs(error);
  float toleranciaActualCm = (pisoActualObjetivo == 0) ? toleranciaLlegadaPiso1Cm : toleranciaLlegadaCm;
  bool cruzoReferencia = pidInicializado &&
                         ((errorAnterior < 0.0 && error > 0.0) ||
                          (errorAnterior > 0.0 && error < 0.0));

  if (errorAbs <= toleranciaActualCm) {
    detenerMotor();
    lecturasLlegada++;

    if (lecturasLlegada < lecturasLlegadaNecesarias) {
      return;
    }

    confirmarLlegada(distanciaCm);
    return;
  }

  if (cruzoReferencia && errorAbs <= toleranciaCruceLlegadaCm) {
    confirmarLlegada(distanciaCm);
    return;
  }

  lecturasLlegada = 0;

  unsigned long ahora = millis();

  if (!pidInicializado) {
    errorAnterior = error;
    ultimoPid = ahora;
    pidInicializado = true;
    return;
  }

  if (ultimoPid == 0) {
    ultimoPid = ahora;
  }

  if (ahora - ultimoPid < intervaloPidMs) {
    return;
  }

  float dt = (ahora - ultimoPid) / 1000.0;
  ultimoPid = ahora;

  ResultadoPID pid = calcularPID(error, dt);

  // La salida firmada del PID define direccion y magnitud del motor.
  float pwmCalculado = fabs(pid.salida);
  pwmCalculado = constrain(pwmCalculado, (float)pwmMin, (float)pwmMax);
  int pwm = (int)pwmCalculado;

  if (errorAbs > errorMinimoParaPwmMinCm && pwmCalculado > 0.0 && pwmCalculado < pwmMinMovimiento) {
    pwm = pwmMinMovimiento;
  }

  if (pid.salida > 0.0) {
    subirMotor(pwm);
  } else if (pid.salida < 0.0) {
    bajarMotor(pwm);
  } else {
    detenerMotor();
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(motorIn1, OUTPUT);
  pinMode(motorIn2, OUTPUT);
  pinMode(motorPwm, OUTPUT);

  for (int i = 0; i < numPisos; i++) {
    pinMode(botonesPiso[i], INPUT_PULLUP);
    estadoBotonAnterior[i] = digitalRead(botonesPiso[i]);
  }

  digitalWrite(trigPin, LOW);
  detenerMotor();
  imprimirAyuda();
}

void loop() {
  leerComandoSerial();
  leerBotonesPisos();

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
    Serial.print(" | P: ");
    Serial.print(ultimoPID.p, 2);
    Serial.print(" | I: ");
    Serial.print(ultimoPID.i, 2);
    Serial.print(" | D: ");
    Serial.print(ultimoPID.d, 2);
    Serial.print(" | Salida PID: ");
    Serial.print(ultimoPID.salida, 2);
    Serial.print(" | PWM aplicado: ");
    Serial.print(pwmActual);
    Serial.print(" | Direccion: ");
    Serial.println(direccionActual);
  }
}
