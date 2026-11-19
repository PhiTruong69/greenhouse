# Gateway core (T08)

Chương trình Python chạy trên PC hoặc Raspberry Pi. Bridge được cắm vào máy này qua USB.

- Đọc telemetry JSON từ **Bridge** qua USB Serial, gửi lên **CoreIOT** dưới tên sub-device `Node1-Garden`.
- Nhận **RPC `setPump`** từ CoreIOT (dashboard hoặc RuleChain), chuyển xuống Bridge, Bridge gửi tiếp tới **Node 1** qua ESP-NOW, rồi trả kết quả về CoreIOT.
- Báo node **online/offline**: tự tạo sub-device khi gateway connect, và báo offline nếu node im lặng quá `node_timeout_s` giây.

```
Node 1 ──ESP-NOW──▶ Bridge ──USB Serial (JSON)──▶ Gateway (Python) ──MQTT Gateway API──▶ CoreIOT
       ◀──────────        ◀─────────────────────                   ◀── RPC setPump ─────
```

## Cấu trúc

| File | Việc |
|---|---|
| `main.py` | Ghép Bridge với CoreIOT: chuyển telemetry, xử lý RPC, theo dõi timeout |
| `bridge_serial.py` | Mở cổng Serial, đọc/gửi từng dòng JSON, tự mở lại cổng khi rút/cắm Bridge |
| `coreiot.py` | MQTT Gateway API (`tb-mqtt-client`): connect/disconnect sub-device, telemetry, RPC |
| `config.example.json` | Mẫu cấu hình. Copy thành `config.json`, file này không đưa lên git |
| `gateway.service` | systemd service để chạy tự động trên Raspberry Pi |

Gateway dùng 3 thread:
- thread đọc Serial (`on_bridge_message`);
- thread MQTT của thư viện (`on_rpc`);
- thread chính, kiểm tra timeout mỗi giây.

Dữ liệu dùng chung giữa các thread được bảo vệ bằng `lock`.

## Cài đặt CoreIOT

1. Vào **Devices → + Add device**, đặt tên ví dụ `Greenhouse-Gateway`, và **tích "Is gateway"**.
2. Copy access token của device gateway này. Lưu ý: đây là token của gateway, không phải token của Node 2.
3. Không cần tạo `Node1-Garden` bằng tay. CoreIOT tự tạo sub-device này lần đầu gateway gửi telemetry, kèm relation từ gateway tới nó.

## Cấu hình (`config.json`)

```json
{
  "serial_port": "COM21",
  "serial_baud": 115200,
  "coreiot_host": "app.coreiot.io",
  "coreiot_port": 1883,
  "gateway_token": "TOKEN_CUA_GATEWAY",
  "device_type": "default",
  "nodes": { "1": "Node1-Garden" },
  "node_timeout_s": 60
}
```

| Khoá | Ý nghĩa |
|---|---|
| `serial_port` | Cổng của Bridge. Windows: `COMxx`. Pi: `/dev/ttyACM0` hoặc đường dẫn `/dev/serial/by-id/...` (ổn định hơn) |
| `device_type` | Device profile CoreIOT gán cho sub-device khi tự tạo |
| `nodes` | Ánh xạ node id trong ESP-NOW sang tên sub-device trên CoreIOT |
| `node_timeout_s` | Không nhận telemetry quá thời gian này thì báo node offline (Node 1 gửi mỗi 10 s) |

## Chạy trên PC (Windows)

```bash
cd Gateway
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
copy config.example.json config.json   # rồi điền token, cổng COM
python main.py
```

Phải đóng Serial Monitor của PlatformIO trước khi chạy, vì mỗi cổng COM chỉ một chương trình mở được.

Ví dụ log:

```
INFO    coreiot  mqtt connected (rc=0)
INFO    bridge   opened COM21
INFO    gateway  bridge boot, mac 34:85:18:xx:xx:xx, channel 1
INFO    coreiot  Node1-Garden online
INFO    coreiot  telemetry Node1-Garden {'temperature': 27.3, 'humidity': 60.1, 'soil': 47, 'soil_raw': 2100, 'pump': 0}
INFO    coreiot  rpc Node1-Garden {'id': 1, 'method': 'setPump', 'params': {'value': True, 'duration': 5}}
INFO    bridge   -> {"cmd":"setPump","node":1,"value":1,"duration":5,"id":1}
INFO    coreiot  rpc reply Node1-Garden #1 {'success': True}
```

## RPC hỗ trợ (cho TV5 làm dashboard và RuleChain)

| Method | params | Phản hồi |
|---|---|---|
| `setPump` | `true`/`false`, `1`/`0`, hoặc `{"value": true, "duration": 30}` | `{"success": true}` hoặc `{"success": false, "error": "..."}` |
| `getPump` | không cần | `true`/`false`, theo telemetry gần nhất |

- `duration` tính bằng giây. Giá trị `0` hoặc bỏ trống thì node dùng giới hạn an toàn 60 s.
- Các lỗi có thể gặp:
  - `busy`: Bridge đang xử lý lệnh trước.
  - `unknown node`: Bridge chưa nhận telemetry nào từ node nên chưa biết MAC.
  - `node did not ACK`: node không trả lời sau 3 lần gửi lại.
  - `bridge not connected`: Gateway chưa mở được cổng Serial.
  - `bridge timeout`: Bridge không phản hồi trong 5 s.

**Thử nhanh:** mở `Node1-Garden` trên CoreIOT, thêm widget **RPC debug terminal** vào một dashboard, rồi gõ:

```
setPump {"value": true, "duration": 5}
```

Widget **Switch control** dùng method `setPump` để bật/tắt và `getPump` để đọc trạng thái.

## Chạy trên Raspberry Pi

**Bridge phải cắm USB vào Pi.** Gateway đọc dữ liệu từ Bridge qua cổng Serial, nên Bridge luôn phải nối trực tiếp với máy chạy Gateway. Cổng USB của Pi cũng cấp nguồn cho Bridge. Node 1 và Node 2 không cần nối gì với Pi.

```bash
# trên Pi
sudo usermod -aG dialout $USER        # quyền mở cổng Serial, đăng xuất rồi đăng nhập lại
ls /dev/serial/by-id/                 # tìm cổng của Bridge
cd ~/greenhouse-iot/Gateway
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
cp config.example.json config.json    # điền token, serial_port = /dev/serial/by-id/...
.venv/bin/python main.py              # chạy thử

# chạy tự động khi Pi khởi động (sửa User/đường dẫn trong file nếu khác)
sudo cp gateway.service /etc/systemd/system/
sudo systemctl enable --now gateway
journalctl -u gateway -f              # xem log
```

## Chưa làm (phần còn lại của T15)

- **Gửi bù khi mất mạng:** lưu telemetry vào SQLite khi mất kết nối CoreIOT, có mạng lại thì gửi kèm `ts` gốc. Hiện tại, telemetry nhận được trong lúc mất mạng sẽ bị mất.
- **Đồng bộ ngưỡng (attribute)** `soil_min`, `temp_max` từ CoreIOT xuống gateway.
