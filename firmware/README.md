# firmware — 親機・子機のファームウェア

Seeed XIAO ESP32S3 に書き込むプログラム。1つのプロジェクトに3種類が入っていて、書き込むときに選ぶ。

| 名前（`-e` に渡す） | 役割 | 書き込む先 |
|---|---|---|
| `koki` | 子機。センサを読んで LoRa で送る | 現場に置く各センサ |
| `oya` | 親機。子機に計測を指示してデータを集め、USB に出す | ラズパイ/PC につなぐ 1 台 |
| `set` | E220（LoRa モジュール）に親機用 / 子機用の設定を書き込むツール | 設定したい E220 をつないだマイコン（使い方は[下記](#e220-の設定set)） |

## 書き込み手順

### 0. PlatformIO を入れる

[PlatformIO](https://platformio.org/) が必要。Python が入っていれば次の 1 行で入る。

```sh
pip install -U platformio
```

`pio --version` と打ってバージョンが出れば OK。`pio` が見つからないと言われたら → [困ったとき](#pio-が見つからない)。

### 1. 書き込む

マイコンを USB でつないで、リポジトリの**ルート**で次を実行する。書き込みたいものの行だけ実行すればよい。

```sh
pio run -d firmware -e oya  -t upload   # 親機
pio run -d firmware -e koki -t upload   # 子機
pio run -d firmware -e set  -t upload   # E220設定ツール
```

- `-d firmware` … `platformio.ini` が `firmware/` の中にあるので、その場所を教える（`cd firmware` してから `-d` なしで実行してもよい）。
- `-e` … **必ず付ける**。付けないと 3 種類を全部ビルドしに行く。
- 最後に `[SUCCESS]` と出れば書き込み完了。

初回だけ、ツールチェーンとライブラリが自動でダウンロードされる（約 1GB、数分かかる）。先に何かを入れる必要はない。

VS Code の PlatformIO 拡張を使うときは、リポジトリ直下ではなく **`firmware/` フォルダを開く**。直下を開くと `platformio.ini` が見つからず、プロジェクトとして認識されない。

### E220 の設定（`set`）

`set` を書き込んだあとは、シリアルモニタ（**115200**）を開くと選択待ちになる。

```sh
pio device monitor -b 115200
```

- `o` を入力 → 親機用の設定を E220 に書き込む
- `k` を入力 → 子機用の設定を E220 に書き込む

設定が終わったら、`oya` または `koki` を書き込み直す。ポート一覧は `pio device list`。

## 困ったとき

### ポートが見つからない・書き込みに失敗する

**BOOT ボタンを押したまま RESET ボタンを押す**と、書き込み用のモード（ブートローダ）に入る。その状態でもう一度書き込みを実行する。

### `pio` が見つからない

`pio: command not found` や `'pio' is not recognized` と出る場合は、次の 2 通りのどちらか。`pio` と `platformio` は同じコマンドなので、どちらで書いてもよい。

まず、どちらの状態か判別する。

```sh
python -m platformio --version
```

- **バージョンが出た** → 下の「A. pip で入れたが PATH に無い」
- **`No module named platformio`** → 下の「B. VS Code 拡張で入れた」

**A. pip で入れたが PATH に無い**

`pio` の代わりに `python -m platformio` と書けばよい。

```sh
python -m platformio run -d firmware -e oya -t upload
```

**B. VS Code 拡張で入れた**

拡張が `~/.platformio/penv/` に専用の仮想環境を作るので、普通の `python` には platformio が入っていない。フルパスで呼ぶ。

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -d firmware -e oya -t upload
```

```bash
"$HOME/.platformio/penv/bin/pio" run -d firmware -e oya -t upload
```

**恒久的に直したいとき** — VS Code のコマンドパレットから `PlatformIO: New Terminal` を開く（PATH が通った状態で起動する）。または `penv/Scripts`（Linux/macOS は `penv/bin`）を PATH に追加する。

### ビルドのたびに違う版が入る

`platformio.ini` の `platform = espressif32` はバージョンを固定していないので、環境によって入る版が変わる。動作確認済みの組み合わせは **espressif32 6.12.0 / framework-arduinoespressif32 3.20017**。揃えたいときは `platformio.ini` を `platform = espressif32@6.12.0` に書き換える。

## リファレンス

### ピン配置（XIAO ESP32S3）

| ピン | 用途 |
|---|---|
| D0 / D7 | E220 UART（TX / RX） |
| D1 | MOSFET 経由のセンサ電源（LOW で ON） |
| D2 / D3 | E220 M0 / M1 |
| D4 / D5 | SHT35 用 I2C（SDA / SCL） |
| D8 | DS18B20 OneWire（黒球温度） |
| D10 | E220 AUX（ディープスリープからの復帰信号） |

### 動作の流れ

- 子機は E220 の WOR 受信で待機し、親機からのパケットで AUX が LOW になって復帰する。
- 親機は 10 分周期で全子機に計測を指示し、順に 3 世代分のデータを回収する。
- 親機は回収したデータを USB シリアル（115200bps）に出す。これを [`pi/serial_to_csv.py`](../pi/serial_to_csv.py) が読む。**出力の形式を変えたら `serial_to_csv.py` の `parse_line` も直すこと。**
