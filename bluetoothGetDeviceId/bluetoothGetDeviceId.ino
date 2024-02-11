#include "BluetoothSerial.h"
#include "esp_bt_device.h"
#include <GyverOLED.h>

GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

bool isBtConnected;

BluetoothSerial SerialBT;

String incoming;

struct Command
{
    String GetfuelVolume = "0";
    String GetfuelVolumeRetry = "1";
    String StartFuelFill = "2";
};

struct Command Command;

void setup() {  
  Serial.begin(115200); // Запускаем последовательный монитор 
  Serial.println("\n");  

  SerialBT.begin("Идентификатор"); // Задаем имя вашего устройства Bluetooth
  SerialBT.register_callback(callback); 
  
  oled.init(27, 26);   // инициализация init(sda, scl)
  oled.clear();       // очистка
  oled.setScale(1);   // масштаб текста (1..4)
  oled.home();        // курсор в 0,0
  oled.setCursorXY(0, 0);
  oled.print("Wait connection...");
}
 

void Log(String message)
{
    Serial.print(message);
}


void loop() {
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
    
    while ((str = strtok_r(p, "|", &p)) != NULL) 
    {
      if(curr == 0) 
      {
        oled.clear();
        oled.setCursorXY(0, 12);
        oled.print("DeviceID:");
        oled.setCursorXY(0, 28);
        oled.print(String(str));
        delay(100);
      }
      curr++;
    }
  
  }  
}
  
void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
  if(event == ESP_SPP_SRV_OPEN_EVT){
    isBtConnected = true;
    Serial.println("Client Connected");
  }

  if(event == ESP_SPP_CLOSE_EVT ){
    Serial.println("Client disconnected");
    isBtConnected = false;
  }
}
