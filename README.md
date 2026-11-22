# Desk Buddy

Desk Buddy là prototype thiết bị để bàn dùng ESP32, e-paper và LCD1602 để xem Google Calendar, ghi chú sự kiện, Pomodoro và đồng hồ offline. Web app liên kết tài khoản Google; Cloudflare Worker xử lý OAuth và cung cấp dữ liệu lịch cho thiết bị.

**Muốn tự làm một thiết bị giống dự án này?** Bắt đầu với [Hướng dẫn dựng Desk Buddy từ đầu](SETUP_GUIDE.md). Hướng dẫn liệt kê ứng dụng cần cài, phần cứng, cấu hình Google OAuth/Cloudflare, build firmware và chạy mô phỏng.

**Chỉ muốn chạy project trên máy?** Xem [các lệnh build và kiểm tra](#build--nạp-firmware) hoặc [mô phỏng Wokwi](#chạy-mô-phỏng-wokwi).

AI agents and contributors: start with [`AGENTS.md`](AGENTS.md) and [`STATUS.md`](STATUS.md), then follow the issue/branch/PR workflow in [`CONTRIBUTING.md`](CONTRIBUTING.md). `AGENTS.md` is the shared project brief and contains a reusable prompt for other coding agents.

---

## Phần cứng mục tiêu

| Thành phần | Chi tiết |
|---|---|
| Vi điều khiển | NodeMCU-32S (ESP32-WROOM-32) |
| E-paper (màn chính) | WeAct Studio 2.9″ trắng đen, SPI |
| LCD phụ | LCD1602 giao tiếp I²C (địa chỉ `0x27`) |
| RTC | DS3231 |
| Nút bấm | MODE (GPIO32), UP (GPIO33), DOWN (GPIO19), OK (GPIO35) |
| Còi | Piezo buzzer qua transistor (GPIO17) |

> **GPIO35** là input-only, không có pull-up nội bộ. Mạch thật cần điện trở **10 kΩ kéo lên 3.3 V**. Không nối 5 V vào bất kỳ GPIO ESP32 nào.

Xem sơ đồ chân đầy đủ và lưu ý nguồn, level-shifter LCD tại [`HARDWARE_DESIGN.md`](HARDWARE_DESIGN.md).
Sơ đồ lắp mạch đầy đủ, gồm driver buzzer, pull-up GPIO35 và phân tách I²C 3.3/5 V: [`HARDWARE_WIRING.md`](HARDWARE_WIRING.md).
Repository hiện chưa có file CAD/mô hình 3D cho vỏ thiết bị.

---

## Kiến trúc hệ thống

```
Người dùng
  → Web account-linking page (đăng nhập/chọn lịch/đăng xuất Google)
  → Cloudflare Worker (OAuth, Calendar API)
  → ESP32: hiển thị lịch/ghi chú/Pomodoro trên e-paper
           hiển thị ngày/giờ liên tục trên LCD từ DS3231
```

Chi tiết tại [`ARCHITECTURE.md`](ARCHITECTURE.md) · API contract tại [`API.md`](API.md).

---

## Giao diện e-paper và điều khiển

> Firmware hiện triển khai luồng điều khiển dưới đây; trải nghiệm trên Wokwi và phần cứng vẫn cần được kiểm tra riêng.

### Vai trò hai màn hình

- **LCD1602** hiển thị ngày ở dòng đầu và giờ cùng trạng thái online/offline ở dòng hai. ESP32 lấy giờ từ DS3231 để LCD vẫn chạy khi mất Wi-Fi; khi có Wi-Fi, thiết bị có thể đồng bộ RTC qua NTP. Pomodoro không thay nội dung LCD.
- **E-paper** là giao diện chính để xem NOTE, điều khiển Pomodoro, quản lý tài khoản và cài đặt ngày giờ. ESP32 cập nhật từng màn riêng; hai màn không nối trực tiếp với nhau.
- Pomodoro chạy nền trên ESP32, nên tiếp tục đếm khi người dùng chuyển sang tab khác.

### Luồng khởi động

- Khi cấp nguồn hoặc đánh thức thiết bị bằng MODE, e-paper hiện Welcome.
- Nếu chưa liên kết Google Calendar, hiện QR để mở web app và ghép tài khoản với `device_id`.
- Nếu đã có trạng thái liên kết/dữ liệu lịch, vào NOTE. QR chỉ chứa URL công khai và mã thiết bị, không chứa token đăng nhập.

### Các chế độ

| Chế độ | Cách dùng dự kiến |
|---|---|
| **Calendar / NOTE** | Calendar hiển thị 7 ngày từ thứ Hai đến Chủ nhật; UP/DOWN chọn ngày, OK mở ghi chú/sự kiện của ngày đó. Trong NOTE, UP/DOWN chuyển giữa các sự kiện. E-paper luân phiên tiêu đề sự kiện và note mỗi 4 giây; nội dung dài tự chuyển trang. OK quay lại lịch tuần; sau 15 giây không thao tác cũng quay về lịch tuần. |
| **Pomodoro** | OK lần lượt xác nhận số chu kỳ, thời gian làm việc và thời gian nghỉ rồi bắt đầu. UP/DOWN chỉnh chu kỳ từng 1 (1–12), thời gian làm việc từng 10 phút (10–120) và nghỉ từng 5 phút (5–60). Khi hết một phiên làm, buzzer kêu hai nhịp khoảng 1 giây; hoàn tất toàn bộ chu kỳ, buzzer kêu khoảng 3 giây. MODE đổi màn hình mà timer và cấu hình vẫn giữ nguyên trong lúc chạy. |
| **Account** | Hiển thị QR để liên kết Google Calendar; OK mở điểm truy cập cấu hình Wi-Fi để chọn mạng và nhập mật khẩu. MODE đổi màn hình. |

MODE nhấn ngắn chuyển tuần tự giữa Calendar, NOTE, Pomodoro và Account. Không có chế độ ngủ.

### Nội dung tiếng Việt trên e-paper

E-paper/font hiện dùng có thể không vẽ được đầy đủ dấu tiếng Việt. Dữ liệu Calendar tiếng Việt vẫn được nhận và giữ nguyên; riêng chuỗi đưa lên màn hình sẽ được chuyển sang dạng **không dấu** (ví dụ `Họp nhóm` → `Hop nhom`). Nếu gặp ký tự không hỗ trợ như emoji, firmware có thể thay bằng `?` hoặc bỏ ký tự đó. Quy tắc này chỉ áp dụng cho bản hiển thị, không sửa dữ liệu trên Google Calendar.

### Thiết bị báo hiệu

- Buzzer báo chuyển trạng thái Pomodoro, nhắc sự kiện và hoàn tất hẹn giờ; cần tiết chế âm báo. Prototype hiện không lắp LED trạng thái.
- E-paper chỉ refresh khi nội dung thay đổi, không dùng làm đồng hồ chạy từng giây.

---

## Cấu trúc thư mục

```
Desk-Buddy-Project/
├── firmware/ESP32/          ← Firmware PlatformIO (production)
│   ├── src/
│   │   ├── main.cpp         ← Vòng lặp chính, vẽ UI, WiFi, fetch Calendar
│   │   ├── interaction.h    ← State machine điều hướng màn hình
│   │   ├── calendar_logic.h ← Parse RFC3339, tính reminder, Gregorian math
│   │   ├── config.example.h ← Mẫu cấu hình (copy → config.h, git-ignored)
│   │   └── config.h         ← Cấu hình cục bộ (KHÔNG commit)
│   ├── test/test_interaction/test_main.cpp  ← Native unit tests
│   └── platformio.ini
├── simulation/wokwi/        ← Sketch mock cho Wokwi (không dùng OAuth thật)
│   ├── sketch.ino
│   ├── diagram.json
│   ├── libraries.txt
│   └── wokwi.toml
├── worker/cloudflare/       ← Cloudflare Worker (OAuth + Calendar API)
│   └── src/index.ts
├── web/                     ← Trang liên kết tài khoản Google (Vite)
│   └── src/main.js
├── SETUP_GUIDE.md            ← Hướng dẫn dựng lại project từ đầu
├── ARCHITECTURE.md
├── API.md
└── HARDWARE_DESIGN.md
```

---

## Chạy local

Yêu cầu: Node.js LTS 22 trở lên, npm.

### 1. Cấu hình Google OAuth

Trong Google Cloud Console, tạo OAuth Client ID loại **Web application**. Thêm Redirect URI:

```text
http://localhost:8787/api/oauth/google/callback
```

Sao chép `worker/cloudflare/.dev.vars.example` thành `worker/cloudflare/.dev.vars` rồi điền giá trị:

```dotenv
APP_ENV=local
WEB_ORIGIN=http://localhost:5173
GOOGLE_CLIENT_ID=...apps.googleusercontent.com
GOOGLE_CLIENT_SECRET=...
GOOGLE_REDIRECT_URI=http://localhost:8787/api/oauth/google/callback
```

Thêm device token cho `DB001`:

```dotenv
DEVICE_API_TOKENS={"DB001":"replace-with-a-long-random-device-token"}
```

> Không commit token. Worker kiểm tra header `Authorization: Bearer <token>` khi ESP32 gọi `/api/calendar?device_id=DB001`. OAuth consent screen cần scope `openid`, `email`, `https://www.googleapis.com/auth/calendar.readonly` và thêm tài khoản thử nghiệm nếu ứng dụng còn ở chế độ Testing.

### 2. Chạy Cloudflare Worker

```powershell
cd D:\Desk-Buddy-Project\worker\cloudflare
npm install
npm run dev
```

Worker chạy tại `http://localhost:8787`. Kiểm tra: `http://localhost:8787/api/health`.

### 3. Chạy trang liên kết tài khoản

```powershell
cd D:\Desk-Buddy-Project\web
npm install
npm run dev
```

Mở `http://localhost:5173` (hoặc `http://localhost:5173/?device_id=DB001`). Nếu Worker ở URL khác, sao chép `web/.env.example` thành `web/.env.local` và đặt `VITE_API_BASE_URL`. Không đặt secret vào biến `VITE_`.

### 4. Kiểm tra OAuth

1. Mở trang Connect → **Đăng nhập bằng Google**.
2. Chọn tài khoản và cấp quyền đọc Calendar.
3. Worker xử lý callback server-side → chuyển về Web.
4. Web truy vấn trạng thái qua session cookie HttpOnly; khi thành công hiển thị "Google Calendar đã được kết nối".

Worker dùng OAuth state ngẫu nhiên hết hạn 10 phút (một lần dùng) cùng PKCE. OAuth state, phiên trình duyệt và Google token được lưu trong Cloudflare KV binding `DESK_BUDDY_STORE` để tồn tại qua lần khởi động lại isolate. Phải tạo và bind namespace trước khi deploy Worker; xem `worker/cloudflare/README.md`.

### 5. Dùng QR trên thiết bị thật

QR cần các địa chỉ public đã deploy; địa chỉ localhost chỉ chạy trên chính máy tính, điện thoại quét QR sẽ không mở được. Trước khi nạp firmware, cấu hình:

1. Deploy Worker và web app qua HTTPS. Trong Worker đặt `WEB_ORIGIN` thành origin web app (chỉ `https://...`, không có đường dẫn), cùng các secret OAuth và device token như phần trên.
2. Trong Google OAuth client, thêm `https://<worker-domain>/api/oauth/google/callback` vào **Authorized redirect URIs**.
3. Build web app với `VITE_API_BASE_URL=https://<worker-domain>` rồi deploy thư mục `web/dist`.
4. Trong `firmware/ESP32/src/config.h`, điền `WEB_APP_URL` là URL public của web app, ví dụ `https://<web-domain>/`, và điền đúng `DEVICE_ID`.
5. Build/nạp firmware, vào trang **Tài khoản**, quét QR. Hoàn tất Google consent và chọn lịch trên web page.

Địa chỉ web và Worker nên cùng một site/domain gốc để trình duyệt cho phép session hoạt động ổn định. Worker lưu liên kết và OAuth token trong KV binding `DESK_BUDDY_STORE`.

### Cấu hình Wi-Fi trên thiết bị

Lần đầu khởi động (hoặc khi mạng đã lưu không kết nối được), ESP32 tạo Wi-Fi tạm có mật khẩu ngẫu nhiên hiển thị trên e-paper. Dùng điện thoại kết nối mạng `DeskBuddy-<device_id>`, mở `http://192.168.4.1`, chọn Wi-Fi nhà và nhập mật khẩu. ESP32 lưu cấu hình trong bộ nhớ nội bộ rồi khởi động lại. Muốn đổi mạng đã lưu, vào màn hình Tài khoản và nhấn OK. MODE nhấn ngắn luôn chuyển chế độ.

### TypeScript check Worker

```powershell
cd D:\Desk-Buddy-Project\worker\cloudflare
npm run check
```

---

## Build & nạp firmware

### Yêu cầu

- PlatformIO (VS Code extension hoặc CLI).
- Driver USB-serial cho NodeMCU-32S đã cài trên máy.
- Cổng COM xuất hiện khi cắm ESP32 vào USB (kiểm tra Device Manager).

### Các bước

1. Mở thư mục `firmware/ESP32` trong VS Code (hoặc `cd` vào đó).
2. Sao chép file cấu hình:
   ```powershell
   Copy-Item firmware\ESP32\src\config.example.h firmware\ESP32\src\config.h
   ```
3. Mở `firmware/ESP32/src/config.h` và điền:
   - `API_BASE_URL` — URL HTTPS của Worker đã deploy (`https://...workers.dev`)
   - `WEB_APP_URL` — URL web app đã deploy (không có query string)
   - `DEVICE_ID` — `DB001`
   - `DEVICE_API_TOKEN` — token khớp với `DEVICE_API_TOKENS` trong Worker secret
   - `API_ROOT_CA_PEM` — PEM root CA xác thực `API_BASE_URL` (giữ nguyên HTTPS validation, không dùng `setInsecure()`)
   - Múi giờ: `LOCAL_UTC_OFFSET_SECONDS = 7 * 60 * 60` (UTC+7, Việt Nam)
4. Compile kiểm tra (không nạp):
   ```powershell
   cd firmware\ESP32
   pio run -e nodemcu-32s
   ```
   PlatformIO dùng partition table riêng với hai app slot 1.94 MB để phù hợp dung lượng firmware trên ESP32 flash 4 MB.
5. Khi ESP32 đã cắm và cổng COM xuất hiện, nạp firmware:
   ```powershell
   pio run -e nodemcu-32s --target upload
   ```
6. Mở Serial Monitor để xem log khởi động:
   ```powershell
   pio device monitor --baud 115200
   ```

> **Không commit `src/config.h`** — file này chứa Wi-Fi và device token, đã được gitignore.

### Driver e-paper

Firmware dùng `GxEPD2_290_GDEY029T94`. **Trước khi nạp lên bo thật**, kiểm tra mã in phía sau panel WeAct để xác nhận đúng driver. Nếu màn hiển thị trắng hoàn toàn sau khi nạp:

1. Kiểm tra Serial log — lỗi khởi tạo e-paper sẽ xuất hiện ở đây.
2. Kiểm tra dây nối SPI: CLK→GPIO18, MOSI→GPIO23, CS→GPIO27, DC→GPIO26, RST→GPIO25, BUSY→GPIO34.
3. Thử driver khác trong `GxEPD2_BW<...>` nếu mã panel không khớp `GDEY029T94`.
4. LCD hoạt động bình thường khi e-paper trắng → vấn đề là driver/wiring e-paper, không phải code.

### Bring-up sequence khuyến nghị

1. Nạp firmware, kiểm tra e-paper hiển thị màn "Lịch".
2. Kiểm tra LCD hiển thị ngày/giờ từ DS3231.
3. Kết nối Wi-Fi → xem Serial log đồng bộ NTP.
4. Liên kết Google Calendar qua trang tài khoản → scan QR trên màn Tài khoản.
5. Kiểm tra cập nhật sự kiện theo thao tác **Update events** trên thiết bị; nhịp đồng bộ nền cần được chốt khi triển khai để cân bằng độ mới dữ liệu và năng lượng.

---

## Chạy mô phỏng Wokwi

Sketch `simulation/wokwi/sketch.ino` là **mock không dùng OAuth/Wi-Fi/Google Calendar thật**. Mục đích: kiểm tra layout UI và button flow.

### Wokwi.com (không cần cài đặt)

1. Vào `https://wokwi.com/projects/new/esp32`.
2. Thay toàn bộ nội dung `diagram.json` và `sketch.ino` bằng file trong thư mục `simulation/wokwi/`.
3. Thêm thư viện từ `libraries.txt` trong Library Manager nếu chưa có.
4. Nhấn **Start Simulation**.

> Nếu mạch không dựng: có thể file `diagram.json` hoặc `sketch.ino` bị dán nối tiếp nhau (hai JSON object liên tiếp). Chọn toàn bộ nội dung trong tab Wokwi và thay bằng nội dung file hoàn chỉnh.

### Wokwi for VS Code

1. Mở thư mục `simulation/wokwi` trong VS Code.
2. Build environment `esp32dev`:
   ```powershell
   cd simulation\wokwi
   pio run -e esp32dev
   ```
3. Chạy Wokwi từ VS Code extension.

**Lưu ý**: Wokwi dùng driver `GxEPD2_290_BS` (hỗ trợ trong simulator); firmware thật dùng `GxEPD2_290_GDEY029T94`. Hành vi/layout không giống hệt nhau.

---

## Unit test (native)

```powershell
cd firmware\ESP32
pio test -e native
```

Test kiểm tra: parse RFC3339, tính reminder, điều hướng màn. Yêu cầu `gcc/g++` trên máy (Windows: cài MinGW/MSYS2 hoặc dùng WSL). Không cần ESP32 thật.

---

## Trạng thái kiểm tra

| Hạng mục | Kết quả |
|---|---|
| `pio run -e nodemcu-32s` | ✅ Thành công (~97% flash) |
| `pio run -e esp32dev` (Wokwi) | ⚠️ Build từng thành công; nhóm báo mô phỏng hiện không hoạt động như mong muốn, cần dựng lại và xác minh |
| `npm run build` (web) | ✅ Thành công |
| `npm run check` (Worker TypeScript) | ✅ Thành công |
| Web/Worker timezone tests (9 cases) | ✅ Thành công |
| `pio test -e native` | ⚠️ Chưa chạy được — máy thiếu `gcc/g++` |
| Nạp firmware lên bo thật | ⏳ Chưa thực hiện — ESP32 chưa được nhận diện qua USB |
| Hiển thị e-paper WeAct thật | ⏳ Chưa xác nhận — cần nạp và kiểm tra bo thật |
| Xác nhận driver `GxEPD2_290_GDEY029T94` | ⏳ Cần kiểm tra mã panel phía sau tấm e-paper |

---

## Giới hạn hiện tại và việc tiếp theo

| Hạng mục | Trạng thái |
|---|---|
| **Nạp firmware thật** | Cần ESP32 xuất hiện ở cổng COM (kiểm tra driver USB-serial và cáp data) |
| **Xác nhận driver e-paper** | Xem mã trên mặt sau panel; thay driver trong `main.cpp` nếu cần |
| **Pomodoro chu kỳ/làm/nghỉ** | Luồng cấu hình, timer nền và tín hiệu buzzer đã biên dịch; cần xác minh hành vi trên thiết bị |
| **Lịch tuần và luân phiên event/note** | Đã triển khai trong production firmware; cần xác minh nút bấm và refresh e-paper |
| **Cấu hình Wi-Fi** | Đã có; kết nối access point của ESP32, chọn mạng và nhập mật khẩu trên trang cục bộ |
| **Ghi chú dài phân trang** | Code đã có; chưa thử trên phần cứng thật |
| **Lưu trữ OAuth bền vững** | Worker Production đã dùng KV; Preview dùng namespace riêng |
| **Đăng nhập/đăng xuất Google** | Người dùng xác nhận hoạt động trên web đã deploy; Calendar trên ESP32 thật chưa xác minh |
| **Pairing thiết bị an toàn** | Quy trình cấp/thu hồi token chưa triển khai |

---

## Bảo mật

- Không commit `src/config.h`, `worker/cloudflare/.dev.vars`, hoặc bất kỳ secret nào.
- Không đặt secret vào biến `VITE_` (bundle vào JS public).
- Token nhúng trong firmware prototype có thể bị trích xuất từ flash; trước khi dùng thực tế cần quy trình token an toàn hơn.
- Worker giữ HTTPS validation — không dùng `setInsecure()` trong firmware.
- Xem thêm: [`API.md` phần Security](API.md#7-security).
