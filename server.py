from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
import json
import threading
import time
import subprocess
# ===== CẤU HÌNH =====

ROOT = Path(__file__).resolve().parent.parent
PROTOTYPE = ROOT / "prototype"
BRIDGE_EXE = ROOT / "bridge" / "bridge.exe"
SLOT_COUNT = 5
SPRAY_DURATION = 0.7

# ===== TRẠNG THÁI MÔ PHỎNG =====

state_lock = threading.Lock()
spray_lock = threading.Lock()

active_slot = None
busy_until = 0.0

remaining = {
    slot: 100
    for slot in range(1, SLOT_COUNT + 1)
}


# ===== HTTP SERVER =====

class ScentStationHandler(SimpleHTTPRequestHandler):

    def __init__(self, *args, **kwargs):
        super().__init__(
            *args,
            directory=str(PROTOTYPE),
            **kwargs
        )

    def send_json(self, status, data):
        body = json.dumps(data).encode("utf-8")

        self.send_response(status)
        self.send_header(
            "Content-Type",
            "application/json; charset=utf-8"
        )
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    # ===== GET =====

    def do_GET(self):

        if self.path == "/api/status":

            with state_lock:
                data = {
                    "system": "ScentStation",
                    "status": "ONLINE",
                    "slot_count": SLOT_COUNT,
                    "backend": "Python",
                    "cpp_connected": False,
                    "active_slot": active_slot,
                    "remaining": remaining.copy()
                }

            self.send_json(200, data)
            return

        super().do_GET()

    # ===== POST =====

    def do_POST(self):
        global active_slot, busy_until
        print("DEBUG POST PATH:", repr(self.path), flush=True)
        if self.path == "/api/refill":
            print("DEBUG REFILL ENTERED", flush=True)
            try:
                length = int(self.headers.get("Content-Length", "0"))
                data = json.loads(self.rfile.read(length))
                slot = data.get("slot")

                if type(slot) is not int or slot not in remaining:
                    self.send_json(400, {"ok": False, "error": "BAD_SLOT"})
                    return

                with spray_lock:
                    with state_lock:
                        remaining[slot] = 100

                self.send_json(200, {
                    "ok": True,
                    "slot": slot,
                    "remaining": 100
                })
            except (ValueError, TypeError):
                self.send_json(400, {"ok": False, "error": "BAD_BODY"})
            return
        if self.path != "/api/spray":
            self.send_json(
                404,
                {"ok": False, "error": "NOT_FOUND"}
            )
            return

        # ===== KIỂM TRA DỮ LIỆU =====

        try:
            content_length = int(
                self.headers.get("Content-Length", "0")
            )

            if content_length <= 0 or content_length > 4096:
                self.send_json(
                    400,
                    {"ok": False, "error": "BAD_BODY"}
                )
                return

            data = json.loads(
                self.rfile.read(content_length)
            )

            if not isinstance(data, dict):
                raise ValueError("Expected JSON object")

            slot = data.get("slot")

            if (
                type(slot) is not int
                or slot not in remaining
            ):
                self.send_json(
                    400,
                    {"ok": False, "error": "BAD_SLOT"}
                )
                return

        except (ValueError, TypeError, UnicodeDecodeError):
            self.send_json(
                400,
                {"ok": False, "error": "BAD_REQUEST"}
            )
            return

        # ===== XỬ LÝ PHUN =====
        # Nếu máy đang phun, yêu cầu mới chờ.
        # Không trả BUSY ngay như phiên bản cũ.

        with spray_lock:

            with state_lock:

                if remaining[slot] <= 0:
                    status = 409
                    result = {
                        "ok": False,
                        "error": "EMPTY",
                        "slot": slot
                    }

                else:
                    active_slot = slot
                    busy_until = time.monotonic() + SPRAY_DURATION

                    status = 200
                    result = {
                        "ok": True,
                        "slot": slot,
                        "remaining": remaining[slot],
                        "message": "SPRAY_ACCEPTED",
                        "duration_ms": int(
                            SPRAY_DURATION * 1000
                        )
                    }

            # Mô phỏng thời gian phun.
            # Chỉ một yêu cầu được chạy trong khối này.

            
            if status == 200:
                try:
                    completed = subprocess.run(
                        [str(BRIDGE_EXE), str(slot)],
                        capture_output=True,
                        text=True,
                        timeout=10,
                        cwd=str(ROOT)
                    )

                    if completed.returncode != 0 or "Result: OK" not in completed.stdout:
                        status = 502
                        result = {
                            "ok": False,
                            "error": "CORE_FAILED"
                        }
                    else:
                        with state_lock:
                            remaining[slot] -= 1
                            result["remaining"] = remaining[slot]

                except (OSError, subprocess.TimeoutExpired):
                    status = 502
                    result = {
                        "ok": False,
                        "error": "CORE_UNAVAILABLE"
                    }

                finally:
                    with state_lock:
                        active_slot = None
                        busy_until = 0.0


                    self.send_json(status, result)


# ===== KHỞI ĐỘNG =====

if __name__ == "__main__":

    server = ThreadingHTTPServer(
        ("127.0.0.1", 8000),
        ScentStationHandler
    )

    print("=" * 40)
    print("ScentStation Backend Running!")
    print("Slots:", SLOT_COUNT)
    print("URL: http://127.0.0.1:8000")
    print("API: /api/status")
    print("API: /api/spray")
    print("=" * 40)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping ScentStation...")
    finally:
        server.server_close()
