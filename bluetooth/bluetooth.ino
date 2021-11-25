#include "BluetoothSerial.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
//#include <Arduino.h>
//#include <WiFi.h> 
#include <WiFiMulti.h>
//#include "Esp.h"

const char* ssid     = "3.14";
const char* password = "0123456789";
int retryFindWiFi = 5;

const uint16_t port = 80;
const char * host = "erpelement.ru"; // ip or dns

BluetoothSerial ESP_BT; // Объект для Bluetooth 

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
  Serial.begin(115200); // Запускаем последовательный монитор 
  Serial.println("\n");
  getFuelInfo();
  ESP_BT.begin("АЗС"); // Задаем имя вашего устройства Bluetooth
  ESP_BT.register_callback(callback);  
  pinMode (LED_BUILTIN, OUTPUT);// задаем контакт подключения светодиода как выходной
  printDeviceAddress();
}
 
void loop() {
  if (ESP_BT.available()) // Проверяем, не получили ли мы что-либо от Bluetooth модуля
  {
    Serial.setTimeout(100); // 100 millisecond timeout
    incoming = ESP_BT.readString();
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
        for (int i=0; i<999; i++)
        {
          if(ClientFuelInfo[i].id == deviceUniqueID)
          {
            ESP_BT.println(ClientFuelInfo[i].id +"|0|"+ ClientFuelInfo[i].fuel);
            Serial.println(">>> " + ClientFuelInfo[i].id +"|0|"+ ClientFuelInfo[i].fuel);
          }
        }
      }
      if(command == Command.StartFuelFill)
      {
        for (int i=0; i< (sizeof ClientFuelInfo/sizeof ClientFuelInfo[0]); i++)
        {
          if(ClientFuelInfo[i].id == deviceUniqueID)
          {
            ESP_BT.println(ClientFuelInfo[i].id +"|"+Command.StartFuelFill+"|"+ ClientFuelInfo[i].fuel);
            Serial.println(">>> " + ClientFuelInfo[i].id +"|"+Command.StartFuelFill+"|"+ ClientFuelInfo[i].fuel);
            if(ClientFuelInfo[i].fuel > 0)
            {
                for(int j = 0; j <= ClientFuelInfo[i].fuel;j=j+10)
                {
                  delay(200);
                  Serial.println(">>> " + ClientFuelInfo[i].id +"|"+Command.StartFuelFill+"|"+ j);
                  ESP_BT.println(ClientFuelInfo[i].id +"|"+Command.StartFuelFill+"|"+ j);
                }
                ClientFuelInfo[i].fuel=0;
            }
          }
        }
      }
    }    
  }  
  delay(100);
  //Serial.println(ESP.getFreeHeap());
}

void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
  if(event == ESP_SPP_SRV_OPEN_EVT){
    Serial.println("Client Connected");
  }

  if(event == ESP_SPP_CLOSE_EVT ){
    Serial.println("Client disconnected");
    //ESP.restart();
    delay(1000);
    ESP_BT.end();
    delay(1000);
    ESP_BT.begin("АЗС");
    delay(1000);
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

void getFuelInfo()
{
  String line="";
  WiFiMulti WiFiMulti;
  WiFiMulti.addAP(ssid, password);  
  Serial.print("Waiting for WiFi");
  int retry = 0;
  while(WiFiMulti.run() != WL_CONNECTED) {
      Serial.print(".");
      //delay(100);
      if (retry > retryFindWiFi)
        {
          Serial.println("\nError Find WiFi");
          return;
        }
      retry++;
  }
  Serial.println("\nWiFi connected. IP address: " + WiFi.localIP().toString());
  Serial.println("Connecting to " + String(host));
  WiFiClient client;
  if (!client.connect(host, port)) {
      Serial.println("Connection failed.");
      Serial.println("Waiting 5 seconds before retrying...");
      delay(5000);
      return;
  }
  client.println("GET /getFuelInfo.php HTTP/1.1");
  client.println("Host: " + String(host) + " \n\n");

  int maxloops = 0;
  while (!client.available() && maxloops < 1000)
  {
    maxloops++;
    delay(10); //delay 1 msec
  }
  if (client.available() > 0)
  {
    while(client.available() > 0)
    {        
      line = client.readStringUntil('\n');
    }
    Serial.println(line);
  }
  else
  {
    Serial.println("client.available() timed out ");
  }
  Serial.println("Closing connection.");
  client.stop();  
  WiFi.mode(WIFI_OFF);

  //set empty
  for (int i=0; i< (sizeof ClientFuelInfo/sizeof ClientFuelInfo[0]); i++)
  {
      ClientFuelInfo[i].id  = "";
      ClientFuelInfo[i].fuel = 0;
  }
  
  int i = 0;
  String subStr = "";     
  for (auto c : line)
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