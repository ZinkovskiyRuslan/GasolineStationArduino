// просыпаемся по аппаратному прерыванию из sleepDelay
#include <GyverPower.h>

const int pinAccess = 2;
const int pinStart = 5;
const int pinStop = 7;
const int pinPump = 9;
const int pinPowerModem = 10;
const int pinStartModem = 11;
const int pinPowerDut = 12;
const int pinVoltageControl = 3;

void setup() {
  Serial.begin(115200);
  pinMode(pinAccess, INPUT_PULLUP);
  pinMode(pinStart, INPUT_PULLUP);
  pinMode(pinStop, INPUT_PULLUP);
  pinMode(pinPump, OUTPUT);
  pinMode(pinPowerDut, OUTPUT);
  pinMode(pinStartModem, OUTPUT);
  pinMode(pinPowerModem, OUTPUT);
  pinMode(pinVoltageControl, INPUT_PULLUP);

  digitalWrite(pinPump, LOW);
  digitalWrite(pinPowerDut, LOW);
  digitalWrite(pinPowerModem, LOW);

  // подключаем прерывание на пин D3 (Arduino NANO)
  attachInterrupt(1, isr, FALLING);

  // глубокий сон
  power.setSleepMode(POWERDOWN_SLEEP);
  if (!digitalRead(pinVoltageControl)) {
    startModem();
  }
}

// обработчик аппаратного прерывания
void isr() {
  // дёргаем за функцию "проснуться"
  // без неё проснёмся чуть позже (через 0-8 секунд)
  power.wakeUp();
}
void loop() {
  //Режим работы от АКБ
  if (digitalRead(pinVoltageControl)) {
    digitalWrite(pinPump, LOW);
    goSleep(6); //2 min
    if (digitalRead(pinVoltageControl)) {
      digitalWrite(pinPowerModem, LOW);
      digitalWrite(pinPowerDut, LOW);
      goSleep(49); //55 min
      if (digitalRead(pinVoltageControl)) {
        //start Dut, ESP32-2
        digitalWrite(pinPowerDut, HIGH);
        goSleep(4); //4 min
      }
      //Modem
      startModem();
      if (digitalRead(pinVoltageControl)) {
        goSleep(2); //2 min
        if (digitalRead(pinVoltageControl)) {
          digitalWrite(pinPowerModem, LOW);
          digitalWrite(pinPowerDut, LOW);
        }
      }
    }
  }
  //Режим работы от сети
  else
  {
    digitalWrite(pinPowerDut, HIGH);
    digitalWrite(pinPowerModem, HIGH);

    if (!digitalRead(pinAccess) && !digitalRead(pinStart)) {
      digitalWrite(pinPump, HIGH);
      Serial.println("start");
    }

    if (digitalRead(pinStop) || digitalRead(pinAccess) || digitalRead(pinVoltageControl)) {
      digitalWrite(pinPump, LOW);
      Serial.println("stop");
    }
    delay(100);
  }
}

void goSleep(uint32_t period)
{
  Serial.println("go sleep " + String(period) + " min");
  period = period * 1000 * 60;
  delay(300);

  // правильно будет вот тут включать прерывание
  attachInterrupt(1, isr, FALLING);

  // спим 12 секунд, но можем проснуться по кнопке
  power.sleepDelay(period);
  // тут проснулись по кнопке или через указанный период

  // а вот тут сразу отключать
  detachInterrupt(1);

  Serial.println("wake up!");
  //delay(300);
}

void startModem()
{
  digitalWrite(pinPowerModem, LOW);
  delay(500);
  digitalWrite(pinPowerModem, HIGH);
  Serial.println("pinPowerModem" + String(pinPowerModem));
  delay(100);
  digitalWrite(pinStartModem, HIGH);
  Serial.println("pinStartModem HIGH");
  delay(3000);
  digitalWrite(pinStartModem, LOW);
  Serial.println("pinStartModem LOW");
}
