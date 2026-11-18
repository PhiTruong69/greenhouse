# Bridge – ESP-NOW ⇄ USB Serial

Firmware cho NodeMCU ESP32-S3 (N16R8) cắm USB vào PC chạy Gateway Python (mã T06):

- Nhận telemetry ESP-NOW từ các node, tự trả ACK, rồi in ra Serial thành **một dòng JSON**.
- Nhận lệnh JSON từ Gateway, gửi xuống node qua ESP-NOW, chờ ACK, gửi lại nếu cần.
- Không kết nối WiFi.

## Cấu trúc thư mục

```
Bridge/
├─ src/main.cpp              khởi tạo, tạo queue và 2 task FreeRTOS
├─ include/bridge_config.h   baud, số node tối đa, thời gian chờ ACK
├─ lib/
│  ├─ BridgeLink/            ESP-NOW: nhận telemetry, ACK, gửi lệnh có gửi lại
│  └─ HostLink/              Serial JSON: đọc lệnh, in telemetry, kết quả lệnh
└─ ../common/protocol.h      định dạng gói ESP-NOW, dùng chung với Node
```

Thư viện ngoài duy nhất là `ArduinoJson` 7. PlatformIO tự tải thư viện này qua `lib_deps`.

## Task FreeRTOS

Mỗi phần cứng chỉ do **một** task dùng. Chỉ `hostTask` ghi Serial, nên các dòng JSON không bao giờ bị lẫn vào nhau. Cả hai task chạy trên core 1. `loop()` không dùng tới và tự xoá.

```
           cmdQueue (HostCommand)
hostTask ─────────────────────────▶ linkTask
(Serial) ◀───────────────────────── (ESP-NOW)
           eventQueue (LinkEvent)
```

| Task | Ưu tiên | Phần cứng | Việc |
|---|---|---|---|
| `linkTask` | 3 | ESP-NOW | Nhận telemetry, trả ACK, gửi lệnh, gửi lại lệnh, tạo sự kiện |
| `hostTask` | 2 | USB Serial | Đọc lệnh JSON, in telemetry, `cmd_result` và `error` |

`hostTask` cần stack 6 KB vì dùng ArduinoJson. Các thông số này chỉnh trong mục FreeRTOS của `bridge_config.h`.

## Build và nạp

```bash
pio run                 # build
pio run -t upload       # nạp
pio device monitor      # xem JSON, 115200
```

Board có 2 cổng USB-C. Với `-DARDUINO_USB_CDC_ON_BOOT=1`, JSON đi ra cổng **USB** (native), Gateway phải mở cổng COM này. Nếu muốn dùng cổng **UART** (COM qua chip USB-UART), xoá dòng đó trong `platformio.ini`.

## Giao thức Serial (cho Gateway – TV4)

Mỗi bản tin là một dòng JSON kết thúc bằng `\n`, tốc độ 115200.

### Bridge → PC

| `type` | Ví dụ | Khi nào |
|---|---|---|
| `boot` | `{"type":"boot","mac":"34:85:18:..","channel":1}` | Bridge vừa khởi động |
| `telemetry` | `{"type":"telemetry","node":1,"seq":12,"temperature":27.3,"humidity":65.1,"soil_raw":2100,"soil":47,"pump":0}` | Có dữ liệu mới từ node (đã lọc gói trùng) |
| `cmd_result` | `{"type":"cmd_result","node":1,"id":42,"ok":true,"result":0}` | Node đã ACK lệnh (`ok:true`), hoặc hết lần gửi lại (`ok:false`) |
| `error` | `{"type":"error","id":42,"msg":"unknown node"}` | Lệnh không gửi được |

- `temperature` và `humidity` **không có** trong bản tin khi node đọc SHT20 lỗi.
- `result`: `0` = OK, `1` = node không hỗ trợ lệnh.
- `msg` của `error` có thể là:
  - `invalid json`: dòng gửi xuống không phải JSON hợp lệ.
  - `unknown cmd`: tên lệnh không được hỗ trợ.
  - `unknown node`: node chưa gửi telemetry lần nào nên Bridge chưa biết MAC.
  - `busy`: lệnh trước chưa xong, thường mất dưới 1,2 s.

### PC → Bridge

```json
{"cmd":"setPump","node":1,"value":1,"duration":30,"id":42}
```

| Trường | Ý nghĩa |
|---|---|
| `cmd` | Hiện chỉ có `setPump` |
| `node` | Mã node đích |
| `value` | `1`/`true` bật, `0`/`false` tắt |
| `duration` | Số giây bơm chạy. `0` hoặc bỏ trống thì node dùng giới hạn tối đa (60 s) |
| `id` | Mã yêu cầu do Gateway đặt (ví dụ RPC id của CoreIOT), được trả lại trong `cmd_result` |

Mỗi lúc Bridge chỉ xử lý một lệnh. Gateway nên chờ nhận `cmd_result` rồi mới gửi lệnh tiếp theo.

## Thử nhanh không cần Gateway

1. Nạp Node 1 và Bridge, cắm Bridge vào PC.
2. Mở `pio device monitor`. Sau khoảng 10 s sẽ thấy dòng `telemetry` của node 1.
3. Gõ dòng sau rồi nhấn Enter:
   ```
   {"cmd":"setPump","node":1,"value":1,"duration":5,"id":1}
   ```
   Relay bơm bật khoảng 5 s. Bridge in ra `cmd_result` với `ok:true`, sau đó là `telemetry` với `pump:1`.

## Cấu hình (`include/bridge_config.h`)

| Hằng số | Mặc định | Ý nghĩa |
|---|---|---|
| `HOST_BAUD` | 115200 | Tốc độ Serial tới PC |
| `MAX_NODES` | 4 | nodeId hợp lệ từ 1 đến 4 |
| `CMD_ACK_TIMEOUT_MS`, `CMD_MAX_RETRIES` | 300, 3 | Thời gian chờ ACK và số lần gửi lại lệnh |

Kênh ESP-NOW và phiên bản giao thức nằm trong `../common/protocol.h`, phải giống nhau giữa Node và Bridge.
