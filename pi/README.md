# pi — 収集と表示（ラズパイ / PC 側）

親機（`oya`）が USB に出すデータを CSV に溜めて、ブラウザで見られるようにする側。
ラズパイでも普通の PC でも動く。

| ファイル | 役割 |
|---|---|
| `serial_to_csv.py` | 親機の USB シリアルを読んで `data.csv` に追記する |
| `server.py` | `data.csv` を読んで画面を配信する Web サーバ |
| `web/` | 表示画面 `viewer.html`、配置エディタ `editor.html`、配置の定義 `site.json` |

2 つのスクリプトは独立している。収集を止めても、溜まったデータの表示はできる。

以降のコマンドは、**リポジトリのルートから `cd pi` した状態**で実行する。

```sh
cd pi
```

## まず試す

センサも親機も無くても、画面だけ確認できる。

```sh
python server.py --dummy
```

ブラウザで http://localhost:8001/ を開いて、ダミーの値が表示されれば OK。
止めるときは `Ctrl+C`。`--dummy` の間は `data.csv` を読みも書きもしない。

## 本番の手順

親機を書き込み済み（[firmware/README.md](../firmware/README.md)）で、USB でこの PC につないであるとする。

### 1. 収集を始める

`pyserial` が必要。

```sh
pip install pyserial
```

親機がつながっているポート名を調べる。

```sh
python -m serial.tools.list_ports
```

`serial_to_csv.py` の先頭の `SERIAL_PORT` を、出てきたポート名（Windows なら `COM13` など）に書き換えてから実行する。

```sh
python serial_to_csv.py
```

`接続: COM13` と出て、しばらくして `受信: [...]` が流れ始めれば成功。計測は 10 分周期なので、最初のデータが来るまで待つ。
`接続失敗、5秒後に再試行...` が続くときは、ポート名が違うか、他のソフト（シリアルモニタなど）がポートを掴んでいる。

### 2. 画面を見る

別のターミナルを開いて実行する。

```sh
python server.py
```

| URL | 内容 |
|---|---|
| http://localhost:8001/ | 表示画面 |
| http://localhost:8001/editor | 配置エディタ |

標準ライブラリだけで動くので、追加のインストールは要らない。

## 配置エディタ

`/editor` で、フロア・部屋・ノード（子機）の位置を編集して `web/site.json` に保存する。
平面図の画像をアップロードすると `web/maps/` に置かれる。この中身は施設情報を含みうるので、Git の追跡対象外にしている。

同梱の `web/site.json` は架空の工場のサンプル。

## 公開範囲の注意

`server.py` は同じネットワークの他の端末（スマホなど）からも見られるよう、既定で全インターフェース（`0.0.0.0`）で待ち受ける。
認証は無いので、**同じネットワークの誰でも `/editor` から配置や平面図を書き換えられる**。信頼できないネットワークでは使わないこと。
自分の PC だけで見るなら、環境変数 `HOST` で待ち受けを絞れる。

```sh
HOST=127.0.0.1 python server.py
```

## データの置き場所

`data.csv` は `pi/` の中に作られる（Git の追跡対象外）。列は `timestamp, node, temp, hum, globe`。

CSV の場所を変えたいときは、環境変数 `CSV_FILE` でパスを渡す（`server.py` だけが対応している）。

```sh
CSV_FILE=/path/to/data.csv python server.py
```
