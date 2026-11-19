"""Kết nối USB Serial tới Bridge: đọc từng dòng JSON, gửi lệnh JSON.

Định dạng bản tin xem Firmware/Bridge/README.md.
"""
import json
import logging
import threading
import time

import serial

log = logging.getLogger("bridge")


class BridgeSerial:
    def __init__(self, port, baud, on_message):
        """on_message(dict) được gọi cho mỗi dòng JSON hợp lệ nhận từ Bridge (chạy trong thread đọc)."""
        self.port = port
        self.baud = baud
        self.on_message = on_message
        self._ser = None
        self._write_lock = threading.Lock()

    def start(self):
        """Chạy thread đọc Serial nền. Thread tự mở lại cổng nếu Bridge bị rút ra cắm lại."""
        threading.Thread(target=self._run, name="bridge-serial", daemon=True).start()

    def send(self, msg):
        """Gửi một lệnh (dict) thành một dòng JSON. Trả về False nếu cổng chưa mở hoặc ghi lỗi."""
        line = json.dumps(msg, separators=(",", ":")) + "\n"
        with self._write_lock:
            if self._ser is None:
                return False
            try:
                self._ser.write(line.encode())
                log.info("-> %s", line.strip())
                return True
            except serial.SerialException as e:
                log.warning("write failed: %s", e)
                return False

    def _open(self):
        """Mở cổng mà không kéo DTR/RTS để không làm reset ESP32."""
        ser = serial.Serial()
        ser.port = self.port
        ser.baudrate = self.baud
        ser.timeout = 1
        ser.dtr = False
        ser.rts = False
        ser.open()
        return ser

    def _run(self):
        """Vòng lặp: mở cổng, đọc từng dòng, đổi sang dict và gọi on_message."""
        while True:
            try:
                ser = self._open()
                with self._write_lock:
                    self._ser = ser
                log.info("opened %s", self.port)

                while True:
                    raw = ser.readline()
                    if not raw:
                        continue
                    text = raw.decode(errors="replace").strip()
                    if not text.startswith("{"):
                        log.debug("skip: %s", text)  # log khởi động của ROM, không phải JSON
                        continue
                    try:
                        msg = json.loads(text)
                    except json.JSONDecodeError:
                        log.warning("bad json: %s", text)
                        continue
                    log.debug("<- %s", text)
                    self.on_message(msg)

            except serial.SerialException as e:
                log.warning("%s: %s, retry in 3 s", self.port, e)
                with self._write_lock:
                    if self._ser is not None:
                        self._ser.close()
                    self._ser = None
                time.sleep(3)
