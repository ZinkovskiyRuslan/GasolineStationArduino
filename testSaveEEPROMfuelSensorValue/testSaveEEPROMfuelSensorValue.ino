#define INIT_ADDR 1023  // номер резервной ячейки для признака первого запуска 1 байт
#define INIT_KEY 42     // ключ первого запуска. 0-254, на выбор
#define CONF_ADDR 1021  // указатель на позицию конфига в памяти

#include <LiquidCrystal_I2C.h>  // подключаем библиотеку дисплея
#include <EEPROM.h>             // подключаем библиотеку ПЗУ

int32_t i = 1;
uint32_t tmr = micros();
uint32_t tmrDiff = 0;

struct Config {
  uint8_t writeCicleCount;  // счётчик сохраения структуры в текущую область памяти
  uint32_t fuel;            // значение накопленным итогом
};

Config config;

LiquidCrystal_I2C lcd(0x27, 20, 4);  // адрес, столбцов, строк

void setup() {
  Serial.begin(115000);
  checkFirstStart();

  uint16_t configPosition; // 2 байта отвечают за текущее расположение конфига в памяти
  EEPROM.get(CONF_ADDR, configPosition);
  config = getConfig();
  Serial.println(
    "fuel:" + String(config.fuel) + 
    ", writeCicleCount:" + String(config.writeCicleCount) +
    ", configPosition:"+String(configPosition)
    );

  lcd.init();           // инициализация
  lcd.backlight();      // включить подсветку  
  lcd.setCursor(1, 0);  // столбец 1 строка 0
  lcd.print("Hello, world!");
  lcd.setCursor(4, 1);  // столбец 4 строка 1
  lcd.print("GyverKIT");

  config.fuel = config.fuel + 100;
  save (config);
}
void loop() {
  i = abs(i*2)+1;
  //i =1
  tmr = micros();
  lcd.setCursor(1, 1);
  lcd.print(String("888 Liters"));
  Serial.println(String(micros()-tmr) );
}

void checkFirstStart()
{
  if (EEPROM.read(INIT_ADDR) != INIT_KEY) // первый запуск
  { 
    EEPROM.write(INIT_ADDR, INIT_KEY);  // записали ключ
    EEPROM.write(CONF_ADDR, 0);         // инициализация позиции конфигурации в памяти first byte
    EEPROM.write(CONF_ADDR + 1, 0);     // инициализация позиции конфигурации в памяти second byte
    EEPROM.put(0, config);              // сохранение конфигурации
  }
}

Config getConfig()
{
  uint16_t configPosition; // 2 байта отвечают за текущее расположение конфига в памяти
  EEPROM.get(CONF_ADDR, configPosition);
  return EEPROM.get(configPosition, config);
}

void save(Config config)
{
  config.writeCicleCount++;
  uint16_t configPosition; // 2 байта отвечают за текущее расположение конфига в памяти
  EEPROM.get(CONF_ADDR, configPosition);
  if(config.writeCicleCount >= 255)
  {
    config.writeCicleCount = 0;
    configPosition++;
    if(configPosition >= 1000)
    {
      configPosition = 0;
    }
    EEPROM.put(CONF_ADDR, configPosition);
  }
  EEPROM.put(configPosition, config);
}