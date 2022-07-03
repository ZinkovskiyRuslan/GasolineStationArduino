#include "esp_sleep.h"
#include "BluetoothSerial.h"
#include "esp_bt_device.h"
#include <WiFiMulti.h>

#define uS_TO_S_FACTOR 1000000  /* коэффициент пересчета микросекунд в секунды */
#define TIME_TO_SLEEP  58        /* время, в течение которого будет спать ESP32 (в секундах) */

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR bool isFistSend = true;
RTC_DATA_ATTR int values[60];
RTC_DATA_ATTR int tankerLevelOld = 0;

const int porTankerValue = 22;
const int controllerId = 1;

int portValue = 0;
const char* ssid     = "wifi";
const char* password = "pRLMa1qy";

//const char* ssid     = "3.14";
//const char* password = "0123456789";

//const char* ssid     = "BL7000";
//const char* password = "1234567890";

int waitFindWiFi = 5;
int logLevel = 0; //0 - all/ 1 - debug/2 - Error/3 - Off
bool isShowLogLevel = false;
const uint16_t port = 80;
const char * host = "erpelement.ru"; // ip or dns

WiFiClient client;
WiFiMulti WiFiMulti;

int LED_BUILTIN = 12;

void setup() {
  pinMode(porTankerValue, INPUT);
  pinMode (LED_BUILTIN, OUTPUT);// задаем контакт подключения светодиода как выходной
  digitalWrite (LED_BUILTIN, LOW);
  Serial.begin(115200); // Запускаем последовательный монитор
  Serial.println("\n");
  WiFiMulti.addAP(ssid, password);
  if (bootCount == 0)
    SetDefaultValues();
  int tankerLevel = getTankerLevel();
  Log("\n bootCount=" + String(bootCount) + "; value=" + String(tankerLevel) + "; tankerLevelOld=" + String(tankerLevelOld) + "; isFistSend=" + String(isFistSend), 2);
  values[bootCount] = tankerLevel;
  bootCount++;
  if (isFistSend)
  {
    if (bootCount == 6)
    {
      addTankerValue();
      SetDefaultValues();
      isFistSend = false;
      bootCount = 0;
    }

  } else {
    if ( abs(tankerLevelOld - tankerLevel) > 10 || bootCount == 60)
    {
      addTankerValue();
      SetDefaultValues();
      bootCount = 0;
    }
  }
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO);
  delay(1000);
  esp_deep_sleep_start();
}

void loop() {
}

void SetDefaultValues()
{
  tankerLevelOld = getTankerLevel();
  for (int i = 0; i < (sizeof values / sizeof(values[0])) - 1; i++)
  {
    values[i] = 0;
  }
}

void ConnectToModem()
{
  if (!WiFiConnect(3))
  {
    ESP.restart();
  } else {
    WiFiDisconnect();
  }
}

void Log(String message, int level)
{
  if (level >= logLevel && isShowLogLevel)
    Serial.print(message + ";Level:" + String(level));
  if (level >= logLevel && !isShowLogLevel)
    Serial.print(message);
}

int getTankerLevel()
{
  Log("\ngetTankerLevel", 1);
  int oldPortValue = 0;
  int pulseTanker = 0;
  int curr = 0;
  uint32_t tmr = micros();

  while (digitalRead(porTankerValue) == HIGH);
  while (digitalRead(porTankerValue) == LOW);
  while (micros() - tmr < 1000000)
  {
    portValue = digitalRead(porTankerValue);
    if (oldPortValue == 0 && portValue == HIGH)
    {
      oldPortValue = 1;
      pulseTanker++;
      delayMicroseconds(10);
    }
    if (oldPortValue == 1 && portValue == LOW)
    {
      oldPortValue = 0;
      pulseTanker++;
      delayMicroseconds(10);
    }
  }
  /*
    for (int i=0; i< 1500; i++)
    {
    Log(String(logs[i]) + ";",2);
    }
  */
  //return (4000 - pulseTanker);
  return pulseTanker;
}

void ResendFromEerpom()
{
  /*
    EEPROM.get(0, eepromValue);
    Log("\nResendFromEerpom: " + String(eepromValue), 3);
    if(String(eepromValue).length() > 0)
    {
      Serial.println("\nResend from eerpom after reboot:" + String(eepromValue));
      if(String(eepromValue) != "/api/system/getFuelInfo.php?systemId=0&id=Start")
        String response = GetRestResponse(String(eepromValue));
      else
        Log("Skip ResendFromEerpom Start", 0);
      String("").toCharArray(eepromValue, sizeof eepromValue);
      EEPROM.put(0, eepromValue);
      EEPROM.commit();
    }
  */
}

void RestartWithEeprom(String getMethod)
{
  Log("Save to EEPROM value: " + getMethod, 3);
  /*
      getMethod.toCharArray(eepromValue,sizeof eepromValue);
      EEPROM.put(0, eepromValue);
      EEPROM.commit();
  */
  ESP.restart();
}

void ExternalLedBlink(int delayValue, int repeat)
{
  Log("\nExternalLedBlink(delayValue=" + String(delayValue) + ", repeat=" + String(repeat) + ")", 0);
  for (int i = 0; i < repeat; i++)
  {
    delay(delayValue);
    digitalWrite (LED_BUILTIN, LOW);
    delay(delayValue);
    digitalWrite (LED_BUILTIN, HIGH);
  }
}

void addTankerValue()
{
  //ExternalLedBlink(200, 50);
  String valueList = "";
  for (int i = 0; i < (sizeof values / sizeof(values[0])); i++)
  {
    if (values[i] == 0) continue;
    valueList += String(values[i]);
    valueList += ";" ;
  }
  valueList.remove(valueList.length() - 1, 1);
  //http://erpelement.ru/api/system/addTankerValue.php?token=klSimn53uRescojnfls&controllerId=1&value=789;788;777
  String response = GetRestResponse( "api/system/addTankerValue.php?token=klSimn53uRescojnfls&controllerId=" + String(controllerId) + "&value=" + valueList );
}

bool WiFiConnect(int connectTryCnt)
{
  Log("\nWiFiConnect connectTryCnt = " + String(connectTryCnt), 1);
  for (int i = 0; i < connectTryCnt; i++)
  {
    int retryWait = 0;
    Log("\nWaiting for WiFi connect try = " + String(i), 1);
    while (WiFiMulti.run() != WL_CONNECTED) {
      Log(".", 1);
      if (retryWait > waitFindWiFi) {
        Log("\nError Find WiFi", 2);
        WiFiDisconnect();
        break;
      }
      retryWait++;
      delay(1000);
    }
    Log("\nWiFiMulti.run:" + String(WiFiMulti.run())  + " IP:" + WiFi.localIP().toString(), 0);
    if (WiFiMulti.run() == WL_CONNECTED && WiFi.localIP().toString() != "0.0.0.0")
      break;
  }
  return (WiFiMulti.run() == WL_CONNECTED);
}

void WiFiDisconnect()
{
  Log("\nWiFiDisconnect", 0);
  // We start by connecting to a WiFi network
  // Удаляем предыдущие конфигурации WIFI сети
  WiFi.disconnect(); // обрываем WIFI соединения
  WiFi.softAPdisconnect(); // отключаем отчку доступа(если она была
  WiFi.mode(WIFI_OFF); // отключаем WIFI
  delay(1000);
}

String GetRestResponse(String getMethod)
{
  String response = "";

  for (int i = 0; i < 3 && WiFi.localIP().toString() == "0.0.0.0"; i++)
  {
    if (!WiFiConnect(3))
      RestartWithEeprom(getMethod);
    if (WiFi.localIP().toString() == "0.0.0.0")
      WiFiDisconnect();
  }

  setWanConnectModem();

  Serial.println("\nWiFi connected. IP address: " + WiFi.localIP().toString());
  Serial.println("Connecting to GET: " + String(host) + "/" + getMethod);

  bool isConnected = false;
  for (int i = 0; i < 6 && isConnected == false; i++)
  {
    Log("\nclient.connect host=" + String(host) + " port=" + port + " IP:" + WiFi.localIP().toString(), 0);
    if (!client.connect(host, port)) {
      Serial.println("\nConnection failed.");
      client.stop();
      //WiFiDisconnect();
      //RestartWithEeprom(getMethod);
    } else {
      isConnected = true;
    }
  }
  if (!isConnected)
  {
    WiFiDisconnect();
    RestartWithEeprom(getMethod);
  }

  client.println("GET /" + getMethod + " HTTP/1.1");
  client.println("Host: " + String(host) + " \n\n");

  uint32_t tmrBegin = micros();
  int currLoop = 0;
  while (!client.available() && currLoop++ <= 3000)
  {
    delay(10);
  }
  Log("\nWait responce time: " +  String((micros() - tmrBegin) / 1000000) + " sec ", 3);
  if (client.available() > 0)
  {
    while (client.available() > 0)
    {
      response = client.readStringUntil('\n');
    }
    Log("\n" + response, 1);
  }
  else
  {
    Serial.println("\nclient.available() timed out ");
    client.stop();
    WiFiDisconnect();
    RestartWithEeprom(getMethod);
  }
  Serial.println("\nClosing connection.");
  client.stop();
  WiFiDisconnect();
  return response;
}

// При простое модем уходит в спящий режим.
// Переводим в активный режим
void setWanConnectModem()
{
  if (!client.connect("192.168.1.1", 80))
  {
    Serial.println("Connection modem failed.");
  } else {
    client.println("GET /goform/setWanConnect?profile_id=25099_1&profile_type=0 HTTP/1.1");
    client.println("Host: " + String("192.168.1.1") + " \n\n");

    int currLoop = 0;
    while (!client.available() && currLoop++ < 3000)
    {
      delay(10);
    }

    if (client.available() > 0)
    {
      String response = "";
      while (client.available() > 0)
      {
        response = client.readStringUntil('\n');
      }
      Log("setWanConnectModem response: " + response, 0);
    }
  }
}
