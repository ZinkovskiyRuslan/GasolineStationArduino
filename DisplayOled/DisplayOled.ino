#include <GyverOLED.h>
/*
  https://kit.alexgyver.ru/tutorials/oled/
    Arduino: SDA – A4, SCL – A5
    Wemos  : SDA – D2, SCL – D1
*/ 
GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

void setup() {
  Serial.begin(115200);
  
  oled.init();        // инициализация
  oled.clear();       // очистка
  oled.setScale(1);   // масштаб текста (1..4)
  oled.home();        // курсор в 0,0
  oled.setCursorXY(20, 0);
  oled.print("Setup");
  delay(1000);
}
void loop() {
    oled.setCursorXY(20, 12);
    oled.print("Loop");
    for(int i = 0; i < 10000000; i++)
    {
        oled.setCursorXY(20, 28);
        oled.print(i);
        delay(100);
    }
}
