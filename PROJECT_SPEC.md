# DESK BUDDY — PROJECT SPECIFICATION

## 1. Giới thiệu

Desk Buddy là thiết bị trợ lý cá nhân trên bàn học, sử dụng NodeMCU-32S (ESP32-WROOM-32) làm bộ điều khiển trung tâm.

Thiết bị có khả năng hiển thị thời gian, lịch học/lịch cá nhân từ Google Calendar, reminder và Pomodoro.

Kiến trúc của hệ thống gồm:

* NodeMCU-32S (ESP32-WROOM-32)
* Màn hình e-paper WeAct Studio 2.9 inch, đen trắng, giao tiếp SPI
* Màn hình LCD1602 giao tiếp I²C
* DS3231 RTC
* RGB LED
* Piezo buzzer
* Web account-linking page
* Cloudflare Worker
* Google Calendar

## 2. Mục tiêu

Desk Buddy giúp người dùng xem nhanh lịch và thời gian ngay trên bàn học mà không cần liên tục mở điện thoại.

Thiết bị phải tiếp tục thực hiện các chức năng local cơ bản khi mất Wi-Fi.

## 3. Chức năng MVP

### 3.1. Thời gian

* LCD1602 hiển thị giờ và trạng thái Pomodoro.
* E-paper hiển thị lịch trong ngày và sự kiện sắp tới.
* Sử dụng DS3231 để duy trì thời gian local khi mất nguồn USB; pin RTC không cấp nguồn cho ESP32 hoặc màn hình.

### 3.2. Google Calendar

* Người dùng mở trang web của thiết bị để đăng nhập Google và cấp quyền chỉ đọc Calendar.
* Người dùng chọn calendar cần đồng bộ.
* Worker lấy và chuẩn hóa các event cần thiết; ESP32 nhận dữ liệu qua API contract.
* Đăng xuất xóa liên kết Calendar đang dùng cho `device_id`; khi đăng nhập tài khoản khác, tài khoản mới thay thế liên kết cũ của thiết bị đó.
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

### 3.6. Màn hình và nút bấm

* E-paper là màn hình chính cho lịch, ghi chú trong mô tả event, và Pomodoro.
* LCD1602 là màn phụ hiển thị đồng hồ/ngày và trạng thái Pomodoro.
* Dùng bốn nút MODE, UP, DOWN, OK. MODE đổi màn; giữ MODE 5 giây yêu cầu deep sleep. UP/DOWN điều hướng nội dung hoặc chỉnh thời lượng Pomodoro. OK mở ghi chú hoặc bắt đầu/tạm dừng Pomodoro.
* Hành vi chi tiết có thể được tinh chỉnh khi dựng lại firmware, nhưng không được giảm còn ba nút.

## 4. Kiến trúc

Người dùng → Web account-linking page → Google OAuth → Cloudflare Worker → Google Calendar.

Cloudflare Worker chuyển dữ liệu thành JSON chuẩn của Desk Buddy và cung cấp cho NodeMCU-32S.

NodeMCU-32S điều khiển hai màn hình, RTC, RGB LED, buzzer và các chức năng local.

## 5. Phần cứng

* NodeMCU-32S (ESP32-WROOM-32)
* 1 × WeAct Studio 2.9 inch e-paper đen trắng (SPI)
* 1 × LCD1602 I²C
* DS3231 RTC
* Pin cúc áo dự phòng cho DS3231 (không cấp nguồn cho toàn thiết bị)
* RGB LED
* Piezo buzzer
* 4 nút nhấn: MODE, UP, DOWN, OK
* Bộ chuyển mức I²C 3.3 V ↔ 5 V cho LCD1602 nếu backpack kéo SDA/SCL lên 5 V
* Nguồn USB 5 V qua cổng của NodeMCU-32S
* Dây/kết nối cần thiết

Arduino Nano boards hiện có không cần dùng trong cấu hình prototype này; một NodeMCU-32S điều khiển các module.

## 6. Ngoài phạm vi MVP

* DHT22
* Camera
* Mobile application riêng
* Wireless charging
* Pin sạc cấp nguồn cho toàn thiết bị
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
