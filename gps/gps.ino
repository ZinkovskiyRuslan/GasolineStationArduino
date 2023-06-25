#include <Wire.h>
#include <TinyGPS++.h>
#include <GyverOLED.h>

#define RXD2 16
#define TXD2 21
HardwareSerial neogps(1);

TinyGPSPlus gps;

GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

void setup() {
  Serial.begin(115200);
  //Begin serial communication Arduino IDE (Serial Monitor)

  //Begin serial communication Neo6mGPS
  neogps.begin(9600, SERIAL_8N1, RXD2, TXD2);
  
  oled.init();        // инициализация
  oled.clear();       // очистка
  oled.setScale(1);   // масштаб текста (1..4)
  oled.home();        // курсор в 0,0

  delay(2000);
}

void loop() {
    
  boolean newData = false;
  for (unsigned long start = millis(); millis() - start < 1000;)
  {
    while (neogps.available())
    {
      if (gps.encode(neogps.read()))
      {
        newData = true;
      }
    }
  }

  //If newData is true
  if(newData == true)
  {
    newData = false;
    Serial.println(gps.satellites.value());
    oled.clear();
    oled.home();
    oled.print("Satellites");
    oled.setCursorXY(0, 12);
    oled.print("Count: " + String(gps.satellites.value()));
    if (gps.location.isUpdated()){
      Serial.print("Latitude= "); 
      Serial.print(gps.location.lat(), 6);

      oled.setCursorXY(0, 24);
      oled.print("Latitude: " + String(gps.location.lat(), 6));

      Serial.print(" Longitude= "); 
      Serial.println(gps.location.lng(), 6);
      
      oled.setCursorXY(0, 36);
      oled.print("Longitude: " + String(gps.location.lng(), 6));
    }
  }
  else
  {
     Serial.println("No Data");
     oled.clear();
     oled.print("No Data");
  }  
}