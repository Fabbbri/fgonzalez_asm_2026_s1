const int POT = A0;

const int IN1 = 7;
const int IN2 = 8;

unsigned long startTime;

void setMotor() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
}

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}

void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  stopMotor();

  // Tiempo inicial en reposo antes de aplicar el escalón
  delay(2000);

  // Aquí inicia la prueba
  startTime = millis();

  // Escalón: motor apagado -> motor encendido
  setMotor();
}

void loop() {
  float t = (millis() - startTime) / 1000.0;

  int raw = analogRead(POT);
  float voltage = raw * (5.0 / 1023.0);

  Serial.print(t, 3);
  Serial.print(",");
  Serial.println(voltage, 3);

  // Duración de la prueba
  if (t >= 3.0) {
    stopMotor();
    while (true);
  }

  delay(50);
}