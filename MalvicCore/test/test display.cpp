#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Новая распиновка SPI для дисплея (только боковые пины)
#define OLED_MOSI   11 
#define OLED_CLK    12 
#define OLED_DC     13
#define OLED_CS     10
#define OLED_RESET  1 

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

unsigned long lastBlinkTime = 0;
bool isBlinking = false;
int blinkDuration = 150; 

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    if(!display.begin(SSD1306_SWITCHCAPVCC)) {
        Serial.println(F("[ERR] Ошибка инициализации SSD1306 по SPI"));
        for(;;); 
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(15, 28);
    display.println("SPI DISPLAY OK");
    display.display();
    delay(2000);
}

void loop() {
    display.clearDisplay();
    unsigned long currentMillis = millis();

    if (!isBlinking && (currentMillis - lastBlinkTime > random(2000, 5000))) {
        isBlinking = true;
        lastBlinkTime = currentMillis;
    } else if (isBlinking && (currentMillis - lastBlinkTime > blinkDuration)) {
        isBlinking = false;
        lastBlinkTime = currentMillis;
    }

    int left_eye_x = 40;
    int right_eye_x = 88;
    int eye_y = 32;

    if (isBlinking) {
        display.drawLine(left_eye_x - 12, eye_y, left_eye_x + 12, eye_y, SSD1306_WHITE);
        display.drawLine(right_eye_x - 12, eye_y, right_eye_x + 12, eye_y, SSD1306_WHITE);
    } else {
        display.fillRoundRect(left_eye_x - 12, eye_y - 14, 24, 28, 6, SSD1306_WHITE);
        display.fillRoundRect(right_eye_x - 12, eye_y - 14, 24, 28, 6, SSD1306_WHITE);
        display.fillCircle(left_eye_x, eye_y, 4, SSD1306_BLACK); 
        display.fillCircle(right_eye_x, eye_y, 4, SSD1306_BLACK); 
    }

    display.display();
}