#include <Arduino.h>
#include "sensor.h"
#include "config.h"

const uint8_t channel              = 0x18;
const uint8_t broadcast_head[]     = {0xFF, 0xFF, 0x18};

const uint8_t NODE_ADDL[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}; // 子機個体番号
const uint8_t NUM_NODES            = sizeof(NODE_ADDL) / sizeof(NODE_ADDL[0]);

const uint32_t CYCLE_MS        = 600000UL; // 10分周期
const uint32_t MEASURE_WAIT_MS = 15000;     // ブロードキャスト後の計測完了待ち
const uint32_t REPLY_TIMEOUT_MS = 10000;   // 子機返信タイムアウト
int n=0;
int label = 0x01;

void setup() {
    pinMode(M0, OUTPUT);
    pinMode(M1, OUTPUT);
    pinMode(AUX_PIN, INPUT);
    digitalWrite(M0, HIGH);
    digitalWrite(M1, LOW); // Mode1: WOR送信モード
    Serial1.begin(9600, SERIAL_8N1, MY_RX, MY_TX);
    Serial.begin(115200);
    unsigned long st = millis();
    delay(1000);
    while (digitalRead(AUX_PIN) == LOW && millis() - st < 3000);
}

void loop() {
    unsigned long cycle_start = millis();

    // ① ブロードキャスト計測コマンド（全子機同時計測）
    Serial.print("Sending broadcast measurement command...");
    while (digitalRead(AUX_PIN) == LOW);
    Serial1.write(broadcast_head, sizeof(broadcast_head));
    Serial1.write(label);
    Serial1.flush();

    Serial.print("label: ");
    Serial.println(label);

    label++;
    if (label > 0x10) label = 0x01;

    // ② 計測完了待ち
    delay(MEASURE_WAIT_MS);

    // ③ 子機ごとにデータ送信コマンドを送り返信を受け取る
    for (uint8_t i = 0; i < NUM_NODES; i++) {
        uint8_t node_id = NODE_ADDL[i];
        uint8_t req_head[] = {0x00, node_id, channel};

        while (Serial1.available()) Serial1.read(); // 余剰バイトクリア
        while (digitalRead(AUX_PIN) == LOW);

        Serial1.write(req_head, sizeof(req_head));
        Serial1.write(0x00);
        Serial1.flush();
        delay(500);

        // 返信待ち: data2, data1, data(最新) の順に int16_t x 4 = 8バイト x 3 = 24バイト
        int16_t recv[12] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
            unsigned long st = millis();
            while (millis() - st < REPLY_TIMEOUT_MS) {
                if (Serial1.available() >= 24) {
                    Serial1.readBytes((uint8_t*)recv, 24);
                    break;
                }
            }
        // recv[0-3]=data2, [4-7]=data1, [8-11]=data(最新)

        while (Serial1.available()) Serial1.read(); // 余剰バイトクリア

        // {個体番号, label, temp*10, hum*10, globe*10}
        Serial.print("---node ");     Serial.println(node_id);

        Serial.print(node_id);   Serial.print(", ");
        Serial.print(recv[0]);   Serial.print(", "); // label
        Serial.print(recv[1]);   Serial.print(", "); // temp*10
        Serial.print(recv[2]);   Serial.print(", "); // hum*10
        Serial.println(recv[3]);                     // globe*10
        
        Serial.print(node_id);   Serial.print(", ");
        Serial.print(recv[4]);   Serial.print(", "); // label
        Serial.print(recv[5]);   Serial.print(", "); // temp*10
        Serial.print(recv[6]);   Serial.print(", "); // hum*10
        Serial.println(recv[7]);                     // globe*10

        Serial.print(node_id);   Serial.print(", ");
        Serial.print(recv[8]);   Serial.print(", "); // label
        Serial.print(recv[9]);   Serial.print(", "); // temp*10
        Serial.print(recv[10]);  Serial.print(", "); // hum*10
        Serial.println(recv[11]);                    // globe*10

        delay(1000); // 次の子機へのコマンド前にAUX安定待ち
    }

    // ④ 残り時間待機して10分周期を維持
    // millis()オーバーフロー対応: 符号なし減算で正しく動作
    // シリアル入力があれば即次サイクルへ
    n++;
    while (millis() - cycle_start < CYCLE_MS) {
        if ( n < 5 )break;
        if (Serial.available()) {
            while (Serial.available()) Serial.read(); // 入力を捨てる
            Serial.println("Manual trigger.");
            break;
        }
        delay(10);
    }
}
