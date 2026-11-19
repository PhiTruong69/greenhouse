"""Kết nối CoreIOT bằng MQTT Gateway API (thư viện tb-mqtt-client).

Gateway là một device loại "gateway" trên CoreIOT. Mỗi node (Node1-Garden, ...) là một
sub-device, CoreIOT tự tạo khi gateway báo connect lần đầu.
"""
import logging
import threading
import time

from tb_gateway_mqtt import TBGatewayMqttClient

log = logging.getLogger("coreiot")


class CoreIotGateway:
    def __init__(self, host, port, token, device_type, on_rpc):
        """on_rpc(device, req_id, method, params) được gọi khi CoreIOT gửi RPC cho một sub-device."""
        self.device_type = device_type
        self.on_rpc = on_rpc
        self._client = TBGatewayMqttClient(host, port, username=token)
        self._client.gw_set_server_side_rpc_request_handler(self._handle_rpc)
        self._online = set()  # sub-device đã báo connect trong phiên MQTT hiện tại
        self._lock = threading.Lock()
        self._host = host

    def start(self):
        """Kết nối MQTT (thử lại tới khi được). Sau đó thư viện tự kết nối lại khi rớt mạng."""
        while True:
            try:
                self._client.connect(callback=self._on_connect)
                log.info("connecting to %s", self._host)
                return
            except OSError as e:
                log.warning("connect failed: %s, retry in 5 s", e)
                time.sleep(5)

    def is_connected(self):
        return self._client.is_connected()

    def set_online(self, device, online):
        """Báo sub-device online (connect) hoặc offline (disconnect). Chỉ gửi khi trạng thái đổi."""
        with self._lock:
            if online == (device in self._online):
                return
            if online:
                self._client.gw_connect_device(device, self.device_type)
                self._online.add(device)
            else:
                self._client.gw_disconnect_device(device)
                self._online.discard(device)
        log.info("%s %s", device, "online" if online else "offline")

    def send_telemetry(self, device, values, ts_ms):
        """Gửi telemetry của sub-device kèm thời điểm đo (ms)."""
        self._client.gw_send_telemetry(device, {"ts": ts_ms, "values": values})
        log.info("telemetry %s %s", device, values)

    def reply_rpc(self, device, req_id, resp):
        """Trả lời một RPC (CoreIOT chờ phản hồi với RPC two-way)."""
        self._client.gw_send_rpc_reply(device, req_id, resp)
        log.info("rpc reply %s #%s %s", device, req_id, resp)

    def _on_connect(self, client, userdata, flags, result_code, *extra):
        """Mỗi lần (kết nối lại): xoá danh sách online để lần telemetry kế tiếp báo connect lại."""
        log.info("mqtt connected (rc=%s)", result_code)
        with self._lock:
            self._online.clear()

    def _handle_rpc(self, client, content):
        """content = {"device": "Node1-Garden", "data": {"id": 1, "method": "setPump", "params": ...}}"""
        device = content.get("device")
        data = content.get("data", {})
        log.info("rpc %s %s", device, data)
        self.on_rpc(device, data.get("id"), data.get("method"), data.get("params"))
