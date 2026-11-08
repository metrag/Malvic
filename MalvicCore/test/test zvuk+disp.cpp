#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <driver/i2s.h>

// --- Распиновка SPI (Дисплей) ---
#define OLED_MOSI   11
#define OLED_CLK    12
#define OLED_DC     13
#define OLED_CS     10
#define OLED_RESET  1

// --- Распиновка I2S_0 (Микрофон MS3625) ---
#define I2S_MIC_PORT I2S_NUM_0
#define MIC_WS_PIN   4
#define MIC_SCK_PIN  5
#define MIC_SD_PIN   6

Adafruit_SSD1306 display(128, 64, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

void setupI2SMic() {
    // Включаем подтяжку линии данных к земле
    gpio_set_pull_mode((gpio_num_t)MIC_SD_PIN, GPIO_PULLDOWN_ONLY);
    
    i2s_config_t mic_cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // MS3625 отдает 24 бита
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
    display.begin(SSD1306_SWITCHCAPVCC);
    setupI2SMic();
}

void loop() {
    int32_t raw_samples[128];
    size_t bytes_read = 0;
    
    // Читаем блок звука
    i2s_read(I2S_MIC_PORT, raw_samples, sizeof(raw_samples), &bytes_read, portMAX_DELAY);
    int samples_read = bytes_read / sizeof(int32_t);

    display.clearDisplay();
    
    // Заголовок
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("MIC OSCILLOSCOPE");

    int last_y = 32;
    int max_amp = 0;

    // Отрисовка звуковой волны
    for (int i = 0; i < samples_read; i++) {
        // Сдвиг 32-битного кадра в 16-битный сэмпл
        int16_t sample = (int16_t)(raw_samples[i] >> 14); 
        
        if (abs(sample) > max_amp) max_amp = abs(sample);

        // Масштабируем сигнал под высоту экрана (центр = 32)
        int y = 32 - (sample / 300); 
        y = constrain(y, 10, 54); // Ограничиваем, чтобы график не ломал интерфейс

        if (i > 0) {
            display.drawLine(i - 1, last_y, i, y, SSD1306_WHITE);
        }
        last_y = y;
    }

    // VU-метр (индикатор максимальной громкости)
    display.drawRect(0, 56, 128, 8, SSD1306_WHITE);
    int bar_w = map(max_amp, 0, 15000, 0, 124);
    bar_w = constrain(bar_w, 0, 124);
    display.fillRect(2, 58, bar_w, 4, SSD1306_WHITE);

    display.display();
}