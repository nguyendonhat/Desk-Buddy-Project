# Desk-Buddy-Project
Github của nhóm Tour de Force

## Chạy skeleton local

Yêu cầu: Node.js 20 trở lên và npm.

### Web Dashboard

```powershell
cd web
npm install
npm run dev
```

Mở URL Vite in ra (mặc định `http://localhost:5173`). Web hiện chỉ là trang kiểm tra local, chưa có Google OAuth hoặc Calendar. Có thể sao chép `web/.env.example` thành `web/.env.local` để cấu hình `VITE_API_BASE_URL`; biến này chỉ dành cho URL public của API, không đặt secret vào biến `VITE_`.

### Cloudflare Worker

```powershell
cd worker/cloudflare
npm install
npm run dev
```

Wrangler mặc định phục vụ Worker tại `http://localhost:8787`. Kiểm tra skeleton bằng cách mở `http://localhost:8787/api/health`. Các route `/api/calendar` và `/api/device/command` hiện trả HTTP 501 để thể hiện rằng API contract đã có trong `API.md` nhưng chưa được triển khai. OAuth và Google Calendar chưa được triển khai.

Nếu task sau cần biến môi trường local, sao chép `.dev.vars.example` thành `.dev.vars`. File `.dev.vars` bị Git ignore; không commit credential hoặc secret.

### TypeScript check

```powershell
cd worker/cloudflare
npm run check
```
