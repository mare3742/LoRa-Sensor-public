# LoRa 環境モニタ

LoRa（E220-900T22S-JP）で複数の子機から気温・湿度・黒球温度を集め、
WBGT を計算して平面図上に表示するシステム。

```
[ 子機 koki × N ] --LoRa--> [ 親機 oya ] --USB serial--> serial_to_csv.py --> data.csv --> server.py --> ブラウザ
```

## 構成

| パス | 中身 |
|---|---|
| `firmware/` | 親機・子機・E220設定ツールのファームウェア（PlatformIO / Arduino / Seeed XIAO ESP32S3） |
| `serial_to_csv.py` | 親機のUSBシリアルを読んで `data.csv` に追記する |
| `server.py` | `data.csv` を読んで表示する Web サーバ |
| `web/` | 表示画面 `viewer.html` / 配置エディタ `editor.html` / 配置定義 `site.json` |

`data.csv` はリポジトリ直下に作られる（追跡対象外）。

## ファームウェアの書き込み

[PlatformIO](https://platformio.org/) が必要。env ごとにコンパイル対象を絞っているので `-e` は必須。

```sh
pio run -d firmware -e oya  -t upload   # 親機
pio run -d firmware -e koki -t upload   # 子機
pio run -d firmware -e set  -t upload   # E220設定ツール
```

シリアルモニタは **115200** で開く（`platformio.ini` の `monitor_speed = 9600` は LoRa モジュール側の速度）。

```sh
pio device monitor -b 115200
```

VS Code の PlatformIO 拡張を使う場合は `firmware/` をワークスペースとして開くこと。

## 計測データの収集

`pyserial` が必要。`serial_to_csv.py` の `SERIAL_PORT` を実際のポート名に書き換えてから実行する。

```sh
pip install pyserial
python serial_to_csv.py
```

## 表示

標準ライブラリのみで動く。

```sh
python server.py            # http://localhost:8001/       表示画面
python server.py --dummy    # センサ無しでダミー表示（data.csv を読まない）
```

`http://localhost:8001/editor` が配置エディタ。フロア・部屋・ノード位置を編集して `web/site.json` に保存する。
平面図画像をアップロードすると `web/maps/` に置かれる。

同梱の `site.json` は架空の工場のサンプル。

## センサ・ピン配置

| ピン | 用途 |
|---|---|
| D0 / D7 | E220 UART (TX / RX) |
| D1 | MOSFET経由センサー電源（LOW で ON） |
| D2 / D3 | E220 M0 / M1 |
| D4 / D5 | SHT35 用 I2C (SDA / SCL) |
| D8 | DS18B20 OneWire（黒球温度） |
| D10 | E220 AUX（ディープスリープからの復帰信号） |

子機は E220 の WOR 受信で待機し、親機からのパケットで AUX が LOW になって復帰する。
親機は 10 分周期で全子機に計測を指示し、順に 3 世代分のデータを回収する。
