#include <Arduino.h>
#include "config.h"

// --- 通信設定 ---
const uint8_t PARENT_ADDH = 0x11;
const uint8_t PARENT_ADDL = 0x92;
const uint8_t channel = 0x18;

const uint8_t rate = 0x62;

uint8_t n = 0x01;
unsigned long lastCheckTime = 0;

// 'o' = 親機モード, 'k' = 子機モード, 0 = 未選択
char selectedMode = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);

    // 設定モード(M0=1, M1=1)へ
    pinMode(M0, OUTPUT);
    pinMode(M1, OUTPUT);
    digitalWrite(M0, HIGH);
    digitalWrite(M1, HIGH);

    Serial1.begin(9600, SERIAL_8N1, MY_RX, MY_TX);

    Serial.println("--- E220 Status Checker & Configurator ---");
    Serial.println("Select mode: 'o' = OYA (親機), 'k' = KOKI (子機)");
}

void loop() {
    // モード未選択時: キー入力待ち
    if (selectedMode == 0) {
        if (!Serial.available()) return;
        char c = Serial.read();
        while (Serial.available()) Serial.read();
        if (c == 'o') {
            selectedMode = 'o';
            Serial.println(">>> OYA mode selected <<<");
        } else if (c == 'k') {
            selectedMode = 'k';
            Serial.println(">>> KOKI mode selected <<<");
        }
        return;
    }

    unsigned long currentTime = millis();

    // 5秒ごとにチェック
    if (currentTime - lastCheckTime >= 5000) {
        lastCheckTime = currentTime;

        while (Serial1.available()) {
            Serial1.read();
        }

        uint8_t readCmd[] = {0xC1, 0x00, 0x08};
        Serial1.write(readCmd, sizeof(readCmd));

        delay(500); // モジュールの返答待ち
        uint8_t configCmd[]  = {0xC0, 0x00, 0x08, 0x00, n, rate, 0x01, 0x18, 0x40, 0x04, 0xA8};
        uint8_t configCmd2[] = {0xC0, 0x09, 0x01, 0x42};
        uint8_t configCmd3[] = {0xC0, 0x0A, 0x01, 0x00};

        if (selectedMode == 'o') {
            // --- 親機モード ---
            configCmd[3] = PARENT_ADDH;
            configCmd[4] = PARENT_ADDL;
        }
        if (Serial1.available()) {
            uint8_t res[32] = {0};
            int len = 0;
            while (Serial1.available() && len < sizeof(res)) {
                res[len++] = Serial1.read();
            }len--;

            if (memcmp(&configCmd[3], &res[3], 1) == 0
                && (res[1+3] == n-1 || selectedMode == 'o')
                && memcmp(&configCmd[2+3], &res[2+3], 4) == 0
                ) {
                Serial.println(">Module_Status:2");
                Serial.println("Message: [READY] Module is already set.");
            } else {
                Serial.println(">Module_Status:1");
                Serial.println(n);
                Serial1.write(configCmd, sizeof(configCmd));
                delay(500);
                Serial1.write(configCmd2, sizeof(configCmd2));
                delay(500);
                Serial1.write(configCmd3, sizeof(configCmd3));
                delay(500);
                uint8_t res2[32] = {0};
                int len2 = 0;
                while (Serial1.available() && len2 < sizeof(res2)) {
                    res2[len2++] = Serial1.read();
                }len2--;
                Serial.printf("feedback:");
                for(int i=0; i<=len2; i++) Serial.printf("%d:%02X ",i, res2[i]); Serial.println("");
                n++;
            }
        } else {
            Serial.println(">Module_Status:0");
            Serial.println("Message: [NOT_CONNECTED] No response. Check wiring/power.");
        }

        Serial.println("------------------------------------------");
    }
}
