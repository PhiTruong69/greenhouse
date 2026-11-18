# Node 2 – Điều khiển (Control)

Firmware cho ESP32-S3 DevKitC-1 (N16R8):

- Điều khiển **relay 2 kênh**: quạt và đèn.
- Kết nối **WiFi + MQTT thẳng tới CoreIOT**, không đi qua Gateway.
- Nhận **RPC** từ dashboard và RuleChain: `setFan`, `setLight`, `getFan`, `getLight`.
- Cập nhật firmware qua mạng bằng **OTA** (mã T14), dùng ThingsBoard Arduino SDK 0.15.

## Cấu trúc thư mục

```
Control/
├─ src/main.cpp                 khởi tạo, tạo netTask
├─ include/control_config.h     tên/phiên bản firmware, host CoreIOT, chân relay, chu kỳ
├─ include/secrets.h            WiFi + access token (KHÔNG đưa lên git)
├─ include/secrets.example.h    mẫu của secrets.h
└─ lib/
   ├─ Relay/                    bật/tắt relay quạt, đèn
   └─ CoreIot/                  WiFi, MQTT, RPC, OTA (ThingsBoard SDK)
```

Có một task FreeRTOS là `netTask`, chạy trên core 1. Task này lo WiFi/MQTT, xử lý RPC (đổi trạng thái relay ngay trong callback), OTA và gửi trạng thái. Mọi việc đều nằm trong một task, nên không cần queue hay mutex.

## Đấu nối

| Thiết bị | Chân | ESP32-S3 |
|---|---|---|
| Relay IN1 (quạt) | IN1 | GPIO4 (`PIN_RELAY_FAN`) |
| Relay IN2 (đèn) | IN2 | GPIO5 (`PIN_RELAY_LIGHT`) |
| Relay VCC / GND | | 5V / GND |
| Quạt 5 V, LED + 220 Ω | | qua tiếp điểm COM/NO của relay |

Relay opto thường **kích mức thấp**, nên mặc định `RELAY_ON_LEVEL = LOW`. Nếu thấy quạt bật khi lệnh là tắt, đổi thành `HIGH`.

## Cài đặt CoreIOT lần đầu

1. Đăng nhập CoreIOT, vào **Devices → + Add device**, đặt tên `Node2-Control`.
2. Mở device vừa tạo, chọn **Copy access token**.
3. Copy `include/secrets.example.h` thành `include/secrets.h`, điền tên WiFi, mật khẩu và token.
4. Kiểm tra `COREIOT_HOST` trong `control_config.h`, mặc định là `app.coreiot.io`, port 1883.
5. Nạp firmware qua USB lần đầu: `pio run -t upload`.
6. Trên CoreIOT, mở **Latest telemetry** của device. Bạn sẽ thấy `fan`, `light`, `current_fw_title = node2-control`, `current_fw_version = 1.0.0`.

Từ lần nạp thứ hai trở đi có thể dùng OTA (xem bên dưới).

### Thử RPC

Tạo dashboard, thêm widget **Switch control** và trỏ tới device `Node2-Control`:

- Quạt: RPC set value method `setFan`, get value method `getFan`.
- Đèn: `setLight` / `getLight`.

Tham số RPC nhận được các dạng `true`/`false`, `1`/`0`, `{"value": true}` hoặc `{"enabled": true}`. Vì vậy RuleChain (TV5) có thể gửi:

```json
{"method": "setFan", "params": true}
```

## OTA – cập nhật firmware qua mạng

### Ý tưởng

Bộ nhớ flash 16 MB được chia thành **hai vùng chương trình**, `app0` và `app1`, mỗi vùng 6,25 MB. Bảng phân vùng là `default_16MB.csv`, khai báo trong `platformio.ini`.

```
┌────────┬────────────┬────────────┬────────┐
│ nvs    │ app0       │ app1       │ spiffs │
│ otadata│ (đang chạy)│ (trống)    │        │
└────────┴────────────┴────────────┴────────┘
             ▲             │ ghi bản mới vào đây
             └── otadata chỉ vùng nào được boot
```

Chip chạy firmware ở một vùng, firmware mới được ghi vào vùng còn lại. Khi ghi xong và checksum đúng, `otadata` được đổi sang vùng mới rồi chip khởi động lại. Nếu tải hỏng giữa chừng thì `otadata` không đổi, và chip vẫn chạy bản cũ.

### Thiết bị và CoreIOT nói chuyện thế nào (MQTT)

CoreIOT chạy trên nền ThingsBoard, nên dùng cơ chế OTA của ThingsBoard. SDK đã lo hết các bước dưới đây:

| Bước | Chiều | Topic / dữ liệu |
|---|---|---|
| 1. Báo phiên bản đang chạy | Node → Cloud | telemetry `current_fw_title`, `current_fw_version` |
| 2. Hỏi gói được gán | Node → Cloud | yêu cầu shared attribute `fw_title`, `fw_version`, `fw_size`, `fw_checksum`, `fw_checksum_algorithm` |
| 3. Cloud báo có gói mới | Cloud → Node | shared attribute thay đổi (khi bạn gán gói cho device) |
| 4. So sánh | trên Node | `fw_title` khác `FW_TITLE` → từ chối (gói của thiết bị khác). `fw_version` trùng → báo `UPDATED`, không tải |
| 5. Tải theo chunk | Node ⇄ Cloud | gửi `v2/fw/request/{id}/chunk/{n}`, nhận `v2/fw/response/{id}/chunk/{n}` (mỗi chunk 4 KB) |
| 6. Kiểm tra và ghi | trên Node | ghi từng chunk vào vùng trống, cuối cùng so sánh SHA-256 với `fw_checksum` |
| 7. Khởi động lại | trên Node | boot vào bản mới. Bản mới báo `current_fw_version` mới và `fw_state = UPDATED` |

Trong suốt quá trình, node gửi telemetry `fw_state` lần lượt là `DOWNLOADING → DOWNLOADED → UPDATING → UPDATED`. Nếu lỗi thì gửi `FAILED`, kèm `fw_error`. CoreIOT dùng giá trị này để hiển thị trạng thái OTA của device. Dashboard riêng (N06) cũng có thể ghi nhận nó thành sự kiện.

Firmware gọi hai hàm của SDK:

- `Subscribe_Firmware_Update`: nhận gói mới khi gói được gán **trong lúc node đang chạy**.
- `Start_Firmware_Update`: khi vừa kết nối, hỏi xem có gói nào được gán **trong lúc node tắt** không. Hàm này cũng là bước báo `UPDATED` sau khi khởi động lại vào bản mới.

### Demo OTA từng bước (luồng E trong kiến trúc)

1. Trong `control_config.h`, sửa `FW_VERSION` thành `"1.1.0"`, rồi thay đổi một chỗ dễ thấy (ví dụ dòng log `[BOOT]`).
2. Chạy `pio run`. File cần upload là `.pio/build/esp32-s3-devkitc-1/firmware.bin`.
3. Trên CoreIOT, vào **Advanced features → OTA updates → + Add package**:
   - **Title**: `node2-control`, phải trùng `FW_TITLE`.
   - **Version**: `1.1.0`, phải trùng `FW_VERSION` của bản vừa build.
   - **Device profile**: profile mà `Node2-Control` đang dùng.
   - **Package type**: Firmware. Upload `firmware.bin`. Checksum để CoreIOT tự tính (SHA-256).
4. Gán gói: mở device `Node2-Control`, chỉnh sửa, chọn **Firmware** = `node2-control 1.1.0`. Hoặc gán trong device profile, khi đó mọi device dùng profile đó đều được cập nhật.
5. Xem Serial Monitor hoặc telemetry `fw_state`. Node sẽ in `[OTA] … %`, sau đó khởi động lại và in `[BOOT] node2-control 1.1.0`.

> **Lưu ý:** `firmware.bin` phải được build với `FW_VERSION` đúng bằng version khai báo trên CoreIOT. Nếu build xong mà quên đổi `FW_VERSION`, node sẽ tải đi tải lại mãi, vì sau khi khởi động lại nó vẫn báo version cũ.

### Giới hạn cần biết

- **Không tự quay về bản cũ.** Nếu bản mới chạy được nhưng không kết nối được WiFi hoặc MQTT (ví dụ sai token), node không tự lùi về bản trước. Khi đó phải nạp lại qua USB. Trước khi OTA, hãy thử bản mới qua USB trước.
- Đang dùng MQTT **không mã hoá** (port 1883), xác thực bằng access token. Muốn bảo mật hơn thì chuyển sang port 8883 với `WiFiClientSecure`.
- Khi khởi động lại sau OTA, relay về trạng thái **tắt**. RuleChain sẽ bật lại quạt ở lần kiểm tra nhiệt độ kế tiếp.

## Build, nạp, xem log

```bash
pio run                 # build
pio run -t upload       # nạp qua USB
pio device monitor      # log, 115200
```

Với `-DARDUINO_USB_CDC_ON_BOOT=1`, log đi ra cổng **USB** (native). Nếu muốn dùng cổng **UART/COM**, xoá dòng đó trong `platformio.ini`.

Ví dụ log:

```
[BOOT] node2-control 1.0.0
[WIFI] connecting to MyWifi
[MQTT] connecting app.coreiot.io:1883
[MQTT] connected
[MQTT] subscribe ok, fw node2-control 1.0.0
[RELAY] fan ON
```

| Triệu chứng | Kiểm tra |
|---|---|
| Kẹt ở `[WIFI] connecting` | Sai SSID hoặc mật khẩu, hoặc WiFi 5 GHz (ESP32 chỉ chạy 2.4 GHz) |
| `[MQTT] connect failed` lặp lại | Sai token, sai `COREIOT_HOST`, mạng chặn port 1883 |
| `fw_state = FAILED`, lỗi title | `Title` của gói khác `FW_TITLE` |
| OTA lặp lại liên tục | `FW_VERSION` trong bản build khác version của gói |
