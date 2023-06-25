//To Do use <EEPROMWearLevel>
#define INIT_ADDR 1023      // addres attribute first start ref in memory 1 байт
#define INIT_KEY 42         // key first start value. 0-254 any
#define CONF_ADDR 1021      // addres position start config in memory 1021-1022

#include <EEPROM.h>

struct Config {
  uint8_t writeCicleCount;  // cout save config current position in memory
  uint32_t data;            // useful data
};

Config config;

void setup() {
  Serial.begin(115000);
  checkFirstStart();
  uint16_t configPosition; // 2 bytes
  EEPROM.get(CONF_ADDR, configPosition);
  config = getConfig();
  Serial.println(
    "data:" + String(config.data) + 
    ", writeCicleCount:" + String(config.writeCicleCount) +
    ", configPosition:"+String(configPosition)
    );
  config.data = config.data + 5;
  save (config);
}

void(* resetFunc) (void) = 0; //declare reset function @ address 0

void loop() {
  delay(100);
  resetFunc();  //call reset
}

void checkFirstStart()
{
  if (EEPROM.read(INIT_ADDR) != INIT_KEY) // is first start
  { 
    EEPROM.write(INIT_ADDR, INIT_KEY);  // write key
    EEPROM.write(CONF_ADDR, 0);         // init position addres position data in memory first byte
    EEPROM.write(CONF_ADDR + 1, 0);     // init position addres position data in memory second byte
    EEPROM.put(0, config);              // save default config
  }
}

Config getConfig()
{
  uint16_t configPosition; 
  EEPROM.get(CONF_ADDR, configPosition);
  return EEPROM.get(configPosition, config);
}

void save(Config config)
{
  config.writeCicleCount++;
  uint16_t configPosition; 
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
