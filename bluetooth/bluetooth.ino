#include "BluetoothSerial.h"
#include "esp_bt_device.h"
#include <WiFiMulti.h>
#include <EEPROM.h>
#define EEPROM_SIZE 200

const int portStartModem = 33;
const int portPin1 = 34;
const int portPin2 = 35;

const int porTankerValue = 22;
const int portExternalLed = 21;
const int portCheckPumpStatus = 23;
const int portTrigger = 19;
const int portCheckBatteryStatus = 18;
const int portBattery = 5;

const String systemId = "0";
const int pulsePerLiter = 200; //number of pulses per liter
const int preStopTrigger = 25; //min value = 0; max value "pulsePerLiter"; 1 = 10ml

int pulse = 0;
int level = 0;
int portValue = 0;
bool isBtConnected;
char eepromValue[200];

int waitingSignal = 30; //waiting for signal from trigger in seconds 

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

BluetoothSerial SerialBT;
WiFiClient client;
WiFiMulti WiFiMulti;

String incoming;
int LED_BUILTIN = 12;

struct Command
{
    String GetfuelVolume = "0";
    String GetfuelVolumeRetry = "1";
    String StartFuelFill = "2";
};

struct ClientFuelInfo
{
    String id;
    int fuel;    
};

struct Command Command;
struct ClientFuelInfo ClientFuelInfo[1000];

void setup() {  
  EEPROM.begin(EEPROM_SIZE);
  
  pinMode(portPin1, INPUT);
  pinMode(portPin2, INPUT);
  pinMode(portCheckPumpStatus, INPUT);
  pinMode(portTrigger, OUTPUT);
  pinMode(portStartModem, OUTPUT);
  pinMode(portExternalLed, OUTPUT);
  pinMode(portBattery, OUTPUT);
  pinMode(portCheckBatteryStatus, INPUT);
  pinMode(porTankerValue, INPUT);
  
  
  digitalWrite(portTrigger, LOW);
  digitalWrite(portStartModem, LOW);
  digitalWrite(portExternalLed, LOW);
  digitalWrite(portBattery, HIGH);
  
  pinMode (LED_BUILTIN, OUTPUT);// задаем контакт подключения светодиода как выходной
  digitalWrite (LED_BUILTIN, LOW);
  
  Serial.begin(115200); // Запускаем последовательный монитор 
  Serial.println("\n");  
  WiFiMulti.addAP(ssid, password);
  
  ExternalLedBlink(100, 3);
  ConnectToModem();
  
  ExternalLedBlink(100, 4);
  ResendFromEerpom();

  ExternalLedBlink(100, 5);
  getFuelInfo("Start"); 
  
  SerialBT.begin("АЗС"); // Задаем имя вашего устройства Bluetooth
  SerialBT.register_callback(callback);  
  printDeviceAddress();
}

void ConnectToModem()
{
  if(!WiFiConnect(3))
  {
    StartModem();
    ESP.restart();
  }else{
    WiFiDisconnect();
  }
}

void Log(String message, int level)
{
  if(level >= logLevel && isShowLogLevel)
    Serial.print(message + ";Level:" + String(level));
  if(level >= logLevel && !isShowLogLevel)
    Serial.print(message);
}

void loop() {

  //getFuelInfo("Start");  
  
  if(isBtConnected)
    ExternalLedBlink(100, 1);
  else
    ExternalLedBlink(500, 1);
    
  if(digitalRead(portCheckPumpStatus) == HIGH)
    StartFuelFillByButton();
  
  
  if(digitalRead(portCheckBatteryStatus) == LOW)
  {
    String response = GetRestResponse("api/system/sendTelegram.php?token=klSimn53uRescojnfls&message=Переход%20АЗС%20на%20резервное%20питание.");
    digitalWrite(portBattery, LOW);
    delay(5000);    
  }
  
  if (SerialBT.available()) // Проверяем, не получили ли мы что-либо от Bluetooth модуля
  {
    Serial.setTimeout(100); // 100 millisecond timeout 
    incoming = SerialBT.readString();
    Serial.println("<<< " + incoming);

    int str_len = incoming.length() + 1; 
    char char_array[str_len];
    incoming.toCharArray(char_array, str_len);
    char *str;
    char *p = char_array;
    int curr=0;
    String deviceUniqueID="", command="", value="";
    
    while ((str = strtok_r(p, "|", &p)) != NULL) 
    {
      if(curr == 0) deviceUniqueID = String(str);
      if(curr == 1) command = String(str);
      if(curr == 2) value = String(str);
      curr++;
    }
    if (curr == 3 )
    {
      if(command == Command.GetfuelVolume)
      {
        int fuelVolume = GetFuelVolume(deviceUniqueID);
        SerialBT.println(deviceUniqueID + "|0|" + fuelVolume);
        Serial.println(">>> " + deviceUniqueID +"|0|"+ fuelVolume);
        if(fuelVolume == 0)
        {
          getFuelInfo(deviceUniqueID);
        }
      }
      if(command == Command.GetfuelVolumeRetry)
      {
        int fuelVolume = GetFuelVolume(deviceUniqueID);
        SendCommand(deviceUniqueID, command, String(fuelVolume));
      }      
      if(command == Command.StartFuelFill)
      {
        for (int i=0; i < (sizeof ClientFuelInfo/sizeof ClientFuelInfo[0]); i++)
        {
          if(ClientFuelInfo[i].id == deviceUniqueID && ClientFuelInfo[i].fuel > 0)
          {
            pulse = 0;
            SerialBT.println(ClientFuelInfo[i].id +"|"+Command.StartFuelFill+"|"+ ClientFuelInfo[i].fuel);
            Serial.println(">>> " + ClientFuelInfo[i].id +"|"+Command.StartFuelFill+"|"+ ClientFuelInfo[i].fuel);
            int fuelBegin = ClientFuelInfo[i].fuel;
            digitalWrite(portTrigger, HIGH);
            
            uint32_t tmr = micros();
            while(ClientFuelInfo[i].fuel > 0)
            {     
                portValue = digitalRead(portPin1);
                if(level == 0 && portValue == HIGH)
                {
                  level = 1;
                  pulse++;
                  tmr = micros();
                }
                if(level == 1 && portValue == LOW)
                {
                  level = 0;
                  pulse++;
                  tmr = micros();
                }
                if(pulse == pulsePerLiter || (ClientFuelInfo[i].fuel == 1 && pulse == (pulsePerLiter - preStopTrigger)))
                {
                    pulse = 0;
                    ClientFuelInfo[i].fuel--;
                    Serial.println(">>> " + ClientFuelInfo[i].id + "|" + Command.StartFuelFill+"|" + ClientFuelInfo[i].fuel);
                    SerialBT.println(ClientFuelInfo[i].id + "|" + Command.StartFuelFill + "|" + ClientFuelInfo[i].fuel);
                }
                if(micros() - tmr > waitingSignal*1000000)
                {
                  Serial.println("Выход. Нет ответа от счётчика");
                  break;
                }
            }            
            digitalWrite(portTrigger, LOW);
            Serial.println(">>> " + ClientFuelInfo[i].id +"|"+Command.StartFuelFill+"|"+ ClientFuelInfo[i].fuel);
            SerialBT.println(ClientFuelInfo[i].id +"|"+Command.StartFuelFill+"|"+ ClientFuelInfo[i].fuel);
            
            setFuelInfo(ClientFuelInfo[i].id, fuelBegin - ClientFuelInfo[i].fuel);
            ClientFuelInfo[i].fuel=0;
            break;
          }
        }
      }
    }    
  }  
}

void ResendFromEerpom()
{
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
}

void RestartWithEeprom(String getMethod)
{
    Bzzz("RestartWithEeprom");
    Log("Save to EEPROM value: " + getMethod, 3);
    getMethod.toCharArray(eepromValue,sizeof eepromValue);
    EEPROM.put(0, eepromValue);
    EEPROM.commit();
    ESP.restart();
}

void ExternalLedBlink(int delayValue, int repeat)
{ 
  Log("\nExternalLedBlink(delayValue=" + String(delayValue) + ", repeat=" + String(repeat)+")", 0);
        
  for(int i = 0; i < repeat; i++)
  {
    delay(delayValue);
    digitalWrite (LED_BUILTIN, LOW);
    digitalWrite (portExternalLed, LOW);
    
    delay(delayValue);
    digitalWrite (LED_BUILTIN, HIGH);  
    digitalWrite (portExternalLed, HIGH);
  }
  Log("\nporTankerValue-" +String(digitalRead(porTankerValue))+
      "\nportCheckPumpStatus-" +String(digitalRead(portCheckPumpStatus))+
      "\nportCheckBatteryStatus-" + String(digitalRead(portCheckBatteryStatus)), 0);

}

void SendCommand(String deviceUniqueID, String command, String value)
{
  SerialBT.println(deviceUniqueID + "|" + command + "|" + value);
  Serial.println(">>> " + deviceUniqueID + "|" + command + "|" + value);
}
       
/*Возвращает доступный объём топлива из массива*/
int GetFuelVolume(String deviceUniqueID)
{
  for (int i = 0; i < (sizeof ClientFuelInfo/sizeof ClientFuelInfo[0]); i++)
  {
    if(ClientFuelInfo[i].id == deviceUniqueID && ClientFuelInfo[i].fuel > 0)
    {
      return ClientFuelInfo[i].fuel;
    }
  }
  return 0;
}

void getFuelInfo(String id)
{
  String response = GetRestResponse("api/system/getFuelInfo.php?systemId=" + systemId + "&id=" + id);
  if(response.length() > 0)
  {
    //set empty ClientFuelInfo array ToDo refactor
    for (int i = 0; i < (sizeof ClientFuelInfo/sizeof ClientFuelInfo[0]); i++)
    {
        ClientFuelInfo[i].id  = "";
        ClientFuelInfo[i].fuel = 0;
    }
    
    int i = 0;
    String subStr = "";     
    for (auto c : response)
    {
        if(c==';')
        {
            String id="";
            String fuel="";
  
            int index = subStr.indexOf('|');
            ClientFuelInfo[i].id = subStr.substring(0, index);
            ClientFuelInfo[i].fuel = subStr.substring(index + 1,subStr.length()).toInt();
            Log("\ncurr=" + String(i)  + " id=" + ClientFuelInfo[i].id.c_str() +" fuel=" + String(ClientFuelInfo[i].fuel), 1);
            i++;
            subStr = "";
        }else{
            subStr += c;
        }        
    }
  }
}

void setFuelInfo(String id, int fuel)
{
  String response = GetRestResponse("api/system/setFuelInfo.php?systemId=" + systemId + "&id=" + id + "&fuel=" + fuel);
}

void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
  if(event == ESP_SPP_SRV_OPEN_EVT){
    isBtConnected = true;
    Serial.println("Client Connected");
  }

  if(event == ESP_SPP_CLOSE_EVT ){
    Serial.println("Client disconnected");
    isBtConnected = false;    
    /*
    SerialBT.flush();
    SerialBT.disconnect();
    ESP.restart();
    */
    /*
    delay(1000);
    SerialBT.end();
    delay(1000);
    SerialBT.begin("АЗС");
    delay(1000);
    */
  }
}
   
void printDeviceAddress() { 
  Serial.print("Bluetooth: ");
  const uint8_t* point = esp_bt_dev_get_address(); 
  for (int i = 0; i < 6; i++) { 
    char str[3]; 
    sprintf(str, "%02X", (int)point[i]);
    Serial.print(str); 
    if (i < 5){
      Serial.print(":");
    } 
  }  
  Serial.println(""); 
}

void StartModem()
{
  Serial.println("Start Modem KeyDown");
  digitalWrite(portStartModem, HIGH);
  delay(3000);
  Serial.println("Start Modem KeyUp");
  digitalWrite(portStartModem, LOW);
  delay(5000);
}
void Bzzz(String message)
{
  Log("\nBzzz = " + message, 1);
  for(int i =  0; i < 20; i++ )
  { 
    digitalWrite(portBattery, LOW);
    delay(3); 
    digitalWrite(portBattery, HIGH);
    delay(3);
  }
}

bool WiFiConnect(int connectTryCnt)
{
  Log("\nWiFiConnect connectTryCnt = " + String(connectTryCnt), 1);
  int retryWait = 0;
  for(int i = 0; i < connectTryCnt; i++)
  {
    Log("\nWaiting for WiFi connect try = " + String(i), 1);
    while(WiFiMulti.run() != WL_CONNECTED) {
        Log(".", 1);
        if (retryWait > waitFindWiFi){
          Log("\nError Find WiFi", 2);
          Bzzz("WiFiConnect");
          WiFiDisconnect();
          break;
        }
        retryWait++;
        delay(1000);
    }
    Log("\nWiFiMulti.run:" + String(WiFiMulti.run())  + " IP:" + WiFi.localIP().toString(), 0);
    if(WiFiMulti.run() == WL_CONNECTED && WiFi.localIP().toString() != "0.0.0.0")
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

  for(int i = 0; i < 3 && WiFi.localIP().toString() == "0.0.0.0"; i++)
  {
    if (!WiFiConnect(3))
      RestartWithEeprom(getMethod);
    if(WiFi.localIP().toString() == "0.0.0.0")
        WiFiDisconnect();
  }
      
  //setWanConnectModem();
  
  Serial.println("\nWiFi connected. IP address: " + WiFi.localIP().toString());
  Serial.println("Connecting to GET: " + String(host) + "/" + getMethod);

  bool isConnected = false;
  for(int i = 0; i < 6 && isConnected == false; i++)
  {
    Log("\nclient.connect host=" + String(host) + " port=" + port + " IP:" + WiFi.localIP().toString(), 0);
    if (!client.connect(host, port)) {
        Serial.println("\nConnection failed.");
        client.stop();  
        //WiFiDisconnect();
        //RestartWithEeprom(getMethod);
    }else{
      isConnected = true;
    }
  }
  if(!isConnected)
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
  Log("\nWait responce time: " +  String((micros() - tmrBegin)/1000000) + " sec ", 3);
  if (client.available() > 0)
  {
    while(client.available() > 0)
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
  }else{
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
      while(client.available() > 0)
      {        
        response = client.readStringUntil('\n');
      }
      Log("setWanConnectModem response: " + response, 0);
    }
  }
}

void StartFuelFillByButton()
{
  Serial.println("Инициирована заправка по кнопке");
  uint32_t tmr = micros();
  uint32_t tmrBegin = micros();
  pulse = 0;
  
  while(true)
  {     
      portValue = digitalRead(portPin1);
      if(level == 0 && portValue == HIGH)
      {
        level = 1;
        pulse++;
        tmr = micros();
      }
      if(level == 1 && portValue == LOW)      {
        level = 0;
        pulse++;
        tmr = micros();
      }
      if(micros() - tmr > 1*1000000 && digitalRead(portCheckPumpStatus) != HIGH)
      {
        String response = GetRestResponse("api/system/sendTelegram.php?token=klSimn53uRescojnfls&message=Ручая%20заправка%20на%20" + String(pulse/pulsePerLiter) + "л.%20за%20" + String((micros()-tmrBegin/*-(micros() - tmr)*/)/1000000) + "сек");
        break;
      }
  }            
}
