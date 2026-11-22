# Dựng Desk Buddy từ đầu

Hướng dẫn này giúp một người mới dựng lại prototype Desk Buddy: ESP32 lấy Calendar qua Worker, web app liên kết tài khoản Google, e-paper hiển thị lịch/ghi chú/Pomodoro và LCD hiển thị đồng hồ. Làm theo thứ tự để mỗi phần có thể kiểm tra riêng.

> **Giới hạn đã biết:** firmware là prototype. Build thành công không chứng minh thiết bị chạy đúng. Mô phỏng Wokwi dùng dữ liệu giả; nó không kiểm tra Google OAuth, Worker thật, driver của panel WeAct, dây nối hay phần cứng thật. Mã panel chính xác cần kiểm tra trên nhãn mặt sau trước khi nạp.

## 1. Bạn cần chuẩn bị

### Tài khoản và ứng dụng

- Máy tính Windows, macOS hoặc Linux có Internet.
- Tài khoản GitHub để lấy mã nguồn.
- Tài khoản Google để tạo Cloud project và thử đăng nhập.
- Tài khoản Cloudflare để chạy Worker và trang web công khai. Có thể dùng local development trước khi deploy.
- [Git](https://git-scm.com/downloads) để clone repository.
- [Visual Studio Code](https://code.visualstudio.com/) và extension [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) để build firmware. PlatformIO IDE cài PlatformIO Core bên trong; không cần cài CLI riêng.
- [Node.js LTS 22 trở lên](https://nodejs.org/en/download/) và npm để chạy web app, Worker và các lệnh Wrangler. Node.js 20 đã hết vòng đời hỗ trợ; chọn một bản LTS còn được hỗ trợ.
- Trình duyệt web hiện đại để cấu hình Cloudflare/Google và kiểm tra đăng nhập.
- Tùy cách chạy mô phỏng: tài khoản miễn phí tại [Wokwi](https://wokwi.com/) hoặc extension Wokwi cho VS Code.
- Tùy bo mạch: driver USB-serial phù hợp với chip trên board (thường CP210x hoặc CH340). Xem chip thật trên board; dùng cáp USB có truyền dữ liệu.

Không cần Arduino IDE cho quy trình PlatformIO trong tài liệu này. Windows có thể dùng script `firmware/ESP32/setup.ps1` để tạo `config.h` và build firmware sau khi đã cài PlatformIO.

### Phần cứng

| Linh kiện | Số lượng / thông tin |
|---|---|
| NodeMCU-32S ESP32-WROOM-32 | 1 |
| WeAct Studio e-paper trắng đen 2.9 inch SPI | 1; kiểm tra mã revision/controller ở mặt sau |
| LCD1602 kèm backpack I²C | 1; địa chỉ mặc định trong firmware là `0x27` |
| RTC DS3231 và pin backup phù hợp | 1 |
| Nút nhấn momentary | 4: MODE, UP, DOWN, OK |
| Piezo buzzer và transistor/MOSFET driver | 1 bộ |
| I²C level shifter hai chiều | 1 nếu LCD backpack dùng pull-up 5 V |
| Điện trở kéo lên 10 kΩ | 1 cho nút OK ở GPIO35 |
| Dây jumper, breadboard và nguồn USB 5 V | đủ dùng |

Xem sơ đồ chân, mức điện áp, pin RTC và cách đấu buzzer trong [HARDWARE_DESIGN.md](HARDWARE_DESIGN.md) và [HARDWARE_WIRING.md](HARDWARE_WIRING.md). Không đưa 5 V vào GPIO ESP32. GPIO35 chỉ là input và cần điện trở kéo lên ngoài. Không dùng pin coin-cell để cấp nguồn cho toàn bộ thiết bị.

Prototype của nhóm bỏ qua LED trạng thái; không cần mua hoặc đấu LED và điện trở RGB.

Mô hình 3D hiện có tại [hardware/3d/desk-buddy-concept.stl](hardware/3d/desk-buddy-concept.stl) chỉ là mô hình mô phỏng/ý tưởng. Các chi tiết lắp ráp bên trong và lỗ luồn dây chưa được thiết kế; chưa dùng mô hình này làm mẫu in cuối cùng hoặc để xác nhận kích thước/lắp vừa linh kiện.

## 2. Lấy mã nguồn và kiểm tra các phần không cần credential

```powershell
git clone https://github.com/nguyendonhat/Desk-Buddy-Project.git
cd Desk-Buddy-Project
```

Mở repo trong VS Code. Có thể chạy mock Wokwi trước để xem bố cục giao diện mà chưa cần Google OAuth hoặc phần cứng; xem mục [Chạy mock Wokwi](#5-chạy-mock-wokwi).

## 3. Tạo Google OAuth client

Worker thực hiện OAuth phía server. Web app và firmware không được chứa Google client secret. Scope hiện tại của Worker là `openid`, `email` và `https://www.googleapis.com/auth/calendar.readonly`.

1. Mở [Google Cloud Console](https://console.cloud.google.com/), tạo project riêng và bật **Google Calendar API**.
2. Mở **Google Auth Platform**. Hoàn thành Branding (tên app, email hỗ trợ, thông tin liên hệ); chọn Audience phù hợp. Với project thử nghiệm cho tài khoản cá nhân, chọn **External / Testing** và thêm từng Google account ở danh sách **Test users**.
3. Ở phần Data Access, khai báo các scope mà Worker dùng: `openid`, `email`, `https://www.googleapis.com/auth/calendar.readonly`.
4. Tạo OAuth client loại **Web application**. Client secret chỉ được lưu ở Worker/local `.dev.vars`.
5. Khi chạy local, thêm Authorized redirect URI chính xác:

   ```text
   http://localhost:8787/api/oauth/google/callback
   ```

6. Khi deploy, thêm redirect URI public đúng dạng:

   ```text
   https://<worker-domain>/api/oauth/google/callback
   ```

   Scheme, hostname, path và dấu `/` phải khớp hoàn toàn với `GOOGLE_REDIRECT_URI`.

Ở chế độ External / Testing chỉ các test user được thêm mới đăng nhập được; đó là lựa chọn phù hợp cho thử nghiệm nhóm. Muốn người dùng Google bất kỳ sử dụng app, cần chuẩn bị yêu cầu xác minh OAuth theo chính sách Google cho các scope nhạy cảm. Xem [hướng dẫn OAuth Web Server của Google](https://developers.google.com/identity/protocols/oauth2/web-server) và [trạng thái kiểm thử/xác minh OAuth](https://developers.google.com/identity/protocols/oauth2/production-readiness/overview).

Nếu định đưa app ra công khai, cần domain mà bạn sở hữu để xác minh quyền sở hữu và hoàn tất branding/privacy-policy theo yêu cầu Google. `workers.dev` và `pages.dev` là domain dùng chung của nền tảng; đừng xem chúng là domain riêng của bạn cho quy trình xác minh production. Có thể gắn custom domain của bạn vào Pages và Worker. Với thử nghiệm cá nhân, giữ app ở Testing và chỉ cho phép test users.

Google yêu cầu ứng dụng production có trang chủ công khai mô tả chức năng, cùng privacy policy phù hợp với cách app dùng/lưu/chia sẻ dữ liệu Google. Chuẩn bị và công khai các trang này trên domain đã xác minh trước khi gửi yêu cầu verification; xem [chính sách OAuth về domain và trang chủ](https://developers.google.com/identity/protocols/oauth2/policies) và [yêu cầu xác minh](https://developers.google.com/identity/protocols/oauth2/production-readiness/policy-compliance).

## 4. Chạy web app và Worker trên máy

### Worker local

Mở PowerShell:

```powershell
cd worker\cloudflare
npm install
Copy-Item .dev.vars.example .dev.vars
```

Sửa `worker/cloudflare/.dev.vars` bằng OAuth client mới và một device token tự tạo:

```dotenv
APP_ENV=local
WEB_ORIGIN=http://localhost:5173
GOOGLE_CLIENT_ID=...apps.googleusercontent.com
GOOGLE_CLIENT_SECRET=<client-secret>
GOOGLE_REDIRECT_URI=http://localhost:8787/api/oauth/google/callback
DEVICE_API_TOKENS={"DB001":"<random-long-device-token>"}
```

Tạo token ngẫu nhiên an toàn bằng PowerShell:

```powershell
[Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
```

Đặt kết quả vào `DEVICE_API_TOKENS`, rồi lưu file. Không commit `.dev.vars`, OAuth secrets hoặc device token. Chạy Worker:

```powershell
npm run dev
```

Worker local mở tại `http://localhost:8787`; health check là `http://localhost:8787/api/health`.

### Web app local

Mở terminal thứ hai:

```powershell
cd web
npm install
Copy-Item .env.example .env.local
```

Đảm bảo `web/.env.local` trỏ đến Worker local:

```dotenv
VITE_API_BASE_URL=http://localhost:8787
```

Chạy web app:

```powershell
npm run dev
```

Mở `http://localhost:5173/?device_id=DB001`, đăng nhập bằng một test user đã khai báo. Không đưa secret vào biến `VITE_`; các giá trị đó được đóng gói vào JavaScript gửi cho trình duyệt.

Trong môi trường local, thử OAuth trên máy tính trước. QR trên thiết bị thật cần URL HTTPS public mà điện thoại truy cập được; `localhost` trên QR sẽ trỏ về điện thoại, không phải máy tính đang chạy Worker.

## 5. Chạy mock Wokwi

Mock đặt tại `simulation/wokwi` và dùng event mẫu, driver e-paper mô phỏng, không gọi Google, Worker hoặc Wi-Fi thật.

**Trên Wokwi.com:** tạo project ESP32, chép `diagram.json`, `sketch.ino`, `libraries.txt` trong thư mục trên vào project, thêm thư viện nếu được hỏi rồi nhấn Start Simulation.

**Trong VS Code:** mở `simulation/wokwi`, cài extension Wokwi, mở PlatformIO terminal và chạy:

```powershell
cd simulation\wokwi
pio run -e esp32dev
```

Chọn Start Wokwi Simulation trong extension. Nút MODE đổi trang; UP/DOWN và OK điều khiển demo. Mock xác nhận layout/button flow ở mức mô phỏng, không xác minh production firmware hay phần cứng thật.

## 6. Deploy bản riêng lên Cloudflare

Tạo Worker và Pages trong tài khoản Cloudflare của bạn. Không dùng KV namespace, OAuth credentials, device token hoặc URL production của nhóm này.

### Deploy Worker

1. Cài Wrangler qua dependency dự án và đăng nhập Cloudflare:

   ```powershell
   cd worker\cloudflare
   npx wrangler login
   ```

2. Tạo hai KV namespaces riêng cho môi trường production và preview:

   ```powershell
   npx wrangler kv namespace create DESK_BUDDY_STORE
   npx wrangler kv namespace create DESK_BUDDY_STORE_PREVIEW
   ```

3. Chép từng namespace ID Wrangler trả về vào `worker/cloudflare/wrangler.toml`, ở binding `DESK_BUDDY_STORE` tương ứng trong production và preview. Giữ nguyên tên binding `DESK_BUDDY_STORE`; Worker cần binding này để lưu OAuth state, session và kết nối Calendar. Không trỏ local/preview vào namespace production.
4. Chọn một URL public cho Worker. Mặc định tên Worker là `desk-buddy-api`; sau deploy URL thường là `https://desk-buddy-api.<account-subdomain>.workers.dev`.
5. Deploy Worker:

   ```powershell
   npm run deploy
   ```

6. Đặt các giá trị phía server bằng Cloudflare Worker secrets/variables (không phải biến `VITE_`):

   ```powershell
   npx wrangler secret put GOOGLE_CLIENT_ID
   npx wrangler secret put GOOGLE_CLIENT_SECRET
   npx wrangler secret put GOOGLE_REDIRECT_URI
   npx wrangler secret put DEVICE_API_TOKENS
   ```

   Nhập `GOOGLE_REDIRECT_URI` là URL callback HTTPS ở bước Google OAuth; `DEVICE_API_TOKENS` là JSON object map device ID tới token, ví dụ `{"DB001":"<random-long-device-token>"}`. Có thể đặt các mục này trong Cloudflare Dashboard → Worker → Settings → Variables and Secrets. Đặt `WEB_ORIGIN` trong `wrangler.toml` trùng origin của Pages, không thêm path.
7. Nếu đã deploy trước khi thêm secrets, deploy lại sau khi cập nhật cấu hình:

   ```powershell
   npm run deploy
   ```

8. Kiểm tra `https://<worker-domain>/api/health` trả về `success: true`.

Wrangler secret được gắn vào Worker theo hướng dẫn chính thức tại [Cloudflare Workers Secrets](https://developers.cloudflare.com/workers/configuration/secrets/); cách tạo KV namespace xem [Cloudflare Workers KV](https://developers.cloudflare.com/kv/get-started/).

### Deploy Web app lên Cloudflare Pages

1. Trong Cloudflare Dashboard, tạo Pages project kết nối repository GitHub này.
2. Đặt root directory là `web`, build command là `npm run build`, output directory là `dist`.
3. Thêm build environment variable `VITE_API_BASE_URL=https://<worker-domain>` cho Production và Preview tương ứng.
4. Deploy và ghi lại URL HTTPS của Pages, ví dụ `https://<your-project>.pages.dev`.
5. Sửa `WEB_ORIGIN` trong `worker/cloudflare/wrangler.toml` thành origin Pages chính xác, không có dấu `/` cuối hoặc path; deploy Worker lại.
6. Thêm callback HTTPS của Worker vào Authorized redirect URIs trong Google OAuth client.

Khi thay URL Pages hoặc Worker, cập nhật cả Google OAuth redirect URI, Worker `WEB_ORIGIN`/`GOOGLE_REDIRECT_URI`, Pages `VITE_API_BASE_URL` và firmware `API_BASE_URL`/`WEB_APP_URL` để các phần trỏ cùng một môi trường.
Nên gắn Pages và Worker vào các hostname cùng custom domain bạn sở hữu, chẳng hạn `deskbuddy.example.com` và `api.deskbuddy.example.com`, để dùng cùng site cho cookie/session.

## 7. Build và nạp firmware thật

1. Xác nhận panel WeAct chính xác theo nhãn phía sau. Firmware hiện chọn `GxEPD2_290_GDEY029T94`; không tuyên bố tương thích với mọi revision 2.9 inch.
2. Mở `firmware/ESP32` trong VS Code/PlatformIO.
3. Tạo file local không được commit:

   ```powershell
   Copy-Item src\config.example.h src\config.h
   ```

4. Điền `API_BASE_URL`, `WEB_APP_URL`, `DEVICE_ID`, `DEVICE_API_TOKEN`, PEM root CA xác thực Worker và múi giờ UTC+7 trong `src/config.h`. Device token phải khớp mục cùng device ID trong Worker secret `DEVICE_API_TOKENS`. `WEB_APP_URL` là URL Pages gốc, không kèm query string.
5. Build trước, chưa upload:

   ```powershell
   pio run -e nodemcu-32s
   ```

   Windows có thể dùng `.\setup.ps1` từ thư mục firmware; script hỏi Worker/web URL, ID, token và file PEM rồi build.
6. Sau khi đấu nối và chọn đúng cổng COM, upload theo yêu cầu của bạn:

   ```powershell
   pio run -e nodemcu-32s --target upload
   ```

7. Theo dõi boot log:

   ```powershell
   pio device monitor --baud 115200
   ```

8. Lần đầu bật, e-paper sẽ hiển thị Wi-Fi setup AP và mật khẩu tạm. Dùng điện thoại kết nối AP `DeskBuddy-<device_id>`, mở `http://192.168.4.1`, chọn mạng và lưu. Sau đó vào màn Account, quét QR bằng điện thoại, đăng nhập Google, chọn Calendar.

`src/config.h` chứa device token và cấu hình riêng, nên không commit. Firmware prototype nhúng token trong flash; bất kỳ ai có thiết bị vật lý có thể trích xuất nó. Tạo token riêng cho từng thiết bị, thu hồi/đổi token nếu thiết bị thất lạc. Không dùng token của nhóm cho bản fork.

## 8. Kiểm tra các thành phần

Các lệnh sau chạy trong thư mục tương ứng:

```powershell
# Web
cd web
npm run build
npm run test:timezone

# Worker
cd ..\worker\cloudflare
npm run check
npm run test:timezone

# Firmware compile only, không nạp
cd ..\..\firmware\ESP32
pio run -e nodemcu-32s

# Native tests; cần toolchain gcc/g++
pio test -e native
```

Ghi lại riêng kết quả build, mô phỏng và kiểm tra phần cứng. Build không chứng minh OAuth hoạt động, mô phỏng không xác minh Calendar thật, và chỉ kiểm tra trên thiết bị mới xác nhận wiring/display/RTC thực tế.

## 9. Nếu không chạy được

- `redirect_uri_mismatch`: so sánh chính xác callback trong Google Console với `GOOGLE_REDIRECT_URI` Worker.
- Đăng nhập báo app chưa xác minh hoặc tài khoản không được phép: thêm tài khoản vào OAuth Test users khi app ở trạng thái Testing; dùng production verification nếu mở cho người dùng ngoài.
- Web báo Worker/CORS: đảm bảo Pages origin khớp `WEB_ORIGIN` và `VITE_API_BASE_URL` trỏ đúng Worker.
- Worker báo thiếu KV binding: kiểm tra `DESK_BUDDY_STORE` được bind và namespace ID đúng cho môi trường đó.
- ESP32 build lỗi config: tạo local `src/config.h` từ `config.example.h`.
- E-paper trắng: kiểm tra nhãn revision, driver class, nguồn 3.3 V và dây SPI theo [HARDWARE_DESIGN.md](HARDWARE_DESIGN.md).
- Calendar không hiện: kiểm tra `device_id`, kết nối Calendar với cùng thiết bị trên web, `DEVICE_API_TOKENS`, token trong `config.h` và Serial log.

## Tài liệu chi tiết

- [Sơ đồ hệ thống](ARCHITECTURE.md)
- [API contract](API.md)
- [Thiết kế phần cứng](HARDWARE_DESIGN.md) và [sơ đồ đấu dây](HARDWARE_WIRING.md)
- [Hướng dẫn Worker](worker/cloudflare/README.md)
- [Hướng dẫn firmware ESP32](firmware/ESP32/README.md)
- [Trạng thái đã xác minh và các việc còn mở](STATUS.md)
