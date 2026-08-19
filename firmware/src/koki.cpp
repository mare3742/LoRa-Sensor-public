#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "sensor.h"
#include "config.h"

// --- 通信設定 ---
const uint8_t PARENT_ADDH = 0x11;
const uint8_t PARENT_ADDL = 0x92;
const uint8_t channel = 0x18;
const uint8_t head[] = {PARENT_ADDH, PARENT_ADDL, channel};

// RTC保持変数
RTC_DATA_ATTR bool    initial_done = false;

// data[0]=label, [1]=temp*10, [2]=hum*10, [3]=globe*10
RTC_DATA_ATTR int16_t data[4]      = {0};
RTC_DATA_ATTR int16_t data1[4]     = {0};
RTC_DATA_ATTR int16_t data2[4]     = {0};

void setup() {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0);
    gpio_hold_dis((gpio_num_t)MY_TX);
    Serial1.begin(9600, SERIAL_8N1, MY_RX, MY_TX);
    delay(20);

    if (!initial_done) {
        delay(1000);

        pinMode(M0, OUTPUT);
        pinMode(M1, OUTPUT);
        digitalWrite(M0, LOW);
        digitalWrite(M1, HIGH); // WOR受信モード
        gpio_hold_en((gpio_num_t)M0);
        gpio_hold_en((gpio_num_t)M1);

        while (Serial1.available()) Serial1.read();
        delay(200);
        initial_done = true;
        
    } else {
        // AUXウェイクアップ後の受信処理
        unsigned long ST = millis();
        while (millis() - ST < 5000) {
            if (Serial1.available()) {
                uint8_t com = 0xFF;
                while (Serial1.available()) {
                    com = Serial1.read();
                    if (com != 0xFF) break;
                }
                while (Serial1.available()) Serial1.read();
                
                if (com == 0x00) {
                    // --- データ送信 ---
                    gpio_hold_dis((gpio_num_t)M1);
                    pinMode(AUX_PIN, INPUT);
                    pinMode(M1, OUTPUT);
                    digitalWrite(M1, LOW); // Mode0（送受信）
                    lsp_AUX(200);

                    ST = millis();
                    while (digitalRead(AUX_PIN) == LOW) {
                        if (millis() - ST > 5000) break;
                    }
                    lsp(20);

                    Serial1.write((uint8_t*)head, sizeof(head));
                    Serial1.write((uint8_t*)data2, sizeof(data2));
                    Serial1.write((uint8_t*)data1, sizeof(data1));
                    Serial1.write((uint8_t*)data,  sizeof(data));
                    Serial1.flush();

                    lsp_AUX(200);

                    while (digitalRead(AUX_PIN) == LOW) {
                        if (millis() - ST > 5000) break;
                    }
                    digitalWrite(M1, HIGH); // WOR受信モードに戻す
                    gpio_hold_en((gpio_num_t)M1);
                }else if (com != 0xFF) {
                    // --- センサー計測 & カウントアップ ---
                    gpio_hold_dis((gpio_num_t)FET_PIN);
                    pinMode(FET_PIN, OUTPUT);
                    digitalWrite(FET_PIN, LOW); // センサー電源ON
                    lsp(500);

                    Wire.begin(SDA1, SCL1, 100000);
                    OneWire oneWire(DS_DATA);
                    DallasTemperature ds_sensor(&oneWire);
                    ds_sensor.begin();
                    lsp(500);

                    float t = NAN, h = NAN;
                    readSHT35(Wire, t, h);

                    ds_sensor.requestTemperatures();
                    float tg = ds_sensor.getTempCByIndex(0);

                    memcpy(data2, data1, sizeof(data1));
                    memcpy(data1, data, sizeof(data));

                    data[0] = com;
                    data[1] = isnan(t)  ? 0x7FFF : (int16_t)(t  * 10.0f);
                    data[2] = isnan(h)  ? 0x7FFF : (int16_t)(h  * 10.0f);
                    data[3] = isnan(tg) ? 0x7FFF : (int16_t)(tg * 10.0f);

                    gpio_hold_dis((gpio_num_t)FET_PIN);
                    digitalWrite(FET_PIN, HIGH); // センサー電源OFF
                    gpio_hold_en((gpio_num_t)FET_PIN);
                } 
                break;
            }
        }
    }
    Serial1.end();
    pinMode(MY_TX, OUTPUT);
    digitalWrite(MY_TX, HIGH);
    gpio_hold_en((gpio_num_t)MY_TX);
    delay(200);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)AUX_PIN, 0);
    gpio_deep_sleep_hold_en();
    esp_deep_sleep_start();
}

void loop() {
}