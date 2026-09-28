# DESK BUDDY — PROJECT SPECIFICATION

## 1. Giới thiệu

Desk Buddy là thiết bị trợ lý cá nhân trên bàn học, sử dụng ESP32-S3 làm bộ điều khiển trung tâm.

Thiết bị có khả năng hiển thị thời gian, lịch học/lịch cá nhân từ Google Calendar, reminder và Pomodoro.

Kiến trúc của hệ thống gồm:

* ESP32-S3
* 2 màn hình TFT
* DS3231 RTC
* RGB LED
* Piezo buzzer
* Web Dashboard
* Cloudflare Worker
* Google Calendar

## 2. Mục tiêu

Desk Buddy giúp người dùng xem nhanh lịch và thời gian ngay trên bàn học mà không cần liên tục mở điện thoại.

Thiết bị phải tiếp tục thực hiện các chức năng local cơ bản khi mất Wi-Fi.

## 3. Chức năng MVP

### 3.1. Thời gian

* Hiển thị giờ.
* Hiển thị ngày.
* Sử dụng DS3231 để duy trì thời gian local.

### 3.2. Google Calendar

* Đăng nhập bằng Google.
* Cấp quyền cho ứng dụng đọc Google Calendar.
* Lấy các event cần thiết.
* Hiển thị:

  * title
  * start
  * end
  * description

### 3.3. Reminder

* Thông báo sự kiện sắp diễn ra.
* Sử dụng RGB LED và buzzer khi cần.

### 3.4. Pomodoro

* Start.
* Reset.
* Timer chạy local trên ESP32.
* Có hiển thị trạng thái/thời gian.
* RGB LED và buzzer có thể được sử dụng để báo trạng thái.

### 3.5. Offline

Khi mất Wi-Fi:

* Clock vẫn chạy.
* Pomodoro vẫn chạy.
* RGB LED và buzzer vẫn hoạt động.
* Thiết bị không được treo vì đang chờ Wi-Fi.

## 4. Kiến trúc

Người dùng → Web Dashboard → Google OAuth → Cloudflare Worker → Google Calendar.

Cloudflare Worker chuyển dữ liệu thành JSON chuẩn của Desk Buddy và cung cấp cho ESP32-S3.

ESP32-S3 điều khiển TFT, RGB LED, buzzer và các chức năng local.

## 5. Phần cứng

* ESP32-S3-WROOM-1
* 2 × TFT ILI9341
* DS3231 RTC
* RGB LED
* Piezo buzzer
* USB-C 5V
* Dây/kết nối cần thiết

## 6. Ngoài phạm vi MVP

* DHT22
* Camera
* Mobile application riêng
* Wireless charging
* Battery
* ESP32 gọi trực tiếp Google Calendar
* AI chatbot
* Database phức tạp nếu chưa cần

## 7. Nguyên tắc phát triển

* API contract là chuẩn chung của project.
* Không tự ý đổi endpoint.
* Không tự ý đổi tên JSON field.
* Không commit secret/token/password.
* Mỗi task được phát triển và kiểm tra riêng.
* Mọi thay đổi quan trọng phải được ghi lại trên GitHub.
