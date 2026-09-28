# DESK BUDDY — ARCHITECTURE

## 1. Tổng quan

Desk Buddy gồm 4 thành phần phần mềm/hệ thống chính:

1. ESP32-S3
2. Cloudflare Worker
3. Web Dashboard
4. Google Calendar

Phần cứng local gồm:

* TFT 1
* TFT 2
* DS3231
* RGB LED
* Buzzer

## 2. Kiến trúc tổng thể

```text
User
  ↓
Web Dashboard
  ↓
Google OAuth
  ↓
Cloudflare Worker
  ↓
Google Calendar
  ↓
Normalized JSON
  ↓
ESP32-S3
  ├── TFT 1
  ├── TFT 2
  ├── RGB LED
  ├── Buzzer
  └── DS3231
```

## 3. Trách nhiệm từng module

### ESP32-S3

* Kết nối Wi-Fi.
* Reconnect Wi-Fi.
* Gọi API Calendar.
* Parse JSON chuẩn.
* Hiển thị dữ liệu.
* Chạy Pomodoro local.
* Quản lý trạng thái online/offline.
* Điều khiển LED/buzzer.
* Đọc DS3231.

### Cloudflare Worker

* Xử lý Google OAuth.
* Gọi Google Calendar API.
* Chuẩn hóa dữ liệu Calendar.
* Cung cấp Calendar API cho ESP32.
* Nhận device commands.
* Xử lý lỗi HTTP.
* Quản lý secrets bằng environment variables.

### Web Dashboard

* Cho người dùng kết nối Google Calendar.
* Hiển thị trạng thái kết nối.
* Hiển thị Calendar.
* Gửi command Pomodoro.

### Google Calendar

Là nguồn dữ liệu lịch của người dùng.

## 4. Online flow

```text
User
 ↓
Web Dashboard
 ↓
Google Login
 ↓
Allow permission
 ↓
Cloudflare Worker
 ↓
Google Calendar API
 ↓
Calendar events
 ↓
Normalize
 ↓
Desk Buddy JSON
 ↓
ESP32
 ↓
TFT
```

## 5. Offline flow

```text
Wi-Fi mất
 ↓
ESP32 chuyển OFFLINE
 ↓
DS3231 → Clock
 ↓
Local Timer → Pomodoro
 ↓
TFT / RGB LED / Buzzer
```

## 6. Quy tắc kiến trúc

* ESP32 không gọi trực tiếp Google Calendar.
* Cloudflare Worker là cầu nối.
* ESP32 chỉ nhận JSON theo API contract.
* Không gửi raw Google Calendar JSON tới ESP32.
* Pomodoro chạy local trên ESP32.
* Mất Wi-Fi không được làm chết clock/Pomodoro.
* Không đổi API endpoint hoặc JSON field nếu chưa thống nhất toàn nhóm.

## 7. Online và Offline

### Online

Có thể:

* Đồng bộ Calendar.
* Nhận dữ liệu mới.
* Gửi command từ Web.
* Hiển thị trạng thái online.

### Offline

Thiết bị vẫn phải:

* Hiển thị giờ.
* Hiển thị ngày.
* Chạy Pomodoro.
* Điều khiển LED/buzzer.

Calendar mới chỉ được cập nhật khi có kết nối lại.

## 8. Device ID

Prototype sử dụng:

```text
DB001
```

Device ID phải được gửi đúng theo API contract.
