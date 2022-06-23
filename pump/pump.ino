const int portAccess = 3;
const int portStart = 5;
const int portStop = 7;
bool pumpStatus = false;
const int portPump = 9;

void setup() {
  Serial.begin(115200);
  pinMode(portAccess, INPUT_PULLUP);
  pinMode(portStart, INPUT_PULLUP);
  pinMode(portStop, INPUT_PULLUP);
  pinMode(portPump, OUTPUT);
}

void loop() {
  if (!digitalRead(portAccess)){
    if (!digitalRead(portStart)){
      pumpStatus = true;    
      digitalWrite(portPump, HIGH);
      Serial.println("start");
    }
  }
  if (digitalRead(portStop) || digitalRead(portAccess)){
    pumpStatus = true;
    digitalWrite(portPump, LOW);
    Serial.println("storp");
  }
  delay(100);
}
