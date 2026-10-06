import './style.css';

const apiBaseUrl = (import.meta.env.VITE_API_BASE_URL || 'http://localhost:8787').replace(/\/$/, '');
const params = new URLSearchParams(window.location.search);
const requestedDeviceId = params.get('device_id');
const deviceId = requestedDeviceId && /^[A-Za-z0-9_-]{1,64}$/.test(requestedDeviceId) ? requestedDeviceId : 'DB001';
const statusElement = document.querySelector('#connection-status');
const messageElement = document.querySelector('#message');
const connectButton = document.querySelector('#connect-button');

document.querySelector('#device-id').textContent = deviceId;
const startUrl = new URL(`${apiBaseUrl}/api/oauth/google/start`);
startUrl.searchParams.set('device_id', deviceId);
connectButton.href = startUrl.toString();

const oauthResult = params.get('oauth');
if (oauthResult === 'denied') {
  messageElement.textContent = 'Bạn đã từ chối quyền đọc lịch. Chưa có tài khoản nào được kết nối.';
} else if (oauthResult === 'error') {
  messageElement.textContent = 'Không thể hoàn tất kết nối Google. Vui lòng thử lại.';
}

async function loadConnectionStatus() {
  try {
    const statusUrl = new URL(`${apiBaseUrl}/api/oauth/connection`);
    statusUrl.searchParams.set('device_id', deviceId);
    const response = await fetch(statusUrl, { credentials: 'include' });
    if (!response.ok) throw new Error('Connection status request failed');
    const result = await response.json();
    if (result.connected) {
      statusElement.textContent = `Google Calendar đã được kết nối${result.email ? ` (${result.email})` : ''}.`;
      statusElement.classList.add('connected');
      connectButton.textContent = 'Kết nối lại bằng Google';
    } else {
      statusElement.textContent = 'Chưa kết nối Google Calendar.';
    }
  } catch {
    statusElement.textContent = 'Không thể kiểm tra kết nối. Hãy kiểm tra Worker local.';
  }
}

if (oauthResult) {
  params.delete('oauth');
  const cleanQuery = params.toString();
  window.history.replaceState({}, '', `${window.location.pathname}${cleanQuery ? `?${cleanQuery}` : ''}${window.location.hash}`);
}

loadConnectionStatus();
