import './style.css';

const apiBaseUrl = (import.meta.env.VITE_API_BASE_URL || 'http://localhost:8787').replace(/\/$/, '');
const params = new URLSearchParams(window.location.search);
const requestedDeviceId = params.get('device_id');
const deviceId = requestedDeviceId && /^[A-Za-z0-9_-]{1,64}$/.test(requestedDeviceId) ? requestedDeviceId : 'DB001';
const statusElement = document.querySelector('#connection-status');
const messageElement = document.querySelector('#message');
const accountEmailElement = document.querySelector('#account-email');
const connectButton = document.querySelector('#connect-button');
const logoutButton = document.querySelector('#logout-button');
const calendarSection = document.querySelector('#calendar');
const countElement = document.querySelector('#event-count');
const calendarMessage = document.querySelector('#calendar-message');
const eventList = document.querySelector('#event-list');
const syncButton = document.querySelector('#sync-button');
const calendarSelect = document.querySelector('#calendar-select');

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

function eventDateKey(value) {
  return value.slice(0, 10);
}

function formatDayHeading(dateKey) {
  const date = new Date(`${dateKey}T12:00:00`);
  const weekday = new Intl.DateTimeFormat('vi-VN', { weekday: 'long' }).format(date);
  const [year, month, day] = dateKey.split('-');
  return `${weekday}, ${day}/${month}/${year}`;
}

function formatEventTime(value) {
  if (/^\d{4}-\d{2}-\d{2}$/.test(value)) return 'Cả ngày';
  const match = value.match(/T(\d{2}):(\d{2})/);
  return match ? `${match[1]}:${match[2]}` : value;
}

function showCalendarError(message) {
  calendarMessage.textContent = message;
  calendarMessage.classList.add('error');
}

function renderEvents(events) {
  eventList.replaceChildren();
  countElement.textContent = `Đã lấy ${events.length} sự kiện trong 7 ngày tới`;

  if (events.length === 0) {
    calendarMessage.textContent = 'Không có sự kiện trong 7 ngày tới';
    calendarMessage.classList.remove('error');
    return;
  }

  calendarMessage.textContent = '';
  calendarMessage.classList.remove('error');
  const orderedEvents = [...events].sort((left, right) => new Date(left.start) - new Date(right.start));
  const eventsByDay = new Map();
  for (const event of orderedEvents) {
    const dateKey = eventDateKey(event.start);
    if (!eventsByDay.has(dateKey)) eventsByDay.set(dateKey, []);
    eventsByDay.get(dateKey).push(event);
  }

  for (const [dateKey, dayEvents] of eventsByDay) {
    const dayItem = document.createElement('li');
    dayItem.className = 'event-day';
    const dayHeading = document.createElement('h3');
    dayHeading.className = 'day-heading';
    dayHeading.textContent = formatDayHeading(dateKey);
    const dayList = document.createElement('ul');
    dayList.className = 'day-events';

    for (const event of dayEvents) {
      const item = document.createElement('li');
      item.className = 'event-item';
      const time = document.createElement('p');
      time.className = 'event-time';
      time.textContent = /^\d{4}-\d{2}-\d{2}$/.test(event.start)
        ? 'Cả ngày'
        : `${formatEventTime(event.start)} – ${formatEventTime(event.end)}`;
      const title = document.createElement('h4');
      title.className = 'event-title';
      title.textContent = event.title || '(Không có tiêu đề)';
      item.append(time, title);
      if (event.description) {
        const description = document.createElement('p');
        description.className = 'event-description';
        description.textContent = event.description;
        item.append(description);
      }
      dayList.append(item);
    }

    dayItem.append(dayHeading, dayList);
    eventList.append(dayItem);
  }
}

async function loadCalendar() {
  if (!calendarSelect.value) {
    showCalendarError('Chọn một calendar để tải sự kiện.');
    return;
  }
  syncButton.disabled = true;
  syncButton.textContent = 'Đang đồng bộ…';
  calendarMessage.classList.remove('error');
  calendarMessage.textContent = 'Đang lấy sự kiện…';
  try {
    const calendarUrl = new URL(`${apiBaseUrl}/api/calendar`);
    calendarUrl.searchParams.set('device_id', deviceId);
    const response = await fetch(calendarUrl, { credentials: 'include', cache: 'no-store' });
    const result = await response.json();
    if (!response.ok) {
      if (response.status === 401) {
        statusElement.textContent = 'Kết nối Google hết hạn hoặc bị thu hồi. Hãy đăng nhập lại.';
        statusElement.classList.remove('connected');
        connectButton.textContent = 'Đăng nhập lại bằng Google';
      }
      throw new Error(result.error || 'Không thể lấy dữ liệu Google Calendar.');
    }
    renderEvents(result.events || []);
  } catch (error) {
    showCalendarError(error instanceof Error ? error.message : 'Không thể lấy dữ liệu Google Calendar.');
  } finally {
    syncButton.disabled = false;
    syncButton.textContent = 'Đồng bộ Google Calendar';
  }
}

async function loadCalendars() {
  calendarSelect.disabled = true;
  calendarSelect.replaceChildren(new Option('Đang tải danh sách calendar…', ''));
  try {
    const calendarsUrl = new URL(`${apiBaseUrl}/api/calendars`);
    calendarsUrl.searchParams.set('device_id', deviceId);
    const response = await fetch(calendarsUrl, { credentials: 'include', cache: 'no-store' });
    const result = await response.json();
    if (!response.ok) throw new Error(result.error || 'Không thể tải danh sách calendar.');

    calendarSelect.replaceChildren(new Option('Chọn Google Calendar…', ''));
    for (const calendar of result.calendars || []) {
      const name = calendar.summary || 'Calendar không có tên';
      const option = new Option(calendar.primary ? `${name} (Lịch chính)` : name, calendar.id);
      calendarSelect.add(option);
    }

    const selectedCalendarId = result.selected_calendar_id || '';
    if (selectedCalendarId && [...calendarSelect.options].some((option) => option.value === selectedCalendarId)) {
      calendarSelect.value = selectedCalendarId;
      calendarSelect.disabled = false;
      await loadCalendar();
    } else {
      calendarSelect.disabled = result.calendars?.length === 0;
      syncButton.disabled = true;
      eventList.replaceChildren();
      countElement.textContent = 'Chưa đồng bộ';
      calendarMessage.classList.remove('error');
      calendarMessage.textContent = result.calendars?.length
        ? 'Chọn một calendar để xem sự kiện.'
        : 'Tài khoản chưa có calendar nào khả dụng.';
    }
  } catch (error) {
    calendarSelect.replaceChildren(new Option('Không tải được danh sách calendar', ''));
    showCalendarError(error instanceof Error ? error.message : 'Không thể tải danh sách calendar.');
  }
}

async function saveCalendarSelection() {
  const calendarId = calendarSelect.value;
  if (!calendarId) {
    syncButton.disabled = true;
    eventList.replaceChildren();
    countElement.textContent = 'Chưa đồng bộ';
    calendarMessage.textContent = 'Chọn một calendar để xem sự kiện.';
    return;
  }

  calendarSelect.disabled = true;
  syncButton.disabled = true;
  calendarMessage.classList.remove('error');
  calendarMessage.textContent = 'Đang lưu lựa chọn calendar…';
  try {
    const response = await fetch(`${apiBaseUrl}/api/calendar/selection`, {
      method: 'POST',
      credentials: 'include',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ device_id: deviceId, calendar_id: calendarId }),
    });
    const result = await response.json();
    if (!response.ok) throw new Error(result.error || 'Không thể lưu lựa chọn calendar.');
    calendarSelect.disabled = false;
    await loadCalendar();
  } catch (error) {
    calendarSelect.disabled = false;
    showCalendarError(error instanceof Error ? error.message : 'Không thể lưu lựa chọn calendar.');
  }
}

async function loadConnectionStatus() {
  try {
    const statusUrl = new URL(`${apiBaseUrl}/api/oauth/connection`);
    statusUrl.searchParams.set('device_id', deviceId);
    const response = await fetch(statusUrl, { credentials: 'include', cache: 'no-store' });
    if (!response.ok) throw new Error('Connection status request failed');
    const result = await response.json();
    if (result.connected) {
      statusElement.textContent = 'Google Calendar đã kết nối';
      statusElement.classList.add('connected');
      accountEmailElement.textContent = result.email ? `Tài khoản: ${result.email}` : '';
      accountEmailElement.hidden = !result.email;
      connectButton.hidden = true;
      logoutButton.hidden = false;
      calendarSection.hidden = false;
      await loadCalendars();
    } else {
      statusElement.textContent = 'Chưa kết nối Google Calendar.';
      statusElement.classList.remove('connected');
      accountEmailElement.textContent = '';
      accountEmailElement.hidden = true;
      connectButton.textContent = 'Đăng nhập bằng Google';
      connectButton.hidden = false;
      logoutButton.hidden = true;
      calendarSection.hidden = true;
    }
  } catch {
    statusElement.textContent = 'Không thể kiểm tra kết nối. Hãy kiểm tra Worker local.';
  }
}

async function logoutFromGoogle() {
  logoutButton.disabled = true;
  logoutButton.textContent = 'Đang đăng xuất…';
  messageElement.textContent = '';
  try {
    const response = await fetch(`${apiBaseUrl}/api/oauth/logout`, {
      method: 'POST',
      credentials: 'include',
    });
    const result = await response.json();
    if (!response.ok || !result.success) throw new Error(result.error || 'Không thể đăng xuất.');

    const statusUrl = new URL(`${apiBaseUrl}/api/oauth/connection`);
    statusUrl.searchParams.set('device_id', deviceId);
    const statusResponse = await fetch(statusUrl, { credentials: 'include', cache: 'no-store' });
    if (!statusResponse.ok) throw new Error('Không thể xác nhận trạng thái đăng xuất.');
    const status = await statusResponse.json();
    if (status.connected) throw new Error('Phiên Google vẫn đang hoạt động. Hãy thử đăng xuất lại.');

    statusElement.textContent = 'Chưa kết nối Google Calendar.';
    statusElement.classList.remove('connected');
    accountEmailElement.textContent = '';
    accountEmailElement.hidden = true;
    connectButton.textContent = 'Đăng nhập bằng Google';
    connectButton.hidden = false;
    logoutButton.hidden = true;
    calendarSection.hidden = true;
    calendarSelect.replaceChildren(new Option('Chọn Google Calendar…', ''));
    calendarSelect.disabled = true;
    syncButton.disabled = true;
    eventList.replaceChildren();
    countElement.textContent = 'Chưa đồng bộ';
    calendarMessage.classList.remove('error');
    calendarMessage.textContent = '';
    messageElement.textContent = '';
  } catch (error) {
    messageElement.textContent = error instanceof Error
      ? `Đăng xuất chưa thành công: ${error.message}`
      : 'Đăng xuất chưa thành công. Vui lòng thử lại.';
  } finally {
    logoutButton.disabled = false;
    logoutButton.textContent = 'Đăng xuất';
  }
}

if (oauthResult) {
  params.delete('oauth');
  const cleanQuery = params.toString();
  window.history.replaceState({}, '', `${window.location.pathname}${cleanQuery ? `?${cleanQuery}` : ''}${window.location.hash}`);
}

syncButton.addEventListener('click', loadCalendar);
calendarSelect.addEventListener('change', saveCalendarSelection);
logoutButton.addEventListener('click', logoutFromGoogle);
loadConnectionStatus();
