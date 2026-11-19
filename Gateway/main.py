"""Gateway core (T08): Bridge (USB Serial) <-> CoreIOT (MQTT Gateway API).

- Telemetry từ Bridge -> CoreIOT dưới tên sub-device (Node1-Garden).
- RPC setPump từ CoreIOT -> lệnh JSON xuống Bridge -> Node 1; trả kết quả về CoreIOT.
- Node không gửi dữ liệu quá node_timeout_s giây -> báo offline.

Chạy: python main.py  (cấu hình trong config.json)
"""
import itertools
import json
import logging
import threading
import time
from pathlib import Path

from bridge_serial import BridgeSerial
from coreiot import CoreIotGateway

log = logging.getLogger("gateway")

TELEMETRY_KEYS = ("temperature", "humidity", "soil", "soil_raw", "pump")
BRIDGE_TIMEOUT_S = 5  # Bridge luôn trả cmd_result trong ~1,2 s; quá 5 s coi như mất Bridge


def parse_pump_params(params):
    """Đọc params của setPump. Nhận: true/false, 1/0, {"value": .., "duration": ..}.
    Trả về (bật?, số giây)."""
    if isinstance(params, dict):
        value = params.get("value", params.get("enabled", False))
        duration = int(params.get("duration", 0))
    else:
        value, duration = params, 0
    if isinstance(value, str):
        value = value.lower() in ("1", "true", "on")
    return bool(value), duration


class Gateway:
    def __init__(self, cfg):
        self.nodes = {int(k): v for k, v in cfg["nodes"].items()}       # node id -> tên sub-device
        self.node_ids = {v: k for k, v in self.nodes.items()}           # tên sub-device -> node id
        self.node_timeout_s = cfg["node_timeout_s"]

        self.last_seen = {}    # tên sub-device -> thời điểm nhận telemetry gần nhất
        self.pump_state = {}   # tên sub-device -> trạng thái bơm gần nhất (cho getPump)
        self.pending = {}      # id lệnh -> (sub-device, rpc id, thời điểm gửi)
        self.lock = threading.Lock()
        self.cmd_ids = itertools.count(1)

        self.bridge = BridgeSerial(cfg["serial_port"], cfg["serial_baud"], self.on_bridge_message)
        self.cloud = CoreIotGateway(cfg["coreiot_host"], cfg["coreiot_port"], cfg["gateway_token"],
                                    cfg["device_type"], self.on_rpc)

    # ---------- Bridge -> CoreIOT ----------

    def on_bridge_message(self, msg):
        """Xử lý một bản tin JSON từ Bridge (chạy trong thread đọc Serial)."""
        kind = msg.get("type")

        if kind == "boot":
            log.info("bridge boot, mac %s, channel %s", msg.get("mac"), msg.get("channel"))

        elif kind == "telemetry":
            device = self.nodes.get(msg.get("node"))
            if device is None:
                log.warning("telemetry from unknown node %s", msg.get("node"))
                return
            values = {k: msg[k] for k in TELEMETRY_KEYS if k in msg}
            with self.lock:
                self.last_seen[device] = time.time()
                self.pump_state[device] = bool(msg.get("pump"))
            self.cloud.set_online(device, True)
            self.cloud.send_telemetry(device, values, int(time.time() * 1000))

        elif kind in ("cmd_result", "error"):
            with self.lock:
                pend = self.pending.pop(msg.get("id"), None)
            if pend is None:
                log.warning("bridge %s: %s", kind, msg)
                return
            device, req_id, _ = pend
            if kind == "cmd_result" and msg.get("ok"):
                self.cloud.reply_rpc(device, req_id, {"success": True})
            else:
                reason = msg.get("msg", "node did not ACK")
                self.cloud.reply_rpc(device, req_id, {"success": False, "error": reason})

    # ---------- CoreIOT -> Bridge ----------

    def on_rpc(self, device, req_id, method, params):
        """Xử lý RPC cho sub-device (chạy trong thread MQTT)."""
        if method == "getPump":  # widget switch trên dashboard đọc trạng thái
            with self.lock:
                state = self.pump_state.get(device, False)
            self.cloud.reply_rpc(device, req_id, state)
            return

        node = self.node_ids.get(device)
        if method != "setPump" or node is None:
            self.cloud.reply_rpc(device, req_id, {"success": False, "error": f"unsupported {method}"})
            return

        on, duration = parse_pump_params(params)
        cmd_id = next(self.cmd_ids)
        with self.lock:
            self.pending[cmd_id] = (device, req_id, time.time())
        cmd = {"cmd": "setPump", "node": node, "value": int(on), "duration": duration, "id": cmd_id}
        if not self.bridge.send(cmd):
            with self.lock:
                self.pending.pop(cmd_id, None)
            self.cloud.reply_rpc(device, req_id, {"success": False, "error": "bridge not connected"})

    # ---------- Theo dõi định kỳ ----------

    def check_timeouts(self):
        """Báo offline cho node im lặng quá lâu; trả lỗi cho lệnh Bridge không phản hồi."""
        now = time.time()
        with self.lock:
            silent = [d for d, t in self.last_seen.items() if now - t > self.node_timeout_s]
            stale = [i for i, (_, _, t) in self.pending.items() if now - t > BRIDGE_TIMEOUT_S]
            stale = [(i, self.pending.pop(i)) for i in stale]

        for device in silent:
            self.cloud.set_online(device, False)
        for _, (device, req_id, _) in stale:
            self.cloud.reply_rpc(device, req_id, {"success": False, "error": "bridge timeout"})

    def run(self):
        """Khởi động kết nối CoreIOT, thread đọc Bridge, rồi kiểm tra timeout mỗi giây."""
        self.cloud.start()
        self.bridge.start()
        while True:
            time.sleep(1)
            self.check_timeouts()


def main():
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)-7s %(name)-8s %(message)s")
    cfg = json.loads((Path(__file__).parent / "config.json").read_text(encoding="utf-8"))
    try:
        Gateway(cfg).run()
    except KeyboardInterrupt:
        log.info("stopped")


if __name__ == "__main__":
    main()
