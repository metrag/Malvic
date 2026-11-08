#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>

// --- Распиновка SPI для дисплея ---
#define OLED_MOSI   11
#define OLED_CLK    12
#define OLED_DC     13
#define OLED_CS     10
#define OLED_RESET  1

// --- Распиновка I2C для MPU6050 ---
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Объекты дисплея и гироскопа
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);
Adafruit_MPU6050 mpu;

bool mpu_ok = false;
uint32_t dizzy_until_ms = 0; // Таймер "головокружения" от тряски

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n--- Инициализация Малвика ---");

    // 1. Инициализация SPI-экрана
    if(!display.begin(SSD1306_SWITCHCAPVCC)) {
        Serial.println(F("[ERR] Ошибка экрана!"));
        for(;;); 
    }
    Serial.println("[OK] SPI дисплей запущен.");

    // 2. Инициализация I2C (400 кГц для быстрой работы)
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 400000);

    // 3. Инициализация MPU6050 (адрес 0x68)
    if (!mpu.begin(0x68, &Wire)) {
        Serial.println("[ERR] MPU6050 не найден! Проверь провода 8 и 9.");
    } else {
        Serial.println("[OK] MPU6050 запущен.");
        mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        mpu_ok = true;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 28);
    display.println(mpu_ok ? "MPU6050 OK!" : "MPU6050 ERROR");
    display.display();
    delay(1500);
}

void drawEyes(int offset_x, int offset_y, bool is_dizzy) {
    display.clearDisplay();

    // Координаты центров глаз
    int lx = 40 + constrain(offset_x, -12, 12);
    int rx = 88 + constrain(offset_x, -12, 12);
    int ey = 32 + constrain(offset_y, -8, 8);

    if (is_dizzy) {
        // Рисуем крестики, если робота потрясли
        display.drawLine(lx - 10, ey - 10, lx + 10, ey + 10, SSD1306_WHITE);
        display.drawLine(lx - 10, ey + 10, lx + 10, ey - 10, SSD1306_WHITE);
        display.drawLine(rx - 10, ey - 10, rx + 10, ey + 10, SSD1306_WHITE);
        display.drawLine(rx - 10, ey + 10, rx + 10, ey - 10, SSD1306_WHITE);
    } else {
        // Обычные глаза со зрачками
        display.fillRoundRect(lx - 12, ey - 14, 24, 28, 6, SSD1306_WHITE);
        display.fillRoundRect(rx - 12, ey - 14, 24, 28, 6, SSD1306_WHITE);
        
        // Зрачки смещаются чуть сильнее, создавая эффект параллакса
        display.fillCircle(lx + (offset_x / 3), ey + (offset_y / 3), 4, SSD1306_BLACK);
        display.fillCircle(rx + (offset_x / 3), ey + (offset_y / 3), 4, SSD1306_BLACK);
    }

    display.display();
}

void loop() {
    uint32_t now = millis();
    int eye_dx = 0;
    int eye_dy = 0;

    if (mpu_ok) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);

        // Коэффициенты для смещения взгляда от наклона - поменять на + для инверсии
        eye_dx = (int)(a.acceleration.y * -2.0f); 
        eye_dy = (int)(a.acceleration.x * -1.5f);

        // Вычисление вектора перегрузки для детекции тряски
        float total_accel = sqrtf(
            a.acceleration.x * a.acceleration.x +
            a.acceleration.y * a.acceleration.y +
            a.acceleration.z * a.acceleration.z
        );

        // Если перегрузка больше 20 м/с^2 (~2G), включаем "головокружение" на 2 секунды
        if (total_accel > 20.0f) {
            dizzy_until_ms = now + 2000;
            Serial.println("[!] Тряска обнаружена!");
        }
    }

    drawEyes(eye_dx, eye_dy, (now < dizzy_until_ms));
}