# DESK BUDDY — API CONTRACT

## 1. Nguyên tắc

Đây là API contract chung của project.

Không được tự ý:

* đổi endpoint;
* đổi tên field;
* đổi kiểu dữ liệu;
* thêm chức năng làm thay đổi architecture.

Nếu cần thay đổi API, phải thống nhất với toàn nhóm trước.

---

# 2. Calendar API

## Endpoint

```http
GET /api/calendar?device_id=DB001
```

## Request

Ví dụ:

```http
GET /api/calendar?device_id=DB001
```

## Response

```json
{
  "device_id": "DB001",
  "events": [
    {
      "title": "Họp nhóm",
      "start": "2026-09-28T19:00:00+07:00",
      "end": "2026-09-28T20:00:00+07:00",
      "description": "Chuẩn bị demo Desk Buddy"
    }
  ]
}
```

## Field

| Field       | Kiểu   | Ý nghĩa            |
| ----------- | ------ | ------------------ |
| device_id   | string | Mã thiết bị        |
| events      | array  | Danh sách sự kiện  |
| title       | string | Tên sự kiện        |
| start       | string | Thời gian bắt đầu  |
| end         | string | Thời gian kết thúc |
| description | string | Nội dung ghi chú   |
| start_time_zone | string (tùy chọn) | Múi giờ IANA để hiển thị start, ví dụ `Asia/Ho_Chi_Minh` |
| end_time_zone | string (tùy chọn) | Múi giờ IANA để hiển thị end |

Worker có thể trả thêm `start_time_zone` và `end_time_zone` trên từng event để Web định dạng thời gian theo múi giờ của event/calendar. Hai field này là metadata tùy chọn; các field hiện có và cấu trúc `{ device_id, events }` được giữ nguyên. Event cả ngày tiếp tục dùng chuỗi date-only `YYYY-MM-DD`.

## Event không có description

Nếu Google Calendar không có description:

```json
{
  "title": "Học Toán",
  "start": "2026-09-28T20:30:00+07:00",
  "end": "2026-09-28T22:00:00+07:00",
  "description": ""
}
```

## Không có event

```json
{
  "device_id": "DB001",
  "events": []
}
```

---

# 3. Device Command API

## Endpoint

```http
POST /api/device/command
```

## Start Pomodoro

Request:

```json
{
  "device_id": "DB001",
  "command": "start"
}
```

Response:

```json
{
  "success": true,
  "device_id": "DB001",
  "command": "start"
}
```

## Reset Pomodoro

Request:

```json
{
  "device_id": "DB001",
  "command": "reset"
}
```

Response:

```json
{
  "success": true,
  "device_id": "DB001",
  "command": "reset"
}
```

---

# 4. Error Response

Ví dụ device_id sai:

```json
{
  "success": false,
  "error": "Invalid device_id"
}
```

Command không hợp lệ:

```json
{
  "success": false,
  "error": "Invalid command"
}
```

---

# 5. Quy tắc dữ liệu

Google Calendar có thể trả dữ liệu khác với format của Desk Buddy.

Cloudflare Worker phải normalize dữ liệu trước khi gửi cho ESP32.

Ví dụ:

```text
Google Calendar
summary
   ↓
Desk Buddy
title
```

```text
Google Calendar
start.dateTime
   ↓
Desk Buddy
start
```

```text
Google Calendar
end.dateTime
   ↓
Desk Buddy
end
```

```text
Google Calendar
description
   ↓
Desk Buddy
description
```

ESP32 không cần biết format JSON gốc của Google Calendar.

---

# 6. Device ID

Prototype:

```text
DB001
```

Mọi request liên quan đến thiết bị phải sử dụng device_id đúng contract.

---

# 7. Security

Không đưa các thông tin sau vào API response public hoặc GitHub:

* Google client secret
* Refresh token
* Access token
* API key
* Wi-Fi password
* Password tài khoản

Secrets phải được lưu ở nơi phù hợp, không hard-code trong source code.
