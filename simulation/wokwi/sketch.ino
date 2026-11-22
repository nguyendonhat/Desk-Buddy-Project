#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <SPI.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <Wire.h>

// UI-flow mock only. RTC is a DS1307 simulator proxy, not a real DS3231.
constexpr int EPD_CS=27, EPD_DC=26, EPD_RST=25, EPD_BUSY=34;
constexpr int MODE_BTN=32, UP_BTN=33, DOWN_BTN=19, OK_BTN=35;
constexpr int RGB_RED=13, RGB_GREEN=14, RGB_BLUE=16, BUZZER_PIN=17;
GxEPD2_BW<GxEPD2_290_BS,GxEPD2_290_BS::HEIGHT> epaper(GxEPD2_290_BS(EPD_CS,EPD_DC,EPD_RST,EPD_BUSY));
U8G2_FOR_ADAFRUIT_GFX text;
LiquidCrystal_I2C lcd(0x27,16,2);
RTC_DS1307 rtc;
DateTime softwareStart(2026,11,7,8,0,0);
DateTime eventToday(2026,11,7,14,30,0), eventTomorrow(2026,11,8,9,0,0);
uint32_t clockStart=0,lastClock=0,pomoEnd=0;
bool rtcOk=false,pomoRunning=false;
uint8_t screen=0,noteView=0,notePage=0,selectedEvent=0,pomoField=0,accountField=0,timeField=0;
uint16_t selectedDay=7,pomoMinutes=25,cycles=4,breakMinutes=5;
bool noteExitSelected=false;
bool prevHigh[4]={true,true,true,true};
const int buttons[4]={MODE_BTN,UP_BTN,DOWN_BTN,OK_BTN};
const char* noteText="Bring the Desk Buddy prototype to class. Check the e-paper wiring, test Google Calendar account linking, then review the next Pomodoro session with the project team.";
DateTime currentTime(){return rtcOk?rtc.now():softwareStart+TimeSpan((millis()-clockStart)/1000);}
uint8_t daysInMonth(int year,int month){static const uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};if(month==2&&((year%4==0&&year%100!=0)||year%400==0))return 29;return days[month-1];}
uint8_t visibleEventCount(){DateTime now=currentTime();if(selectedDay==now.day())return 1;if(selectedDay==now.day()+1&&selectedDay<=daysInMonth(now.year(),now.month()))return 1;return 0;}
void drawHeader(const char* title,const char* right=""){text.setCursor(8,16);text.print(title);if(right[0]){text.setCursor(210,16);text.print(right);}epaper.drawLine(6,22,290,22,GxEPD_BLACK);}
void drawCalendar(){
  DateTime now=currentTime();const uint8_t firstWeekday=DateTime(now.year(),now.month(),1,0,0,0).dayOfTheWeek();const uint8_t monthDays=daysInMonth(now.year(),now.month());
  drawHeader("NOTE - CHON NGAY","UP/DOWN  OK");
  text.setCursor(8,38);text.print(now.month());text.print("/");text.print(now.year());
  const char* weekdays[]={"S","M","T","W","T","F","S"};
  for(uint8_t c=0;c<7;++c){text.setCursor(10+c*39,51);text.print(weekdays[c]);}
  for(uint8_t d=1;d<=monthDays;++d){uint8_t cell=firstWeekday+d-1,row=cell/7,col=cell%7;int x=7+col*39,y=55+row*11;if(d==selectedDay)epaper.drawRect(x,y-9,34,11,GxEPD_BLACK);text.setCursor(x+5,y);if(d==now.day())text.print("*");text.print(d);}
  text.setCursor(8,121);text.print("OK: mo su kien   MODE: tab");
}
void drawEvents(){
  char date[20];snprintf(date,sizeof(date),"%02u/%02u/%04u",selectedDay,currentTime().month(),currentTime().year());drawHeader("NOTE - SU KIEN",date);
  if(!visibleEventCount()){text.setCursor(8,55);text.print("Khong co su kien ngay nay");text.setCursor(8,120);text.print("OK: ve lich  MODE: tab");return;}
  DateTime event=selectedDay==currentTime().day()?eventToday:eventTomorrow;
  text.setCursor(8,51);text.print("> ");text.print(event.hour()<10?"0":"");text.print(event.hour());text.print(":");text.print(event.minute()<10?"0":"");text.print(event.minute());text.print("  Hop nhom Desk Buddy");
  text.setCursor(8,75);text.print("1 su kien");text.setCursor(8,120);text.print("OK: xem note  MODE: tab");
}
void drawNote(){
  drawHeader("NOTE - HOP NHOM",noteExitSelected?"[ THOAT ]":"UP/DOWN");
  const size_t length=strlen(noteText),pageSize=84,totalPages=(length+pageSize-1)/pageSize,offset=notePage*pageSize;
  for(uint8_t line=0;line<4;++line){size_t start=offset+line*21;if(start>=length)break;text.setCursor(8,43+line*17);for(uint8_t j=0;j<21&&noteText[start+j];++j)text.print(noteText[start+j]);}
  text.setCursor(8,120);text.print("Trang ");text.print(notePage+1);text.print("/");text.print(totalPages);text.print("  OK: chon thoat");
}
void drawUi(){
  epaper.setRotation(1);epaper.setFullWindow();epaper.firstPage();do{
    epaper.fillScreen(GxEPD_WHITE);epaper.drawRect(0,0,epaper.width(),epaper.height(),GxEPD_BLACK);
    text.begin(epaper);text.setFontMode(1);text.setFontDirection(0);text.setForegroundColor(GxEPD_BLACK);text.setBackgroundColor(GxEPD_WHITE);text.setFont(u8g2_font_6x12_tf);
    if(screen==0){if(noteView==0)drawCalendar();else if(noteView==1)drawEvents();else drawNote();}
    else if(screen==1){drawHeader("POMODORO",pomoRunning?"RUNNING":"SETUP");text.setCursor(12,50);text.print(pomoField==0?"> ":"  ");text.print("Cycles: ");text.print(cycles);text.setCursor(12,70);text.print(pomoField==1?"> ":"  ");text.print("Focus: ");text.print(pomoMinutes);text.print(" min");text.setCursor(12,90);text.print(pomoField==2?"> ":"  ");text.print("Break: ");text.print(breakMinutes);text.print(" min");text.setCursor(180,115);text.print(pomoRunning?"PAUSE":"START");text.setCursor(8,121);text.print("UP/DOWN edit OK field/start");}
    else if(screen==2){drawHeader("ACCOUNT","CONNECTED");text.setCursor(12,55);text.print(accountField==0?"> Update events":"  Update events");text.setCursor(12,78);text.print(accountField==1?"> Delete data":"  Delete data");text.setCursor(12,112);text.print("OK select  MODE next tab");}
    else {drawHeader("TIME SETTING","RTC");text.setCursor(12,54);text.print("Manual / Automatic NTP");text.setCursor(12,78);text.print("Set date and time");text.setCursor(12,112);text.print("UP/DOWN  OK  MODE exit");}
  }while(epaper.nextPage());epaper.hibernate();
}
void drawClock(){DateTime n=currentTime();char a[17],b[17];snprintf(a,sizeof(a),"%02d/%02d/%04d",n.day(),n.month(),n.year());snprintf(b,sizeof(b),"%02d:%02d:%02d DEMO",n.hour(),n.minute(),n.second());lcd.setCursor(0,0);lcd.print("                ");lcd.setCursor(0,0);lcd.print(a);lcd.setCursor(0,1);lcd.print("                ");lcd.setCursor(0,1);lcd.print(b);}
void handleNote(uint8_t key){if(noteView==0){if(key==1&&selectedDay<daysInMonth(currentTime().year(),currentTime().month()))++selectedDay;else if(key==2&&selectedDay>1)--selectedDay;else if(key==3)noteView=1;}else if(noteView==1){if(key==1||key==2)selectedEvent=0;else if(key==3){if(visibleEventCount()){noteView=2;notePage=0;noteExitSelected=false;}else noteView=0;}}else if(noteView==2){size_t pages=(strlen(noteText)+83)/84;if(key==1){if(noteExitSelected){noteExitSelected=false;notePage=0;}else if(notePage+1<pages)++notePage;else noteExitSelected=true;}else if(key==2){if(noteExitSelected){noteExitSelected=false;notePage=0;}else if(notePage>0)--notePage;}else if(key==3&&noteExitSelected){noteView=1;noteExitSelected=false;}}}
void setup(){Serial.begin(115200);pinMode(RGB_RED,OUTPUT);pinMode(RGB_GREEN,OUTPUT);pinMode(RGB_BLUE,OUTPUT);pinMode(BUZZER_PIN,OUTPUT);for(int p:buttons)pinMode(p,INPUT_PULLUP);Wire.begin(21,22);lcd.init();lcd.backlight();rtcOk=rtc.begin();if(rtcOk){if(!rtc.isrunning())rtc.adjust(DateTime(F(__DATE__),F(__TIME__)));softwareStart=rtc.now();}clockStart=millis();selectedDay=currentTime().day();SPI.begin(18,-1,23,EPD_CS);epaper.init(115200);drawUi();drawClock();Serial.println("Desk Buddy UI simulator ready. MODE/UP/DOWN/OK");}
void loop(){const uint32_t ms=millis();bool pressed[4];for(int i=0;i<4;++i){bool high=digitalRead(buttons[i])==HIGH;pressed[i]=prevHigh[i]&&!high;prevHigh[i]=high;}uint8_t key=255;if(pressed[0]){screen=(screen+1)%4;drawUi();}else if(pressed[1])key=1;else if(pressed[2])key=2;else if(pressed[3])key=3;if(key!=255){if(screen==0)handleNote(key);else if(screen==1){if(key==1&&pomoField<3)++pomoField;else if(key==2&&pomoField>0)--pomoField;else if(key==3&&pomoField==3){pomoRunning=!pomoRunning;pomoEnd=ms+pomoMinutes*60000UL;}else if(key==3)pomoField=(pomoField+1)%4;if(key==1&&pomoField==0&&cycles<12)++cycles;if(key==2&&pomoField==0&&cycles>1)--cycles;if(key==1&&pomoField==1&&pomoMinutes<60)++pomoMinutes;if(key==2&&pomoField==1&&pomoMinutes>1)--pomoMinutes;if(key==1&&pomoField==2&&breakMinutes<60)++breakMinutes;if(key==2&&pomoField==2&&breakMinutes>1)--breakMinutes;}else if(screen==2){if(key==1||key==2)accountField=1-accountField;else if(key==3&&accountField==0)Serial.println("Mock: update events");else if(key==3&&accountField==1)Serial.println("Mock: delete local calendar data");}drawUi();}if(ms-lastClock>=1000){lastClock=ms;drawClock();}if(pomoRunning&&static_cast<int32_t>(pomoEnd-ms)<=0){pomoRunning=false;digitalWrite(BUZZER_PIN,HIGH);delay(250);digitalWrite(BUZZER_PIN,LOW);drawUi();}delay(20);}
