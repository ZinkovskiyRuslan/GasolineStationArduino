#include "esp_sleep.h"
#include "BluetoothSerial.h"
#include "esp_bt_device.h"
#include <WiFiMulti.h>
#include <BLEDevice.h>
#include <BLEBeacon.h>

#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00) >> 8) + (((x)&0xFF) << 8))

int scanTime = 10; //In seconds
BLEScan *pBLEScan;
int dutValue = 0;
int dutTemperature = 0;

#define uS_TO_S_FACTOR 1000000    /* коэффициент пересчета микросекунд в секунды */
#define TIME_TO_SLEEP  58         /* время, в течение которого будет спать ESP32 (в секундах) */

const int controllerId = 1;
const int sensorId = 0;

int portValue = 0;
//const char* ssid     = "wifi";
//const char* password = "e15e9008";

const char* ssid     = "3.14";
const char* password = "0123456789";

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

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
      if (advertisedDevice.haveManufacturerData() == true)
      {
        std::string strManufacturerData = advertisedDevice.getManufacturerData();

        uint8_t cManufacturerData[100];
        strManufacturerData.copy((char *)cManufacturerData, strManufacturerData.length(), 0);

        BLEBeacon oBeacon = BLEBeacon();
        oBeacon.setData(strManufacturerData);

        if(strManufacturerData.length() == 26 && String(advertisedDevice.getAddress().toString().c_str()) == "00:18:e9:e4:cc:26")
        {
          
          Serial.printf("if ID: %04X Major: %d Minor: %d UUID: %s Power: %d strManufacturerDataLength: %d, Address: %s   \n", 
              oBeacon.getManufacturerId(), ENDIAN_CHANGE_U16(oBeacon.getMajor()), ENDIAN_CHANGE_U16(oBeacon.getMinor()), 
              oBeacon.getProximityUUID().toString().c_str(), oBeacon.getSignalPower(), strManufacturerData.length(),
              advertisedDevice.getAddress().toString().c_str()
            );
          Serial.println(String(advertisedDevice.getAddress().toString().c_str()));
          Serial.println("Температура = " + String(cManufacturerData[9]-50));
          dutTemperature = cManufacturerData[9]-50;
          //Serial.println(String(cManufacturerData[7])+String(cManufacturerData[6])+String(cManufacturerData[5]));
          dutValue = cManufacturerData[5] | (cManufacturerData[6] << 8) | (cManufacturerData[7] << 16) | (cManufacturerData[8] << 24);
          Serial.println("Уровень = " + String(dutValue));/*
          for (int i = 0; i < strManufacturerData.length(); i++)
          {
            Serial.printf("[%X]", cManufacturerData[i]);
          }
          Serial.println("");
          for (int i = 0; i < strManufacturerData.length(); i++)
          {
            Serial.println(String(cManufacturerData[i]));
          }
          */
        }
      }
    }
};


void setup() {  
  Serial.begin(115200); // Запускаем последовательный монитор
  Serial.println("\n");
  
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99); // less or equal setInterval value

  
  // put your main code here, to run repeatedly:
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  for(int i = 0; i < 3 && dutValue == 0; i++)
  {
    //delay(1000);
    Serial.print("Retry scan Dut: " + String(i));
    BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  }
  //Serial.print("Devices found: ");
  //Serial.println(foundDevices.getCount());
  //Serial.println("Scan done!");
  pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory
  delay(1000);
  
  pinMode (LED_BUILTIN, OUTPUT);// задаем контакт подключения светодиода как выходной
  digitalWrite (LED_BUILTIN, LOW);
  WiFiMulti.addAP(ssid, password);
  
  String response = GetRestResponse( "api/system/addTankerValueTemperature.php?token=klSimn53uRescojnfls&controllerId=" + String(controllerId) + 
    "&sensorId=" + String(sensorId) + 
    "&value=" + String(dutValue) + 
    "&temperature=" + String(dutTemperature) 
  );

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO);
  delay(1000);
  esp_deep_sleep_start();
}

void loop() {
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
      //RestartWithEeprom(getMethod);
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
    //RestartWithEeprom(getMethod);
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
    //RestartWithEeprom(getMethod);
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
