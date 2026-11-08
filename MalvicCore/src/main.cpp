#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <driver/i2s.h>
#include <math.h>

// --- Распиновка ---
#define OLED_MOSI   11
#define OLED_CLK    12
#define OLED_DC     13
#define OLED_CS     10
#define OLED_RESET  1

#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

#define I2S_MIC_PORT I2S_NUM_0
#define MIC_WS_PIN   4
#define MIC_SCK_PIN  5
#define MIC_SD_PIN   6

// Пин для одинарного тумблера
#define MUTE_SWITCH_PIN 44 // Используем пин RX

Adafruit_SSD1306 display(128, 64, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);
Adafruit_MPU6050 mpu;
bool mpu_ok = false;

enum MalvicState { ASLEEP, AWAKE };
MalvicState currentState = ASLEEP;

unsigned long lastActivityTime = 0;
const unsigned long AWAKE_DURATION = 10000;
const float SHAKE_THRESHOLD = 20.0f;
const int AUDIO_WAKE_THRESHOLD = 8000;

int z_offset = 0;
bool is_muted = false;

// Переменные для отслеживания двойного щелчка (Kill Switch)
volatile uint32_t last_edge_ms = 0;
volatile uint8_t toggle_EdgeCount = 0;
volatile bool kill_switch_triggered = false;
uint32_t kill_banner_until_ms = 0;

// Обработчик прерывания от тумблера
void IRAM_ATTR onSwitchChange() {
    uint32_t now = millis();
    if (now - last_edge_ms > 40) { // Антидребезг контактов (40 мс)
        if (now - last_edge_ms < 600) { // Если прошло меньше 600 мс между щелчками
            toggle_EdgeCount++;
            if (toggle_EdgeCount >= 2) {
                kill_switch_triggered = true;
                toggle_EdgeCount = 0; // Сбрасываем счетчик
            }
        } else {
            toggle_EdgeCount = 1;
        }
        last_edge_ms = now;
    }
}

void setupI2SMic() {
    gpio_set_pull_mode((gpio_num_t)MIC_SD_PIN, GPIO_PULLDOWN_ONLY);
    
    i2s_config_t mic_cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 256,
        .use_apll = false
    };
    
    i2s_pin_config_t mic_pins = {
        .bck_io_num = MIC_SCK_PIN,
        .ws_io_num = MIC_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = MIC_SD_PIN
    };
    
    i2s_driver_install(I2S_MIC_PORT, &mic_cfg, 0, NULL);
    i2s_set_pin(I2S_MIC_PORT, &mic_pins);
    gpio_pulldown_en((gpio_num_t)MIC_SD_PIN);
}

void setup() {
    Serial.begin(115200);
    
    // Настройка пина тумблера
    pinMode(MUTE_SWITCH_PIN, INPUT_PULLUP);
    // Привязываем прерывание, чтобы ловить щелчки мгновенно
    attachInterrupt(digitalPinToInterrupt(MUTE_SWITCH_PIN), onSwitchChange, CHANGE);
    
    display.begin(SSD1306_SWITCHCAPVCC);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 400000);
    mpu_ok = mpu.begin(0x68, &Wire);
    
    if (mpu_ok) {
        mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    }
    
    setupI2SMic();
    lastActivityTime = millis();
    currentState = AWAKE;
}

int getAudioPeak() {
    // Если включен программный Mute — возвращаем тишину
    if (is_muted) return 0;

    int32_t raw_samples[128];
    size_t bytes_read = 0;
    i2s_read(I2S_MIC_PORT, raw_samples, sizeof(raw_samples), &bytes_read, 0);
    
    int samples_read = bytes_read / sizeof(int32_t);
    int max_amp = 0;
    
    for (int i = 0; i < samples_read; i++) {
        int16_t sample = (int16_t)(raw_samples[i] >> 14);
        if (abs(sample) > max_amp) {
            max_amp = abs(sample);
        }
    }
    return max_amp;
}

void drawScreen(int eye_dx, int eye_dy, bool kill_banner) {
    display.clearDisplay();

    // Верхняя панель статусов
    display.setTextSize(1);
    if (kill_banner) {
        display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(18, 2);
        display.print("!! KILL SWITCH !!");
    } else if (is_muted) {
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.print("[MUTE]");
    }

    display.setTextColor(SSD1306_WHITE);
    int lx = 40, rx = 88, ey = 32;

    if (currentState == ASLEEP) {
        // Закрытые глаза
        display.drawLine(lx - 12, ey + 5, lx + 12, ey + 5, SSD1306_WHITE);
        display.drawLine(rx - 12, ey + 5, rx + 12, ey + 5, SSD1306_WHITE);

        z_offset++;
        if(z_offset > 60) z_offset = 0;
        if(z_offset > 0)  { display.setCursor(95, 15 - (z_offset/3)); display.print("z"); }
        if(z_offset > 20) { display.setCursor(105, 10 - (z_offset/3)); display.print("z"); }
        if(z_offset > 40) { display.setCursor(115, 5 - (z_offset/3)); display.print("Z"); }
    } else {
        // Открытые глаза со смещением зрачков
        lx += constrain(eye_dx, -12, 12);
        rx += constrain(eye_dx, -12, 12);
        ey += constrain(eye_dy, -8, 8);

        display.fillRoundRect(lx - 12, ey - 14, 24, 28, 6, SSD1306_WHITE);
        display.fillRoundRect(rx - 12, ey - 14, 24, 28, 6, SSD1306_WHITE);
        display.fillCircle(lx + (eye_dx / 3), ey + (eye_dy / 3), 4, SSD1306_BLACK);
        display.fillCircle(rx + (eye_dx / 3), ey + (eye_dy / 3), 4, SSD1306_BLACK);
    }
    
    display.display();
}

void loop() {
    uint32_t now = millis();
    bool activityDetected = false;
    int eye_dx = 0, eye_dy = 0;

    // Читаем тумблер (LOW = Mute включен)
    is_muted = (digitalRead(MUTE_SWITCH_PIN) == LOW);

    // Отработка двойного щелчка
    if (kill_switch_triggered) {
        kill_switch_triggered = false;
        kill_banner_until_ms = now + 2500;
        Serial.println("[EVENT] Сработал KILL SWITCH! Отправка команды на ПК...");
        // Здесь в будущем будет отправка по WebSocket на ПК
    }

    int mic_peak = getAudioPeak();
    if (mic_peak > AUDIO_WAKE_THRESHOLD) {
        activityDetected = true;
    }

    if (mpu_ok) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);
        
        eye_dx = (int)(a.acceleration.y * -2.0f);
        eye_dy = (int)(a.acceleration.x * -1.5f);
        
        float total_accel = sqrtf(a.acceleration.x*a.acceleration.x + a.acceleration.y*a.acceleration.y + a.acceleration.z*a.acceleration.z);
        if (total_accel > SHAKE_THRESHOLD) {
            activityDetected = true;
        }
    }

    if (activityDetected) {
        lastActivityTime = now;
        if (currentState == ASLEEP) {
            currentState = AWAKE;
            Serial.println("[MALVIC] Проснулся!");
        }
    }

    if (currentState == AWAKE && (now - lastActivityTime > AWAKE_DURATION)) {
        currentState = ASLEEP;
        Serial.println("[MALVIC] Заснул...");
    }

    // Отрисовка
    drawScreen(eye_dx, eye_dy, (now < kill_banner_until_ms));
    
    if (currentState == ASLEEP) {
        delay(30);
    }
}