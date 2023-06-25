#include <GyverOLED.h>

GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

const int pinBtnStart = 4;
const int pinImpulse = 7;

void setup() {
  Serial.begin(115200);
  
  pinMode(pinBtnStart, INPUT_PULLUP);
  pinMode(pinImpulse, OUTPUT);
  
  oled.init();        // инициализация
  oled.clear();       // очистка
  oled.setScale(1);   // масштаб текста (1..4)
  oled.home();        // курсор в 0,0
  oled.setCursorXY(20, 0);
  oled.print("Wait");


}
void loop() {
  Serial.println(String(digitalRead(pinBtnStart)));
  if(!digitalRead(pinBtnStart))
  {
    oled.clear();       // очистка
    oled.setCursorXY(20, 14);
    oled.print("Start"  );
    delay(100);
    for(int i = 0; i < 10000000; i++)
    {
      //delay(1);
      digitalWrite(pinImpulse, HIGH);
      //delay(1);
      digitalWrite(pinImpulse, LOW);
    }

    oled.setCursorXY(20, 28);
    oled.print("Stop "  );
  }
}
