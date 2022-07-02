#include <avr/wdt.h>
#include <avr/sleep.h>


volatile int sleepCnt = 0; //счётчик циклов в режиме сна

const int sleepPowerDut = 10; //Цикл после которого будет подан сигнал на включение питания ДУТа
const int sleepPowerOffDut = 20; //Цикл после которого будет подан сигнал на выключение питания ДУТа

const int sleepPowerModem = 10; //Цикл после которого будет подан сигнал на включение питания модема
const int sleepPowerOffModem = 19; //Цикл после которого будет подан сигнал на выключение питания модема

const int portAccess = 3;
const int portStart = 5;
const int portStop = 7;
const int portPump = 9;
const int powerModem= 10;
const int startModem = 11;
const int powerDut = 12;
const int voltageControl = 13;

int i = 0;


void setup() {
  Serial.begin(115200);
  pinMode(portAccess, INPUT_PULLUP);
  pinMode(portStart, INPUT_PULLUP);
  pinMode(portStop, INPUT_PULLUP);
  pinMode(portPump, OUTPUT);
  pinMode(powerDut, OUTPUT);
  pinMode(startModem, OUTPUT);
  pinMode(powerModem, OUTPUT);
  pinMode(voltageControl, INPUT_PULLUP);

  digitalWrite(portPump, LOW);
  digitalWrite(powerDut, LOW);
  digitalWrite(powerModem, LOW);
}

void loop() {
  delay(100);
  Serial.println(String(sleepCnt));  delay(100);
  Serial.println(String(sleepCnt));  delay(100);

  //Режим работы от АКБ
  if (digitalRead(voltageControl)) {
    digitalWrite(portPump, LOW);
    sleepCnt++;
    Serial.println("sleepCnt" + String(sleepCnt));
    sleep();
    //Dut
    if (sleepCnt > sleepPowerDut)
      digitalWrite(powerDut, HIGH);
    if (sleepCnt > sleepPowerOffDut)
    {
      digitalWrite(powerDut, LOW);
      sleepCnt = 0;
    }
    //Modem
    if (sleepCnt >= sleepPowerModem)
    {
      digitalWrite(powerModem, HIGH);
      Serial.println("powerModem" + String(powerModem));
      delay(3000);
      if (sleepCnt == sleepPowerModem)
      {
        digitalWrite(startModem, HIGH);
        Serial.println("startModem HIGH");
        delay(3000);
        digitalWrite(startModem, LOW);
        Serial.println("startModem LOW");
      }
    }
    if (sleepCnt > sleepPowerOffModem)
    {
      digitalWrite(powerModem, LOW);
    }

  }
  //Режим работы от сети
  else
  {
    digitalWrite(powerDut, HIGH);
    digitalWrite(powerModem, HIGH);
    sleepCnt = 0;
    if (!digitalRead(portAccess) && !digitalRead(portStart)) {
      digitalWrite(portPump, HIGH);
      Serial.println("start");
    }

    if (digitalRead(portStop) || digitalRead(portAccess) || digitalRead(voltageControl)) {
      digitalWrite(portPump, LOW);
      Serial.println("stop");
    }
    delay(100);
  }
}

void sleep()
{
  wdt_enable(WDTO_2S); //Задаем интервал сторожевого таймера (2с)
  WDTCSR |= (1 << WDIE); //Устанавливаем бит WDIE регистра WDTCSR для разрешения прерываний от сторожевого таймера
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); //Устанавливаем интересующий нас режим
  sleep_mode(); // Переводим МК в спящий режим
}


ISR (WDT_vect) {
  wdt_disable();
}
