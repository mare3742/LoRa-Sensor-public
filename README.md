# LoRa 環境モニタ

📄 **[概要スライド（PDF）](概要.pdf)**

LoRa（E220-900T22S-JP）で複数の子機から気温・湿度・黒球温度を集め、
WBGT（暑さ指数）を計算して平面図上に表示するシステム。

```
┌──── 子機 ─────┐       ┌─────────────────── 親機 ───────────────────────┐
[ ESP(koki) × N ]-LoRa->[ ESP(oya) ]-USB->[ PC(ラズパイ) ]-->[ モニタ表示 ]
└──────────── firmware/ ──────────────┘└───────────── pi/ ───────────────┘
```

LoRa通信にはE220モジュールを使用する~[設定必須](firmware/README.md)~
このリポジトリは **ソフトウェア側（ファームウェアとラズパイ/PC 側）** だけを置いている。
基板・筐体は [LoRa-Sensor-hardware](https://github.com/mare3742/LoRa-Sensor-hardware)。

## どこを読めばいいか

| やりたいこと | 読むもの |
|---|---|
| マイコン（親機・子機）にプログラムを書き込む | [firmware/README.md](firmware/README.md) |
| 親機のデータを CSV に保存する・画面に表示する | [pi/README.md](pi/README.md) |

| 試したい・困った・調べたいとき | 読むもの |
|---|---|
| センサなしで画面だけ試す | [pi/README.md の「まず試す」](pi/README.md#まず試す) |
| 書き込みでエラーが出た / `pio` が見つからない | [firmware/README.md の「困ったとき」](firmware/README.md#困ったとき) |
| ピン配置・通信の仕組みを知りたい | [firmware/README.md の「リファレンス」](firmware/README.md#リファレンス) |

