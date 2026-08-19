#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"
// --- 通信設定 ---
const uint8_t PARENT_ADDH = 0x11; // 親機アドレス高位（適宜変更）
const uint8_t PARENT_ADDL = 0x92; // 親機アドレス低位（適宜変更）

const int TIME_TO_SLEEP = 20;
const uint64_t uS_TO_S_FACTOR = 1000000ULL;

void lsp(uint32_t ms) {//[ms]だけLightSleepする関数
    esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000);
    esp_light_sleep_start();
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
}

void lsp_AUX(uint32_t ms) {//AUXがlow or [ms] LightSleepする関数
    esp_sleep_enable_ext0_wakeup((gpio_num_t)AUX_PIN, 0);
    esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000);
    esp_light_sleep_start();
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    delay(20);
}

bool readSHT35(TwoWire &w, float &t, float &h) {// SHT35 読み取り関数
    uint8_t addr = 0x45;
    w.beginTransmission(addr);
    w.write(0x24); w.write(0x00);
    if (w.endTransmission() != 0) return false;
    delay(20); 
    if (w.requestFrom(addr, (uint8_t)6) == 6) {
        uint16_t t_raw = (w.read() << 8) | w.read(); w.read();
        uint16_t h_raw = (w.read() << 8) | w.read(); w.read();
        t = -45.0 + 175.0 * (float)t_raw / 65535.0;
        h = 100.0 * (float)h_raw / 65535.0;
        return true;
    }
    return false;
}

float calculateWBGT(float t, float h, float tg) {// WBGT計算関数
    float tw = t * atan(0.151977 * sqrt(h + 8.313659)) + atan(t + h) - atan(h - 1.676331) + 0.00391838 * pow(h, 1.5) * atan(0.023101 * h) - 4.686035;
    return 0.7 * tw + 0.3 * tg;
}

// 銀行家の丸め（round half to even）×10 → SHT35用
int16_t bankersRound10(float val) {
    float scaled = val * 10.0f;
    float base = floorf(scaled);
    float diff = scaled - base;
    if (diff < 0.5f) return (int16_t)base;
    if (diff > 0.5f) return (int16_t)base + 1;
    // ちょうど0.5: 偶数方向へ
    int16_t b = (int16_t)base;
    return (b % 2 == 0) ? b : b + 1;
}

// 四捨五入×10 → DS18B20用
int16_t round10(float val) {
    return (int16_t)roundf(val * 10.0f);
}

bool waitSerial(Stream &serialRef, uint32_t timeoutMs) {
    uint32_t startMs = millis();
    while (!serialRef.available()) {
        if (millis() - startMs >= timeoutMs) {
            return false;
        }
        delay(1);
    }
    delay(20);
    return true;
}