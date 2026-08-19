import serial
import csv
import os
import time
from datetime import datetime, timedelta
from collections import defaultdict, deque

# ===== 設定 =====
SERIAL_PORT = 'COM13'       # シリアルポート (例: '/dev/ttyUSB0' on Linux/Mac)
BAUD_RATE = 115200
# リポジトリ直下の data.csv。カレントディレクトリに依存しない
OUTPUT_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data.csv')
RECONNECT_INTERVAL = 5     # 再接続試行間隔（秒）
TS_FORMAT = '%Y-%m-%d %H:%M:%S'   # web/demo/server.py・web/monitor/server.py と揃えること
# ================

def save_to_csv(rows):
    file_exists = os.path.exists(OUTPUT_FILE)
    try:
        with open(OUTPUT_FILE, 'a', newline='', encoding='utf-8-sig') as f:
            writer = csv.writer(f)
            if not file_exists:
                writer.writerow(['timestamp', 'node', 'temp', 'hum', 'globe'])
            writer.writerows(rows)
        print(f"保存完了: {len(rows)}行 → {OUTPUT_FILE}")
    except Exception as e:
        print(f"保存エラー: {e}")

def parse_line(line):
    try:
        parts = line.strip().split(',')
        if len(parts) != 5:
            return None
        device_id = int(parts[0])
        label_data= int(parts[1])
        temp      = int(parts[2]) / 10.0
        hum       = int(parts[3]) / 10.0
        globe     = int(parts[4]) / 10.0
        if label_data < 1 or label_data > 16: return None
        return device_id, label_data, temp, hum, globe
    except Exception:
        return None
    
def parse_label_line(line):
    try:
        key, value = line.strip().split(':', 1)
        key = key.strip()
        if not key.endswith('label'):
            return None
        return key, int(value.strip())
    except Exception:
        return None

def connect():
    while True:
        try:
            ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
            print(f"接続: {SERIAL_PORT}")
            return ser
        except Exception:
            print(f"接続失敗、{RECONNECT_INTERVAL}秒後に再試行...")
            time.sleep(RECONNECT_INTERVAL)

def main():
    timestamp = [None] * 17  # index 0は未使用、1〜16を使う
    pending_rows = []
    recent = defaultdict(lambda: deque(maxlen=8))  # device_id -> 直近label_dataの履歴
    ser = connect()

    while True:
        try:
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode('utf-8', errors='ignore').strip()
            if not line:
                continue

            if ':' in line:
                result = parse_label_line(line)
                if result:
                    key, label = result
                    timestamp[label] = datetime.now().strftime(TS_FORMAT)
                    continue
            else:
                result = parse_line(line)
                if result is None:
                    continue

                device_id, label_data, temp, hum, globe = result

                dq = recent[device_id]
                if label_data in dq:
                    continue
                dq.append(label_data)
                if timestamp[label_data] is None:
                    continue 

                # label は計測時刻の照合と重複除去にだけ使い、CSVには残さない
                # （5列: 時刻,node,temp,hum,globe = server.py が読む形式）
                row = [timestamp[label_data], device_id, temp, hum, globe]
                save_to_csv([row])
                print(f"受信: {row}")

        except KeyboardInterrupt:
            print("終了します")
            break
        except Exception:
            print("切断、再接続します...")
            try:
                ser.close()
            except Exception:
                pass
            if pending_rows:
                save_to_csv(pending_rows)
                pending_rows = []
            ser = connect()

if __name__ == '__main__':
    main()
