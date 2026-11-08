# DESK BUDDY — ARCHITECTURE

## 1. Tổng quan

Desk Buddy gồm 4 thành phần phần mềm/hệ thống chính:

1. NodeMCU-32S (ESP32-WROOM-32)
2. Cloudflare Worker
3. Web account-linking page
4. Google Calendar

Phần cứng local gồm:

* WeAct Studio 2.9 inch monochrome e-paper (SPI)
* LCD1602 with I²C interface
* DS3231
* DS3231 backup coin cell (RTC only)
* RGB LED
* Buzzer
* Four buttons: MODE, UP, DOWN, OK

## 2. Kiến trúc tổng thể

```text
User
  ↓
  Web account-linking page
  ↓
Google OAuth
  ↓
Cloudflare Worker
  ↓
Google Calendar
  ↓
Normalized JSON
  ↓
NodeMCU-32S (ESP32-WROOM-32)
  ├── WeAct 2.9 inch e-paper
  ├── LCD1602 I²C
  ├── RGB LED
  ├── Buzzer
  ├── Buttons
  └── DS3231
```

## 3. Trách nhiệm từng module

### NodeMCU-32S (ESP32-WROOM-32)

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
* Xử lý lỗi HTTP.
* Lưu OAuth state, browser session và liên kết/tokens Calendar trong KV theo `device_id`; Production và Preview dùng namespace riêng.
* Quản lý secrets bằng environment variables/secrets của Cloudflare.

### Web account-linking page

* Là trang trung gian để đăng nhập Google, cấp quyền đọc và chọn Calendar cho một `device_id`.
* Hiển thị trạng thái kết nối và nút đăng xuất/đổi tài khoản.
* Đăng xuất xóa kết nối hiện tại; tài khoản đăng nhập kế tiếp thay thế kết nối Calendar trước đó.
* Không cần thay thế ứng dụng Calendar hay làm dashboard nhiều chức năng.

### Google Calendar

Là nguồn dữ liệu lịch của người dùng.

## 4. Online flow

```text
User
 ↓
Web account-linking page
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
E-paper / LCD1602
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
E-paper / LCD1602 / RGB LED / Buzzer
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

Prototype mặc định sử dụng:

```text
DB001
```

Device ID phải được gửi đúng theo API contract.
Mỗi thiết bị thử nghiệm/người dùng cần `device_id` riêng. Không dùng chung ID giữa các agent khi kiểm tra đổi tài khoản vì kết nối mới sẽ thay thế kết nối gắn với ID đó.
