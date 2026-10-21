"""Local-only NS-3 dashboard server. Python standard library; no pip needed."""
import json
import subprocess
import threading
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

NS3_DIR = Path.home() / 'ns-3-dev'
DASHBOARD_DIR = Path(__file__).resolve().parent
LOCK = threading.Lock()

class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(DASHBOARD_DIR), **kwargs)

    def send_json(self, status, data):
        body = json.dumps(data).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        if urlparse(self.path).path != '/api/run':
            return self.send_json(404, {'error': 'Unknown endpoint'})
        try:
            size = int(self.headers.get('Content-Length', '0'))
            if size < 2 or size > 1024:
                return self.send_json(400, {'error': 'Invalid request size'})
            payload = json.loads(self.rfile.read(size))
            value = payload.get('extraRateKbps')
            if isinstance(value, bool) or not isinstance(value, (int, float)) or not (0 <= value <= 800):
                return self.send_json(400, {'error': 'Traffic must be between 0 and 800 Kbps'})
            if not LOCK.acquire(blocking=False):
                return self.send_json(409, {'error': 'Another simulation is already running'})
            try:
                cmd = ['./ns3', 'run', f'smart-traffic-ips-live --extraRateKbps={value:g}']
                result = subprocess.run(cmd, cwd=NS3_DIR, capture_output=True, text=True, timeout=180)
                if result.returncode != 0:
                    return self.send_json(500, {'error': 'NS-3 failed. See Ubuntu terminal. ' + (result.stderr or result.stdout)[-900:]})
                path = NS3_DIR / 'dashboard-results.json'
                data = json.loads(path.read_text(encoding='utf-8'))
                if abs(float(data.get('extraRateKbps', -1)) - value) > 0.01:
                    return self.send_json(500, {'error': 'NS-3 returned an old JSON result'})
                return self.send_json(200, {'data': data})
            finally:
                LOCK.release()
        except subprocess.TimeoutExpired:
            return self.send_json(504, {'error': 'NS-3 run timed out'})
        except Exception as exc:
            return self.send_json(500, {'error': str(exc)})

if __name__ == '__main__':
    print('Dashboard: http://localhost:8765/Smart_Traffic_IDS_IPS_Live.html', flush=True)
    print('Keep this Ubuntu terminal open. Press Ctrl+C to stop.', flush=True)
    ThreadingHTTPServer(('127.0.0.1', 8765), Handler).serve_forever()
