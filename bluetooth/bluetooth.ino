#include "BluetoothSerial.h"
#include "esp_bt_device.h"
#include <WiFiMulti.h>

const int batteryVoltagePin  = 36;
const int portPin1 = 34;
const int portPin2 = 35;
const int portTrigger = 27;
const int pulsePerLiter = 200; //number of pulses per liter
const int preStopTrigger = 25; //min value = 0; max value "pulsePerLiter"; 1 = 10ml

int pulse = 0;
int level = 0;
int portValue = 0;

int waitingSignal = 15; //waiting for signal in seconds 

const char* ssid     = "wifi";
const char* password = "0123456789";
int retryFindWiFi = 5;

const uint16_t port = 80;
const char * host = "erpelement.ru"; // ip or dns

BluetoothSerial SerialBT;

String incoming;
int LED_BUILTIN = 12;

struct Command
{
    String GetfuelVolume = "0";
    String StartFuelFill = "1";
};

struct ClientFuelInfo
{
    String id;
    int fuel;    
};

struct Command Command;
struct ClientFuelInfo ClientFuelInfo[1000];

void setup() {  
  pinMode(batteryVoltagePin, INPUT);
  pinMode(portPin1, INPUT);
  pinMode(portPin2, INPUT);
  pinMode(portTrigger, OUTPUT);
  digitalWrite(portTrigger, LOW);
  pinMode (LED_BUILTIN, OUTPUT);// задаем контакт подключения светодиода как выходной
  digitalWrite (LED_BUILTIN, LOW);
  
  Serial.begin(2000000); // Запускаем последовательный монитор 
  Serial.println("\n");
  getFuelInfo("Start");
  SerialBT.begin("АЗС"); // Задаем имя вашего устройства Bluetooth
  SerialBT.register_callback(callback);  
  printDeviceAddress();
}
 
void loop() {  
  delay(50); // 100 millisecond timeout    
  digitalWrite (LED_BUILTIN, LOW);
  delay(50); // 100 millisecond timeout    
  digitalWrite (LED_BUILTIN, HIGH);
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
        SerialBT.disconnect();
        if(fuelVolume == 0)
        {
          getFuelInfo(deviceUniqueID);
        }
      }
      if(command == Command.StartFuelFill)
      {
        for (int i=0; i< (sizeof ClientFuelInfo/sizeof ClientFuelInfo[0]); i++)
        {
          if(ClientFuelInfo[i].id == deviceUniqueID && ClientFuelInfo[i].fuel > 0)
          {
            SerialBT.println(ClientFuelInfo[i].id +"|"+Command.StartFuelFill+"|"+ ClientFuelInfo[i].fuel);
            Serial.println(">>> " + ClientFuelInfo[i].id +"|"+Command.StartFuelFill+"|"+ ClientFuelInfo[i].fuel);
            int fuelBegin = ClientFuelInfo[i].fuel;
            digitalWrite(portTrigger, HIGH);
            uint32_t tmr = micros();
            while(ClientFuelInfo[i].fuel > 0)
            {     
                portValue = analogRead(portPin1);
                if(level == 0 && portValue == 4095)
                {
                  level = 1;
                  pulse++;
                  tmr = micros();
                }
                if(level == 1 && portValue < 4095)
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
  String response = GetRestResponse("getFuelInfo.php?id=" + id + "&batteryVoltage=" + analogRead(batteryVoltagePin));
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
  String response = GetRestResponse("setFuelInfo.php?id=" + id + "&fuel=" + fuel);
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

String GetRestResponse(String getMethod)
{
  String response = "";
  WiFiMulti WiFiMulti;
  WiFiMulti.addAP(ssid, password);  
  
  Serial.print("Waiting for WiFi");
  int retry = 0;
  while(WiFiMulti.run() != WL_CONNECTED) {
      Serial.print(".");
      if (retry > retryFindWiFi)
        {
          Serial.println("\nError Find WiFi");
          WiFi.mode(WIFI_OFF);
          return "";
        }
      retry++;
  }
  Serial.println("\nWiFi connected. IP address: " + WiFi.localIP().toString());
  Serial.println("Connecting to GET: " + String(host) + "/" + getMethod);
  WiFiClient client;
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
