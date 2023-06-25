
#include <LiquidCrystal_I2C.h>  // подключаем библиотеку дисплея

uint32_t i = 0;

LiquidCrystal_I2C lcd(0x27, 20, 4);  // адрес, столбцов, строк

void setup() {
  Serial.begin(115000);
  while (!Serial);
  Serial.println("");
  Serial.println("Старт приложеия");
  lcd.init();           // инициализация
  lcd.backlight();      // включить подсветку  
  lcd.setCursor(1, 0);  // столбец 1 строка 0
  lcd.print("Hell word!");
}

void loop() {
  i = i + 1;
  lcd.setCursor(4, 1);
  lcd.print(String(i));
}
