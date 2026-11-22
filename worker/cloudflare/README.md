# Cloudflare Worker

Worker này thực hiện Google OAuth, lưu trạng thái đăng nhập/liên kết Calendar trong Cloudflare KV, chuẩn hóa event về Desk Buddy API và cung cấp Calendar cho firmware ESP32.

## Bắt đầu

Hướng dẫn đầy đủ cho người mới, gồm tạo Google OAuth client, cấu hình `.dev.vars`, tạo KV namespace cho Cloudflare, deploy Worker/Pages và ghép firmware nằm ở [SETUP_GUIDE.md](../../SETUP_GUIDE.md).

## Chạy local

Yêu cầu Node.js LTS 22 trở lên và npm.

```powershell
cd worker\cloudflare
npm install
Copy-Item .dev.vars.example .dev.vars
```

Giữ `APP_ENV=local` và `WEB_ORIGIN=http://localhost:5173` trong `.dev.vars.example`; điền Google OAuth client ID/secret, callback local và `DEVICE_API_TOKENS`, rồi chạy:

```powershell
npm run dev
```

Worker local chạy tại `http://localhost:8787`; health endpoint là `/api/health`. Local mode dùng KV emulation riêng. Không dùng namespace production cho dev/preview.

## Cloudflare binding và môi trường

Worker yêu cầu KV binding tên `DESK_BUDDY_STORE` để lưu OAuth state, browser session và token Calendar. Tạo namespace riêng cho mỗi tài khoản Cloudflare/môi trường và thay các ID mẫu/cũ trong `wrangler.toml` bằng namespace ID của bạn. Tách production và preview để dữ liệu đăng nhập thử không lẫn với tài khoản người dùng thật.

Cloudflare Worker cần các biến `WEB_ORIGIN` (origin của Pages, không kèm path) và `GOOGLE_REDIRECT_URI`, cùng secrets `GOOGLE_CLIENT_ID`, `GOOGLE_CLIENT_SECRET`, `DEVICE_API_TOKENS`. Mỗi device ID phải có token riêng trong JSON của `DEVICE_API_TOKENS`; firmware gửi token đó qua `Authorization: Bearer ...`.

Không dùng URL, KV namespace, token hoặc OAuth client thuộc nhóm dự án trong bản fork. Không log hoặc commit OAuth token, `.dev.vars` hay secret. Chi tiết cách tạo KV và đặt secret xem [Cloudflare Workers KV](https://developers.cloudflare.com/kv/get-started/) và [Workers Secrets](https://developers.cloudflare.com/workers/configuration/secrets/).

## Kiểm tra

```powershell
npm run check
npm run test:timezone
```

Timezone test kiểm tra tuần lịch Monday–Monday theo giờ Việt Nam và metadata sự kiện. Kiểm tra local không xác minh OAuth production hay API Calendar của tài khoản thật.
