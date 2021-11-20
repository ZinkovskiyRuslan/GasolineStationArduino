
#include "BluetoothSerial.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

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

void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
  if(event == ESP_SPP_SRV_OPEN_EVT){
    Serial.println("Client Connected");
  }

  if(event == ESP_SPP_CLOSE_EVT ){
    Serial.println("Client disconnected");
  }
}

void setup() {
  Serial.begin(115200); // Запускаем последовательный монитор 
  ESP_BT.begin("АЗС"); // Задаем имя вашего устройства Bluetooth
  ESP_BT.register_callback(callback);
  
  pinMode (LED_BUILTIN, OUTPUT);// задаем контакт подключения светодиода как выходной
  printDeviceAddress();

  //fill test data
  for (int i=0; i<999; i++)
  {
      ClientFuelInfo[i].id  = "->" + String(i) + "<-";
      ClientFuelInfo[i].fuel = i;
      if (i==900)
      {
        ClientFuelInfo[i].id = "b73147eb6ff3e51b";
      }
  }
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
        for (int i=0; i<999; i++)
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
                ClientFuelInfo[i].fuel=100;
            }
          }
        }
      }
    }    
  }  
  delay(100);
}

  
  

void printDeviceAddress() { 
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
