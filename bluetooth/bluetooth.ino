#include "BluetoothSerial.h"
#include "esp_bt_device.h"
#include <WiFiMulti.h>

const int portPin1 = 34;
const int portPin2 = 35;
const int portStartModem = 5;

const int porTankerValue = 27;
const int portExternalLed = 4;
const int portCheckPumpStatus = 2;
const int portTrigger = 15;
const int portCheckBatteryStatus = 14;
const int portBattery = 13;



const String systemId = "0";
const int portValueMax = 4095;
const int pulsePerLiter = 200; //number of pulses per liter
const int preStopTrigger = 25; //min value = 0; max value "pulsePerLiter"; 1 = 10ml

int pulse = 0;
int level = 0;
int portValue = 0;

int waitingSignal = 30; //waiting for signal from trigger in seconds 

const char* ssid     = "wifi";
const char* password = "pRLMa1qy";
//const char* ssid     = "3.14";
//const char* password = "0123456789";
int retryFindWiFi = 5;

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
  getFuelInfo("Start"); 
  SerialBT.begin("АЗС"); // Задаем имя вашего устройства Bluetooth
  SerialBT.register_callback(callback);  
  printDeviceAddress();
}
 
void loop() {    
  digitalWrite(portBattery, HIGH);
  delay(500); // 50 millisecond timeout    
  digitalWrite (LED_BUILTIN, LOW);
  digitalWrite (portExternalLed, LOW);
  
  delay(500); // 50 millisecond timeout    
  digitalWrite (LED_BUILTIN, HIGH);  
  digitalWrite (portExternalLed, HIGH);
  
  if(analogRead(portCheckPumpStatus) == portValueMax)
    StartFuelFillByButton();
  
  if(analogRead(portCheckBatteryStatus) == 0)
    SendBatteryEvent();
    
  Serial.println("<<< " + String(analogRead(portCheckBatteryStatus))+"--" +String(analogRead(porTankerValue)));
    
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
                portValue = analogRead(portPin1);
                if(level == 0 && portValue == portValueMax)
                {
                  level = 1;
                  pulse++;
                  tmr = micros();
                }
                if(level == 1 && portValue < portValueMax)
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
            printf("curr=%i id=%s fuel=%i\n", i, ClientFuelInfo[i].id.c_str(), ClientFuelInfo[i].fuel);
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
    Serial.println("Client Connected");
  }

  if(event == ESP_SPP_CLOSE_EVT ){
    Serial.println("Client disconnected");
        
    digitalWrite(LED_BUILTIN, LOW);
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
void WiFiRun()
{
    if (WiFiConnect())
      return;
    StartModem();
    WiFiRun();
}

void StartModem()
{
  Serial.println("Start Modem KeyDown");
  digitalWrite(portStartModem, HIGH);
  delay(3000);
  Serial.println("Start Modem KeyUp");
  digitalWrite(portStartModem, LOW);
}

bool WiFiConnect()
{
  int retry = 0;
  Serial.print("Waiting for WiFi");
  while(WiFiMulti.run() != WL_CONNECTED) {
      Serial.print(".");
      if (retry > retryFindWiFi)
        {
          Serial.println("\nError Find WiFi");
          WiFi.mode(WIFI_OFF);
          return false;
        }
      retry++;
      delay(1000);
  }
  return true;
}

String GetRestResponse(String getMethod)
{
  String response = "";  
  WiFiRun();
  // begin region modem connect to 3g
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
      while(client.available() > 0)
      {        
        response = client.readStringUntil('\n');
      }
      Serial.println("modem response: " + response);
    }
  }
  // end region modem connect to 3g
  
  Serial.println("\nWiFi connected. IP address: " + WiFi.localIP().toString());
  Serial.println("Connecting to GET: " + String(host) + "/" + getMethod);

  if (!client.connect(host, port)) {
      Serial.println("Connection failed.");
      client.stop();  
      WiFi.mode(WIFI_OFF);
      return "";
  }
  client.println("GET /" + getMethod + " HTTP/1.1");
  client.println("Host: " + String(host) + " \n\n");

  int currLoop = 0;
  while (!client.available() && currLoop++ < 3000)
  {
    delay(10);
  }
  
  if (client.available() > 0)
  {
    while(client.available() > 0)
    {        
      response = client.readStringUntil('\n');
    }
    Serial.println(response);
  }
  else
  {
    Serial.println("client.available() timed out ");
    client.stop();
    WiFi.mode(WIFI_OFF);
    return "";
  }
  Serial.println("Closing connection.");
  client.stop();
  WiFi.mode(WIFI_OFF);
  return response;
}

void StartFuelFillByButton()
{
  uint32_t tmr = micros();
  pulse = 0;
  
  while(true)
  {     
      portValue = analogRead(portPin1);
      if(level == 0 && portValue == portValueMax)
      {
        level = 1;
        pulse++;
        tmr = micros();
      }
      if(level == 1 && portValue < portValueMax)
      {
        level = 0;
        pulse++;
        tmr = micros();
      }
      if(micros() - tmr > waitingSignal*1000000)
      {
        Serial.println("Выход. Нет ответа от счётчика при ручой заправке");        
        if(pulse > pulsePerLiter)
          String response = GetRestResponse("api/system/sendTelegram.php?token=klSimn53uRescojnfls&message=Ручая%20заправка%20на%20" + String(pulse/pulsePerLiter) + "л.");
        else
          Serial.println("Ложное срабатывание: pulse=" + String(pulse));        
        break;
      }
  }            
}

void SendBatteryEvent()
{
   String response = GetRestResponse("api/system/sendTelegram.php?token=klSimn53uRescojnfls&message=Переход%20АЗС%20на%20резервное%20питание.");
     
}
