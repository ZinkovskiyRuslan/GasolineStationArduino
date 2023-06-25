
#include <GyverPower.h>         // просыпаемся по аппаратному прерыванию из sleepDelay
#include <LiquidCrystal_I2C.h>  // подключаем библиотеку дисплея
#include <EEPROMWearLevel.h>

#define INIT_ADDR 1023  // номер резервной ячейки для признака первого запуска 1 байт
#define INIT_KEY 42     // ключ первого запуска. 0-254, на выбор

#define EEPROM_LAYOUT_VERSION 0
#define AMOUNT_OF_INDEXES 1
#define INDEX_RING_BUFFER 0


const int pinAccess = 4;
const int pinStart = 5;
const int pinStop = 7;
const int pinPump = 9;
const int pinPowerModem = 10;

//const int pinTempSensor = 6;
//[[deprecated("use modem esp 32")]
const int pinStartModem = 11;

//[[deprecated("use bluetooth dut")]
const int pinPowerDut = 12;

const int pinPulse = 2;
const int pinVoltageControl = 3;
const int pulsePerLiter = 200;          // импульсов на 1 литр

uint32_t antiBounceDelayMicros = 100000;// интервал защиты от дребезга контактов
uint32_t tmrChangePulse = 0;            // момент срабатывания прерывания по импульсу micros()
uint32_t pulseCount = 0;                // счётчик импульсов от сенсора
uint32_t pulseDisplayCount = 0;         // счётчик импульсов выведенных на дисплей
unsigned long tmrStopButtonPress = 0;   // момент нажатия на кнопку остановки millis()

String statusPins = "";

LiquidCrystal_I2C lcd(0x27, 20, 4);  // адрес, столбцов, строк

void lcdPrint( bool forse = false );

void setup() {
  Serial.begin(115200);
  while (!Serial);
  EEPROMwl.begin(EEPROM_LAYOUT_VERSION, AMOUNT_OF_INDEXES, 1000);
  checkFirstStart();
  lcdInit();
  pinMode(pinAccess, INPUT_PULLUP);
  pinMode(pinStart, INPUT_PULLUP);
  pinMode(pinStop, INPUT_PULLUP);
  pinMode(pinPump, OUTPUT);
  pinMode(pinPowerDut, OUTPUT);
  pinMode(pinStartModem, OUTPUT);
  pinMode(pinPowerModem, OUTPUT);
  pinMode(pinPulse, INPUT_PULLUP);
  pinMode(pinVoltageControl, INPUT_PULLUP);
//  pinMode(pinTempSensor, OUTPUT);

  digitalWrite(pinPump, LOW);
  digitalWrite(pinPowerDut, LOW);
  digitalWrite(pinPowerModem, LOW);
  
  // подключаем прерывание на пин D2 (Arduino NANO)
  attachInterrupt(0, countPulse, CHANGE); // прерывание вызывается при смене значения на порту, с LOW на HIGH и наоборот

  // подключаем прерывание на пин D3 (Arduino NANO)
  attachInterrupt(1, isr, FALLING); // прерывание вызывается только при смене значения на порту с HIGH на LOW

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

void countPulse(){
 // if( micros() - tmrChangePulse > antiBounceDelayMicros)
  {
    pulseCount++;
    tmrChangePulse = micros();
  }
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
      //startModem();
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
      if(!digitalRead(pinPump)){
        pulseCount = 0;
        Serial.println("Starting");
      }
      digitalWrite(pinPump, HIGH);
      //Serial.println("start");
    }

    if (!digitalRead(pinStop) || digitalRead(pinAccess) || digitalRead(pinVoltageControl)) {
      if(digitalRead(pinPump)){ // остановка
        tmrStopButtonPress = millis();
        Serial.println("Stoping");
      }
      digitalWrite(pinPump, LOW);
      //Serial.println("stop");
    }

    if(tmrStopButtonPress > 0 && millis() - tmrStopButtonPress > 3000)
    {
      tmrStopButtonPress = 0;
      int liters = round((float)pulseCount/pulsePerLiter);
      uint32_t totalSumm;
      EEPROMwl.get(INDEX_RING_BUFFER, totalSumm);
      totalSumm = totalSumm + liters;
      EEPROMwl.putToNext(INDEX_RING_BUFFER, totalSumm);
      Serial.println("Save liters:" + String(liters) + " totalSumm:" + String(totalSumm));
      lcdInit();
    }

    lcdPrint();
    
    logIsChangedPinStatus();
  }
}

void logIsChangedPinStatus()
{
  String statusPinsCheck = 
      " pinAccess:" + String(digitalRead(pinAccess)) +
      " pinStart:" + String(digitalRead(pinStart)) +
      " pinStop:" + String(digitalRead(pinStop)) +
      " pinPump:" + String(digitalRead(pinPump)) +
      " pinPowerDut:" + String(digitalRead(pinPowerDut)) +
      " pinStartModem:" + String(digitalRead(pinStartModem)) +
      " pinPowerModem:" + String(digitalRead(pinPowerModem)) +
      " pinPulse:" + String(digitalRead(pinPulse)) +
      " pinVoltageControl:" + String(digitalRead(pinVoltageControl));

  if(statusPins != statusPinsCheck)
  {
    statusPins = statusPinsCheck;
    Serial.println(statusPins);
    Serial.println();
  }
}

void checkFirstStart()
{
  if (EEPROM.read(INIT_ADDR) != INIT_KEY) // первый запуск
  { 
    Serial.println("first start");
    EEPROM.write(INIT_ADDR, INIT_KEY);  // записали ключ
    EEPROMwl.putToNext(INDEX_RING_BUFFER, 0);
  }
}

void lcdInit()
{
  lcd.init();             // инициализация
  lcd.backlight();        // включить подсветку  
  lcd.setCursor(0, 1);    // столбец 0 строка 3
  lcd.print("Current: ");
  lcd.setCursor(0, 3);    // столбец 0 строка 3
  uint32_t totalSumm = 0; // Итоговая сумма
  EEPROMwl.get(INDEX_RING_BUFFER, totalSumm);
  lcd.print("Total summ: "  + String(totalSumm));
  lcdPrint(true);
}

void lcdPrint(bool forse)
{
  if(pulseCount - pulseDisplayCount >= pulsePerLiter/200 || forse)
  {
    pulseDisplayCount = pulseCount;
    lcd.setCursor(9, 1);
    float liters = (float)pulseCount/pulsePerLiter;
    lcd.print(String(liters,2));
    //Serial.println("pulseCount:" + String(pulseCount)+" liters:" + String(liters));
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
