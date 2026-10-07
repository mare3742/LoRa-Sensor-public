# LoRa 環境モニタ

LoRa（E220-900T22S-JP）で複数の子機から気温・湿度・黒球温度を集め、
WBGT（暑さ指数）を計算して平面図上に表示するシステム。

```
[ 子機 koki × N ] --LoRa--> [ 親機 oya ] --USB--> [ ラズパイ/PC ]
                                                    serial_to_csv.py → data.csv → server.py → ブラウザ
  └───────── firmware/ ─────────┘                    └──────────────── pi/ ────────────────┘
```

このリポジトリは **ソフトウェア側（ファームウェアとラズパイ/PC 側）** だけを置いている。
筐体・配線・全体の組み立て手順は別リポジトリにまとめる予定（準備中）。
<!-- docs リポジトリを作ったら、ここに URL を入れる -->

## どこを読めばいいか

やりたいことの行を選んで、リンク先に進む。

| やりたいこと | 読むもの |
|---|---|
| マイコン（親機・子機）にプログラムを書き込む | [firmware/README.md](firmware/README.md) |
| 書き込みでエラーが出た / `pio` が見つからない | [firmware/README.md の「困ったとき」](firmware/README.md#困ったとき) |
| ピン配置・通信の仕組みを知りたい | [firmware/README.md の「リファレンス」](firmware/README.md#リファレンス) |
| 親機のデータを CSV に保存する・画面に表示する | [pi/README.md](pi/README.md) |
| センサなしで画面だけ試す | [pi/README.md の「まず試す」](pi/README.md#まず試す) |

## フォルダ構成

| パス | 中身 |
|---|---|
| [`firmware/`](firmware/) | 親機・子機・E220 設定ツールのファームウェア（PlatformIO / Arduino / Seeed XIAO ESP32S3） |
| [`pi/`](pi/) | ラズパイ（または PC）で動かす側。USB シリアルの収集 `serial_to_csv.py`、表示サーバ `server.py`、画面 `web/` |

`firmware/` と `pi/` は同じリポジトリで管理している。親機が USB に出す文字列の形式と、
`pi/serial_to_csv.py` がそれを読む処理がセットになっているため、片方を変えたらもう片方も直すこと。
