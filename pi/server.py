"""環境モニタの表示サーバ。

data.csv（時刻,node,temp,hum,globe）と site.json を配信するだけ。
シリアルポートには触らないので、収集スクリプトと独立して動く。
標準ライブラリのみで動作する。

    python3 server.py
    python3 server.py --dummy   センサなしで動かす（data.csv は読まない・書かない）

    http://localhost:8001/          表示画面
    http://localhost:8001/editor    配置エディタ
"""
import os
import re
import sys
import json
import random
from math import atan, sqrt, sin
from pathlib import Path
from datetime import datetime
from collections import defaultdict
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

# ===== 設定 =====
HTTP_PORT   = 8001          # 表示サーバの待ち受けポート
HISTORY_MAX = 30            # 1ノードあたりグラフに渡す件数
NAN_LIMIT   = 200.0         # 子機の計測失敗値 0x7FFF(=3276.7) を弾く閾値
TAIL_BYTES  = 512 * 1024    # CSVは末尾だけ読む（肥大しても速度が落ちない）
TS_FORMAT   = '%Y-%m-%d %H:%M:%S'
MAX_UPLOAD  = 24 * 1024 * 1024

ROOT      = Path(__file__).resolve().parent   # このファイルのあるフォルダ（pi/）
WEB_DIR   = ROOT / 'web'                      # viewer/editor/site.json/maps の置き場
CSV_FILE  = Path(os.environ.get('CSV_FILE', ROOT / 'data.csv'))
SITE_FILE = WEB_DIR / 'site.json'
MAP_DIR   = WEB_DIR / 'maps'

IMAGE_TYPES = {'.png': 'image/png',  '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg',
               '.gif': 'image/gif',  '.webp': 'image/webp', '.svg': 'image/svg+xml'}

DUMMY       = False         # --dummy で立つ。立っている間 CSV は一切見ない
DUMMY_STEP  = 30            # ダミーの計測間隔（秒）
DUMMY_NODES = list(range(1, 27))   # site.json にノードが1つもないときに出す番号（1〜26）
# ================


def calculate_wbgt(t, h, tg):
    """firmware/src/function.cpp の calculateWBGT と同じ式（Stull近似 + 0.7Tw + 0.3Tg）"""
    tw = (t * atan(0.151977 * sqrt(h + 8.313659))
          + atan(t + h)
          - atan(h - 1.676331)
          + 0.00391838 * (h ** 1.5) * atan(0.023101 * h)
          - 4.686035)
    return 0.7 * tw + 0.3 * tg


def _clean(v):
    """計測失敗（0x7FFF → 3276.7）を None に落とす"""
    return None if abs(v) > NAN_LIMIT else round(v, 1)


def read_tail_lines():
    """CSVの末尾だけを読む。先頭の欠けた行は捨てる。"""
    size = CSV_FILE.stat().st_size
    with open(CSV_FILE, 'rb') as f:
        if size > TAIL_BYTES:
            f.seek(size - TAIL_BYTES)
            f.readline()
        blob = f.read()
    return blob.decode('utf-8-sig', errors='ignore').splitlines()


def parse_csv():
    """data.csv → {node: [{at,t,h,g,wbgt}, ...]}（計測時刻順）"""
    seen = defaultdict(dict)          # node -> {計測時刻: rec}
    for line in read_tail_lines():
        parts = line.strip().split(',')
        if len(parts) != 5:
            continue
        try:
            at, node = parts[0], int(parts[1])
            t, h, g = (_clean(float(v)) for v in parts[2:5])
        except ValueError:
            continue                  # ヘッダ行や壊れた行はここで弾かれる
        wbgt = None if None in (t, h, g) else round(calculate_wbgt(t, h, g), 1)
        # 子機は3世代分を毎回送るので、同じ計測時刻は同一計測として上書き
        seen[node][at] = {'at': at, 't': t, 'h': h, 'g': g, 'wbgt': wbgt}
    return {node: sorted(d.values(), key=lambda r: r['at'])[-HISTORY_MAX:]
            for node, d in seen.items()}


class Cache:
    """CSVが更新されたときだけ読み直す"""

    def __init__(self):
        self.stamp = None
        self.data = {}

    def get(self):
        try:
            st = CSV_FILE.stat()
        except FileNotFoundError:
            return {}
        stamp = (st.st_mtime, st.st_size)
        if stamp != self.stamp:
            self.data = parse_csv()
            self.stamp = stamp
        return self.data


cache = Cache()


# ---------- ダミーデータ ----------

def dummy_rows(node, now):
    """1ノード分の履歴をつくる。番号と時刻が同じなら必ず同じ値になる（グラフが暴れない）"""
    end = int(now.timestamp() // DUMMY_STEP) * DUMMY_STEP     # 直近の計測時刻に丸める
    base = 23.0 + (node % 5) * 2.6                            # 番号ごとに暑さの水準を散らす
    rows = []
    for i in range(HISTORY_MAX - 1, -1, -1):
        ts = end - i * DUMMY_STEP
        rnd = random.Random(node * 1000003 + ts)              # 時刻を種にするので毎回同じ揺れ
        swing = sin(ts / 900.0 + node)                        # 15分ほどの周期でゆっくり上下
        t = base + swing * 1.5 + rnd.uniform(-0.3, 0.3)
        h = 52.0 + sin(ts / 1200.0 + node * 2) * 8.0 + rnd.uniform(-1.0, 1.0)
        g = t + 2.0 + (node % 3) * 3.0 + swing * 2.0 + rnd.uniform(-0.5, 0.5)
        rows.append({'at': datetime.fromtimestamp(ts).strftime(TS_FORMAT),
                     't': round(t, 1), 'h': round(h, 1), 'g': round(g, 1),
                     'wbgt': round(calculate_wbgt(t, h, g), 1)})
    return rows


def dummy_data():
    """site.json に置かれているノード全部に値を出す。未配置なら DUMMY_NODES を出す"""
    ids = [n['id'] for n in load_site().get('nodes', []) if isinstance(n.get('id'), int)]
    now = datetime.now()
    return {node: dummy_rows(node, now) for node in (ids or DUMMY_NODES)}


def snapshot():
    now = datetime.now()
    nodes = []
    for node, rows in sorted((dummy_data() if DUMMY else cache.get()).items()):
        latest = dict(rows[-1])
        try:
            latest['age_sec'] = (now - datetime.strptime(latest['at'], TS_FORMAT)).total_seconds()
        except ValueError:
            latest['age_sec'] = 1e9
        latest['at'] = latest['at'][11:]      # 'HH:MM:SS' だけ画面に出す
        nodes.append({
            'id': node,
            'latest': latest,
            'history': [{k: r[k] for k in ('t', 'h', 'g', 'wbgt')} for r in rows],
        })
    return {'nodes': nodes, 'dummy': DUMMY}


# ---------- site.json ----------

EMPTY_SITE = {'title': '環境モニタ', 'floors': [], 'nodes': []}


def load_site():
    try:
        return json.loads(SITE_FILE.read_text(encoding='utf-8'))
    except (FileNotFoundError, ValueError):
        return EMPTY_SITE


def save_site(body):
    site = json.loads(body.decode('utf-8'))          # 壊れたJSONはここで弾く
    if not isinstance(site.get('floors'), list) or not isinstance(site.get('nodes'), list):
        raise ValueError('floors / nodes が配列ではありません')
    tmp = SITE_FILE.with_suffix('.json.tmp')          # 書き込み中に読まれても壊れないように
    tmp.write_text(json.dumps(site, ensure_ascii=False, indent=2), encoding='utf-8')
    tmp.replace(SITE_FILE)
    return {'saved': str(SITE_FILE), 'floors': len(site['floors']), 'nodes': len(site['nodes'])}


def safe_name(name):
    """アップロード名からディレクトリ要素を落として英数字だけにする"""
    return re.sub(r'[^A-Za-z0-9._-]', '_', Path(name).name)


def save_map(name, blob):
    name = safe_name(name)
    ext = Path(name).suffix.lower()
    if ext not in IMAGE_TYPES:
        raise ValueError(f'扱えない拡張子です: {ext}')
    MAP_DIR.mkdir(exist_ok=True)
    (MAP_DIR / name).write_bytes(blob)
    return {'path': f'maps/{name}'}


# ---------- HTTP ----------

class Handler(BaseHTTPRequestHandler):
    def _send(self, code, ctype, body):
        self.send_response(code)
        self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(body)

    def _json(self, obj, code=200):
        self._send(code, 'application/json; charset=utf-8',
                   json.dumps(obj, ensure_ascii=False).encode('utf-8'))

    def _html(self, name):
        self._send(200, 'text/html; charset=utf-8', (WEB_DIR / name).read_bytes())

    def do_GET(self):
        path = self.path.split('?')[0]
        try:
            if path == '/api/latest':
                self._json(snapshot())
            elif path == '/api/site':
                self._json(load_site())
            elif path.startswith('/maps/'):
                f = MAP_DIR / safe_name(path)
                if f.is_file():
                    self._send(200, IMAGE_TYPES.get(f.suffix.lower(), 'application/octet-stream'),
                               f.read_bytes())
                else:
                    self._send(404, 'text/plain; charset=utf-8', b'no map')
            elif path in ('/editor', '/editor.html'):
                self._html('editor.html')
            elif path in ('/', '/index.html', '/viewer.html'):
                self._html('viewer.html')
            else:
                self._send(404, 'text/plain; charset=utf-8', b'not found')
        except ConnectionError:
            pass        # ブラウザを閉じた／リロードしただけ

    def do_POST(self):
        path = self.path.split('?')[0]
        try:
            n = int(self.headers.get('Content-Length') or 0)
        except ValueError:
            n = 0
        if not 0 < n <= MAX_UPLOAD:
            return self._json({'error': 'データ長が不正です'}, 413)
        try:
            body = self.rfile.read(n)
            if path == '/api/site':
                self._json(save_site(body))
            elif path.startswith('/api/map/'):
                self._json(save_map(path[len('/api/map/'):], body))
            else:
                self._json({'error': 'not found'}, 404)
        except ConnectionError:
            pass
        except Exception as e:
            self._json({'error': str(e)}, 400)

    def handle_error(self, request, client_address):
        pass            # 接続断でトレースバックを出さない

    def log_message(self, *args):
        pass            # アクセスログでコンソールを埋めない


def main():
    global DUMMY
    DUMMY = '--dummy' in sys.argv[1:] or os.environ.get('DUMMY') == '1'

    if DUMMY:
        print("ダミーデータモード: data.csv は読みません（書き込みもしません）")
    else:
        print(f"読み込むCSV: {CSV_FILE}")
    print(f"配置定義    : {SITE_FILE}")
    print(f"表示画面    : http://localhost:{HTTP_PORT}/")
    print(f"配置エディタ: http://localhost:{HTTP_PORT}/editor")
    try:
        ThreadingHTTPServer(('0.0.0.0', HTTP_PORT), Handler).serve_forever()
    except KeyboardInterrupt:
        print("終了します")


if __name__ == '__main__':
    main()
