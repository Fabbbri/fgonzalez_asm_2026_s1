const int POT = A0;

const int ENA = 5;   // PWM hacia ENA del L298N
const int IN1 = 7;
const int IN2 = 8;

void setMotor(int pwm) {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, pwm);
}

void stopMotor() {
  analogWrite(ENA, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}

void runStep(int pwmValue, float durationSeconds) {
  stopMotor();
  delay(1000);  // reposo antes del escalón

  unsigned long startTime = millis();

  // Aquí se aplica el escalón
  setMotor(pwmValue);

  while (true) {
    float t = (millis() - startTime) / 1000.0;

    int raw = analogRead(POT);
    float voltage = raw * (5.0 / 1023.0);

    Serial.print(t, 3);
    Serial.print(",");
    Serial.println(voltage, 3);

    if (t >= durationSeconds) {
      stopMotor();
      Serial.println("END");
      break;
    }

    delay(50);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(POT, INPUT);
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  stopMotor();
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    int commaIndex = cmd.indexOf(',');

    if (commaIndex > 0) {
      int pwmValue = cmd.substring(0, commaIndex).toInt();
      float durationSeconds = cmd.substring(commaIndex + 1).toFloat();

      pwmValue = constrain(pwmValue, 0, 255);

      runStep(pwmValue, durationSeconds);
    }
  }
}