# Desk-Buddy-Project
Github của nhóm Tour de Force

## Chạy local

Yêu cầu: Node.js 20 trở lên và npm.

### 1. Cấu hình Google OAuth

Trong Google Cloud Console, tạo OAuth Client ID loại **Web application**. Thêm Redirect URI chính xác sau vào Authorized redirect URIs:

```text
http://localhost:8787/api/oauth/google/callback
```

Sao chép `worker/cloudflare/.dev.vars.example` thành `worker/cloudflare/.dev.vars`, sau đó thay các giá trị mẫu:

```dotenv
GOOGLE_CLIENT_ID=...apps.googleusercontent.com
GOOGLE_CLIENT_SECRET=...
GOOGLE_REDIRECT_URI=http://localhost:8787/api/oauth/google/callback
```

Không đưa Client Secret vào Web, QR hoặc Git. OAuth consent screen cần được cấu hình và thêm tài khoản thử nghiệm nếu ứng dụng còn ở chế độ Testing. Worker yêu cầu scope `openid`, `email` và `https://www.googleapis.com/auth/calendar.readonly`.

### 2. Chạy Cloudflare Worker

Trong PowerShell:

```powershell
cd D:\Desk-Buddy-Project\worker\cloudflare
npm install
npm run dev
```

Worker chạy tại `http://localhost:8787`. Kiểm tra health tại `http://localhost:8787/api/health`.

### 3. Chạy Web

Mở PowerShell khác:

```powershell
cd D:\Desk-Buddy-Project\web
npm install
npm run dev
```

Mở `http://localhost:5173`. Trang Connect mặc định dùng `device_id=DB001`; có thể mở `http://localhost:5173/?device_id=DB001` như URL trong QR. Nếu Worker ở URL khác, sao chép `web/.env.example` thành `web/.env.local`, đặt `VITE_API_BASE_URL` thành URL public của Worker rồi khởi động lại Vite. Không đặt secret/token vào biến `VITE_`.

### 4. Kiểm tra OAuth

1. Mở trang Connect và chọn **Đăng nhập bằng Google**.
2. Chọn tài khoản Google và cho phép quyền đọc Calendar.
3. Google chuyển về Worker callback; Worker xử lý code server-side rồi chuyển về Web.
4. Web truy vấn connection status bằng session cookie HttpOnly. Khi thành công sẽ hiển thị “Google Calendar đã được kết nối”.
5. Để kiểm tra từ chối, bắt đầu lại và chọn Cancel/deny. Web sẽ báo rằng quyền chưa được cấp và không hiển thị trạng thái Connected.

Worker dùng OAuth state ngẫu nhiên, hết hạn sau 10 phút và chỉ dùng một lần, cùng PKCE. Callback lỗi/từ chối không trả token cho Web. OAuth token được giữ trong memory server-side cho task Calendar sau này; không có endpoint đọc token. Calendar API chưa được triển khai.

### Giới hạn development và device linking

Worker hiện lưu authorization tạm thời, session, connection và token trong memory của isolate. Dữ liệu có thể mất khi Worker restart/evict và không chia sẻ đáng tin cậy giữa nhiều isolate; đây chỉ là giải pháp local development. Trước deploy hoặc dùng nhiều thiết bị, cần bổ sung storage bền/được bảo vệ cho mapping device-account, session và refresh token, ví dụ Cloudflare D1 với mã hóa token thích hợp hoặc dịch vụ lưu secret chuyên dụng.

Hiện chưa có firmware/pairing credential. `device_id` chỉ chọn thiết bị, không phải secret; biết `DB001` không cho phép đọc status vì status cần browser session và chưa có Calendar endpoint. Tuy nhiên, người biết ID vẫn có thể tự khởi tạo OAuth và cố liên kết tài khoản của họ với ID đó. Trước sử dụng thực tế, thiết bị cần cấp pairing nonce ngẫu nhiên, ngắn hạn, dùng một lần qua kênh được xác thực; không thể bảo đảm quyền sở hữu thiết bị chỉ bằng QR có device ID.

### TypeScript check

```powershell
cd D:\Desk-Buddy-Project\worker\cloudflare
npm run check
```
