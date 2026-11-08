#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <driver/i2s.h>
#include <math.h>

// --- Распиновка SPI (Дисплей) ---
#define OLED_MOSI   11
#define OLED_CLK    12
#define OLED_DC     13
#define OLED_CS     10
#define OLED_RESET  1

// --- Распиновка I2C (MPU6050) ---
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

// --- Распиновка I2S_1 (Усилитель MAX98357A) ---
#define I2S_SPK_PORT I2S_NUM_1
#define SPK_BCLK_PIN 3
#define SPK_LRC_PIN  2
#define SPK_DIN_PIN  7

#define SAMPLE_RATE 16000
#define SAFE_VOL    0.15f  // Ограничение громкости 15% при питании от USB

Adafruit_SSD1306 display(128, 64, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);
Adafruit_MPU6050 mpu;
bool mpu_ok = false;
uint32_t dizzy_until_ms = 0;

// Инициализация шины I2S для вывода звука
void setupI2SSpeaker() {
    i2s_config_t spk_cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t spk_pins = {
        .bck_io_num = SPK_BCLK_PIN,
        .ws_io_num = SPK_LRC_PIN,
        .data_out_num = SPK_DIN_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(I2S_SPK_PORT, &spk_cfg, 0, NULL);
    i2s_set_pin(I2S_SPK_PORT, &spk_pins);
}

// Функция генерации чистого тона
void playTone(float freq_hz, int duration_ms) {
    int total_samples = (SAMPLE_RATE * duration_ms) / 1000;
    int16_t buf[128];
    size_t bytes_written = 0;
    int16_t max_amp = (int16_t)(32767.0f * SAFE_VOL);

    for (int i = 0; i < total_samples; i += 128) {
        int chunk = min(128, total_samples - i);
        for (int j = 0; j < chunk; j++) {
            float phase = 2.0f * M_PI * freq_hz * (i + j) / SAMPLE_RATE;
            buf[j] = (int16_t)(sinf(phase) * max_amp);
        }
        i2s_write(I2S_SPK_PORT, buf, chunk * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    }
    // Тишина в конце
    memset(buf, 0, sizeof(buf));
    i2s_write(I2S_SPK_PORT, buf, sizeof(buf), &bytes_written, portMAX_DELAY);
}

void drawEyes(int offset_x, int offset_y, bool is_dizzy) {
    display.clearDisplay();
    int lx = 40 + constrain(offset_x, -12, 12);
    int rx = 88 + constrain(offset_x, -12, 12);
    int ey = 32 + constrain(offset_y, -8, 8);

    if (is_dizzy) {
        display.drawLine(lx - 10, ey - 10, lx + 10, ey + 10, SSD1306_WHITE);
        display.drawLine(lx - 10, ey + 10, lx + 10, ey - 10, SSD1306_WHITE);
        display.drawLine(rx - 10, ey - 10, rx + 10, ey + 10, SSD1306_WHITE);
        display.drawLine(rx - 10, ey + 10, rx + 10, ey - 10, SSD1306_WHITE);
    } else {
        display.fillRoundRect(lx - 12, ey - 14, 24, 28, 6, SSD1306_WHITE);
        display.fillRoundRect(rx - 12, ey - 14, 24, 28, 6, SSD1306_WHITE);
        display.fillCircle(lx + (offset_x / 3), ey + (offset_y / 3), 4, SSD1306_BLACK);
        display.fillCircle(rx + (offset_x / 3), ey + (offset_y / 3), 4, SSD1306_BLACK);
    }
    display.display();
}

void setup() {
    Serial.begin(115200);
    
    // Инициализация систем
    display.begin(SSD1306_SWITCHCAPVCC);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 400000);
    mpu_ok = mpu.begin(0x68, &Wire);
    
    if (mpu_ok) {
        mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    }

    setupI2SSpeaker();

    // Приветственный звук R2-D2 при загрузке
    playTone(587.33f, 100);  // Ре
    delay(50);
    playTone(880.00f, 150);  // Ля
}

void loop() {
    uint32_t now = millis();
    int eye_dx = 0, eye_dy = 0;

    if (mpu_ok) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);

        eye_dx = (int)(a.acceleration.y * -2.0f); 
        eye_dy = (int)(a.acceleration.x * -1.5f);

        float total_accel = sqrtf(a.acceleration.x * a.acceleration.x +
                                  a.acceleration.y * a.acceleration.y +
                                  a.acceleration.z * a.acceleration.z);

        if (total_accel > 20.0f && now > dizzy_until_ms) {
            dizzy_until_ms = now + 2000;
            // Возмущенный писк при тряске
            playTone(320.0f, 100);
            playTone(220.0f, 150);
        }
    }

    drawEyes(eye_dx, eye_dy, (now < dizzy_until_ms));
}