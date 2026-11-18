# Node 1 – Vườn (Sensor node)

Firmware cho ESP32-S3 DevKitC-1 (N16R8) gắn tại cây trong vườn:

- Đọc nhiệt độ, độ ẩm không khí từ **SHT20 RS485** qua Modbus RTU (module TTL-RS485 tự đảo chiều **XY-485**), mã T02.
- Đọc **cảm biến độ ẩm đất điện dung v2.0** (analog).
- Điều khiển **relay bơm**.
- Gửi dữ liệu lên Bridge qua **ESP-NOW** (không kết nối WiFi), hai chiều có ACK, mã T06.

## Cấu trúc thư mục

```
Sensor_node/
├─ src/main.cpp            khởi tạo module, tạo queue và 3 task FreeRTOS
├─ include/node_config.h   chân, địa chỉ Modbus, hiệu chỉnh, chu kỳ – chỉ sửa ở đây
├─ lib/
│  ├─ SHT20/               đọc SHT20 RS485 qua Modbus RTU
│  ├─ SoilSensor/          đọc ADC, đổi sang % độ ẩm đất
│  ├─ Pump/                relay bơm, tự tắt sau PUMP_MAX_ON_MS
│  └─ NodeLink/            ESP-NOW: gửi telemetry, chờ ACK, nhận lệnh
└─ ../common/protocol.h    định dạng gói ESP-NOW, dùng chung với Bridge
```

Mỗi file `.h` trong `lib/` ghi chú công dụng của từng hàm.

## Task FreeRTOS

Mỗi phần cứng chỉ do **một** task dùng, nên không cần mutex. Các task trao đổi qua queue và task notification. Tất cả chạy trên core 1, còn core 0 dành cho WiFi/ESP-NOW. `loop()` không dùng tới và tự xoá.

```
             telemetryQueue              pumpQueue
sensorTask ──────────────────▶ linkTask ───────────▶ pumpTask
     ▲                          (ESP-NOW)            (relay)
     └──────────── xTaskNotifyGive (bơm đổi trạng thái) ───┘
```

| Task | Ưu tiên | Phần cứng | Việc |
|---|---|---|---|
| `linkTask` | 3 | ESP-NOW | Gửi telemetry, chờ ACK, gửi lại, nhận lệnh, trả ACK. Thức dậy tối đa mỗi 10 ms |
| `pumpTask` | 2 | Relay | Thực hiện lệnh bơm, tự tắt khi hết giờ, báo `sensorTask` gửi ngay |
| `sensorTask` | 1 | Serial1 (RS485), ADC | Đọc cảm biến mỗi 10 s hoặc khi được báo, đẩy vào `telemetryQueue` |

- `telemetryQueue` dài 1 và dùng `xQueueOverwrite`, nên chỉ giữ số liệu mới nhất.
- `sensorTask` có ưu tiên thấp nhất vì đọc Modbus có thể chặn tới 200 ms.
- Stack, ưu tiên và core chỉnh trong mục FreeRTOS của `node_config.h`.

## Đấu nối

| Thiết bị | Chân thiết bị | ESP32-S3 (GPIO) |
|---|---|---|
| XY-485 | TXD | GPIO18 (`PIN_RS485_RX`) |
| XY-485 | RXD | GPIO17 (`PIN_RS485_TX`) |
| XY-485 | VCC / GND | 3V3 / GND |
| XY-485 ↔ SHT20 | A+ / B− | A+ / B− của SHT20 |
| SHT20 RS485 | nguồn | 5–30 V DC (theo nhãn cảm biến), GND chung |
| Cảm biến đất v2.0 | AOUT | GPIO1 (`PIN_SOIL`, ADC1) |
| Cảm biến đất v2.0 | VCC / GND | 3V3 / GND |
| Relay bơm | IN | GPIO5 (`PIN_PUMP`) |
| Bơm 5 V | | nguồn sạc USB 5 V **riêng**, chỉ nối chung GND |

> Có thể đổi chân trong `include/node_config.h`. Tránh GPIO35–37 (PSRAM octal của bản N16R8), GPIO19–20 (USB) và các chân strapping 0, 3, 45, 46.
> Chân cảm biến đất phải là chân **ADC1**, vì ADC2 không dùng được khi radio đang bật.

## Cấu hình (`include/node_config.h`)

| Hằng số | Mặc định | Ý nghĩa |
|---|---|---|
| `NODE_ID` | 1 | Mã node, Gateway dùng để đặt tên `Node1-Garden` |
| `PUMP_ON_LEVEL` | `HIGH` | Đổi thành `LOW` nếu relay kích mức thấp |
| `SHT20_MODBUS_ADDR`, `SHT20_MODBUS_BAUD` | 1, 9600 | Địa chỉ và tốc độ của SHT20 |
| `SHT20_MODBUS_FUNC` | `0x04` | Mã hàm đọc: `0x04` (input register) hoặc `0x03` (holding register), xem tài liệu module |
| `SHT20_REG_TEMP`, `SHT20_REG_HUM` | `0x0001`, `0x0002` | Địa chỉ thanh ghi nhiệt độ và độ ẩm (giá trị x10) |
| `SOIL_RAW_DRY`, `SOIL_RAW_WET` | 2900, 1200 | Hiệu chỉnh cảm biến đất (xem bên dưới) |
| `TELEMETRY_PERIOD_MS` | 10000 | Chu kỳ gửi dữ liệu |
| `ACK_TIMEOUT_MS`, `MAX_RETRIES` | 300, 3 | Thời gian chờ ACK và số lần gửi lại |
| `PUMP_MAX_ON_MS` | 60000 | Bơm tự tắt sau thời gian này |

Kênh ESP-NOW (`ESPNOW_CHANNEL`) nằm trong `../common/protocol.h` và phải giống Bridge.

## Build và nạp

Mở thư mục `Firmware/Sensor_node` bằng VS Code có PlatformIO, hoặc chạy lệnh:

```bash
pio run                 # build
pio run -t upload       # nạp
pio device monitor      # xem log, 115200
```

DevKitC-1 có 2 cổng USB-C. Với cấu hình hiện tại (`-DARDUINO_USB_CDC_ON_BOOT=1`), log Serial đi ra cổng **USB** (native). Nếu muốn xem log qua cổng **UART** (COM qua chip USB-UART), hãy xoá dòng đó trong `platformio.ini`.

## Hiệu chỉnh cảm biến đất

1. Nạp firmware, mở Serial Monitor.
2. Để cảm biến ngoài không khí, ghi lại `soilRaw`, gán vào `SOIL_RAW_DRY`.
3. Cắm cảm biến vào cốc nước tới vạch giới hạn, ghi lại `soilRaw`, gán vào `SOIL_RAW_WET`.
4. Nạp lại firmware.

## Hoạt động

1. Khi khởi động, relay ở trạng thái tắt. Node in ra MAC và kênh ESP-NOW.
2. Cứ mỗi 10 s, và ngay khi bơm đổi trạng thái, `sensorTask` đọc cảm biến, rồi `linkTask` gửi `TelemetryPkt`.
3. Nếu sau 300 ms chưa có ACK thì gửi lại, tối đa 3 lần. Sau đó gói được tính là mất (`lost`).
4. Ban đầu node gửi broadcast. Khi nhận ACK đầu tiên, node ghi nhớ MAC của Bridge và chuyển sang gửi unicast.
5. Khi nhận `CMD_SET_PUMP`, node bật hoặc tắt bơm, trả ACK, rồi gửi telemetry ngay. Lệnh trùng `seq` (Bridge gửi lại) chỉ được ACK lại, không thực hiện lần nữa.

Ví dụ log:

```
[LINK] Node 1, MAC 34:85:18:xx:xx:xx, channel 1
[SENSOR] T=27.3 H=65.1 soilRaw=2104 soil=47% pump=0 modbus=ok lost=0
[LINK] Bridge 34:85:18:yy:yy:yy
[RX] cmd=1 value=1 duration=30s
[PUMP] ON
```

## Gỡ lỗi nhanh

| Triệu chứng | Kiểm tra |
|---|---|
| `modbus=FAIL` | Đảo A/B, kiểm tra nguồn SHT20, TX/RX của XY-485, địa chỉ, baud, mã hàm và thanh ghi trong `node_config.h` |
| `lost` tăng liên tục | Bridge chưa chạy, khác kênh ESP-NOW, khác `PROTO_VERSION` |
| ESP32 reset khi bơm chạy | Bơm phải dùng nguồn riêng, chỉ nối chung GND |
| % độ ẩm đất không đổi | Chưa hiệu chỉnh, hoặc cảm biến đang cắm vào chân ADC2 |
