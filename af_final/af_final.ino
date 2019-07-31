#include <Wire.h>
#include <SoftwareSerial.h>
#include <Servo.h>
#include <EEPROM.h>

#define RTC 104
// SCL - pin A5
// SDA - pin A4
#define rxPin 5
#define txPin 4

// motion array length
#define motionArrLength 4
#define romStartAddr 200

SoftwareSerial BTSerial(4, 5); //블루투스의 Tx, Rx핀을 2번 3번핀으로 설정
Servo servo;

unsigned long pri_time = 0;
unsigned long delayTime = 60000;
String priFix = "";
String moreMsg = "";
String inputString = "";         
boolean stringComplete = false;  
boolean runring = false;
int readHour;
int readMin;
int dateTimes[6];   // 0:year, 1:month, 2:day, 3:hour, 4:minute, 5:address
int saved_hour_addr = 1;  
int saved_min_addr = 2;
int addrPossible = 1;
int addrMax = 10; 

extern volatile unsigned long timer0_millis; // millis reset 0;
boolean isMoving = false;

uint8_t timeAndDate[7]; // seconds, minutes, hours, day, date, month, year
// motion Array
int motionArry[4]; // angle, repeat, hour, minute
byte seconds, minutes, hours, day, date, month, year;

// rom address
int Angle_addr = 200;
int Repeat_addr = 201;

void setup() {
  servo.attach(9);
  Serial.begin(9600);
  BTSerial.begin(9600);
  Wire.begin();

  // 스트링 byte 수 설정.
  inputString.reserve(10); 
  servo.write(0);

  Serial.println("AutoFeed Start");
  Serial.print("angle : ");
  Serial.println(servo.read());
  moveDelay();
  servo.detach();

  getMotion();
}

void loop() { 
  while(!BTSerial.available() && isMoving == false) {
     // 타이머 작동하기
    actionTime();
  }
  // servo 동작 컨트롤
  moveSwitch();

  if (Serial.available()) {    
    BTSerial.write(Serial.read());  
  } 
  inputString = "";
} // loop end

void actionTime() {
  unsigned long current_time = millis();
  if(current_time - pri_time > delayTime) {
    Serial.println(current_time);
    // 움직임 값 읽어온다.
    getMotion();
    // 현재 시간을 읽어온다.
    getCurrentTime();
    
    // 움직여야 되는 시간 비교
    int mhour = byteToInt(hours);
    int mminute = byteToInt(minutes);
    Serial.print("현재 초:");
    if(mhour == motionArry[2] && mminute == motionArry[3]) {
      Serial.println("움직여야 될 시간이다.");  
      for(int i=0; i < motionArry[1]; i++) {
          // 동작 테스트
        angleAct(motionArry[0]);
        moveDelay();
        angleAct(0);
        moveDelay();
      }
    } 
    pri_time = current_time;
  }
    // 일정시간( 100분 )뒤 Reset 0 경과시간.
    if(current_time > 6000*1000) {
      timer0_millis = 0;
      pri_time = 0;
      Serial.println("Reset 0");
    }
}

void immovable() {
  isMoving = true;
}

void movable() {
  isMoving = false;
}

int byteToInt(byte bscr) {
  int r;
  r = bscr & 0xFF;
  return r;
}

// 동작 설정값 가져오기
void getMotion() {
   for(int i = 200; i < 204 ; i++){
    motionArry[i-200] = EEPROM.read(i);
    Serial.println("address:" + String(i) + "\t value = " + motionArry[i-200]);
  }
}

void getCurrentTime() {
  Wire.beginTransmission(RTC); 
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(RTC, 7); 
 
  if(Wire.available()) {
    seconds = Wire.read(); // get seconds
    minutes = Wire.read(); // get minutes
    hours   = Wire.read();   // get hours
    day     = Wire.read();
    date    = Wire.read();
    month   = Wire.read(); 
    year    = Wire.read();
       
    seconds = (((seconds & B11110000)>>4)*10 + (seconds & B00001111)); 
    minutes = (((minutes & B11110000)>>4)*10 + (minutes & B00001111)); 
    hours   = (((hours & B00110000)>>4)*10 + (hours & B00001111)); 
    day     = (day & B00000111); // 1-7
    date    = (((date & B00110000)>>4)*10 + (date & B00001111)); 
    month   = (((month & B00010000)>>4)*10 + (month & B00001111));
    year    = (((year & B11110000)>>4)*10 + (year & B00001111));
  }
}

void moveSwitch() {
   while(BTSerial.available()) 
  { 
    char inChar = (char)BTSerial.read();
    if(inChar == 'a') {
      Serial.println("angle: 0");
      angleAct(0);
      Serial.println(servo.read());
    }
    if(inChar == 'b') {
      int  curang = servo.read();   // 반환형은 int 
      if (curang >= 180) {
        break;
      } 
      int addAng = curang + 1;
      angleAct(addAng);
      Serial.println("angle Add: ");
      Serial.println(servo.read());
    }
    if(inChar == 'c') {
      int  curang = servo.read();   // 반환형은 int 
      if(curang <= 0) {
        break;
      } 
      int addAng = curang - 1;
      angleAct(addAng);
      Serial.println("angle Add: ");
      Serial.println(servo.read());
    }
    if(inChar == 'd') {
      angleAct(180);
      moveDelay();
      Serial.print("angle : ");
      Serial.println(servo.read());
      angleAct(0);
      moveDelay();
      Serial.print("angle : ");
      Serial.println(servo.read());
    }
    if(inChar == 'e') {
      int addAng = 180;
      angleAct(addAng);
      Serial.println("angle Add: ");
      Serial.println(servo.read());
    }
    // eeprom 에 각도, 반복 횟수 저장하기
    if(inChar == 'f') {
      Serial.println("angle, repeat, hour, minute rom Save:");
      motionSave();
    }
    // eeprom data read
    if(inChar == 't') {
      Serial.println("read time send:");
      //RTC의 시간 값을 읽어서 변수에 저장.
      getArduinoDate();
      // 저장된 값을 실제 폰으로 전송.
      eepromRead();
    }

    // 타이머의 시간을 맞춘다.
    if(inChar == 'q') {
      Serial.println("Timer Time Set:");
      motionSave();
    }
     // 스마트폰의 시간을 아두이노 시간으로 동기화.
    if(inChar == 'h') {
      Serial.println("Date Time Current Set:");
      setBtValToDTC();
      get3231Date();
    }

     // Seekbar Control
    if(inChar == 'i') {
      String inputStr = "";
      Serial.println("seekbar");
      while (BTSerial.available()) {
        Serial.println("seekbar read");
        char c = (char)BTSerial.read();
        inputStr += c;
        if(c == '\n') {
          break;
        }
      }
      int angleSeekbar = intFromString(inputStr);
      angleAct(angleSeekbar);
      moveDelay();
    }    
    delay(5);
  }
}

int intFromString(String str) {
  char angChar[4];
  str.toCharArray(angChar, 4);
  int angleInt = atoi(angChar);
  return angleInt;
}


// dts에 블루투스에서 읽어온 날자/시간 값을 time set하기
void setBtValToDTC() {
  byte tempC;
  while (BTSerial.available()) {
      byte temYearHi = BTSerial.read();
      delay(50);
      byte temYearLo = BTSerial.read();
      delay(50);
      byte temMonthHi = BTSerial.read();
      delay(50);
      byte temMonthLo = BTSerial.read();
      delay(50);
      byte temDateHi = BTSerial.read();
      delay(50);
      byte temDateLo = BTSerial.read();
      delay(50);
      byte temHoursHi = BTSerial.read();
      delay(50);
      byte temHoursLo = BTSerial.read();
      delay(50);
      byte temMinHi = BTSerial.read();
      delay(50);
      byte temMinLo = BTSerial.read();
      delay(50);
      byte temSecHi = BTSerial.read();
      delay(50);
      byte temSecLo = BTSerial.read();
      delay(50);
      byte temDay = BTSerial.read();
      delay(50);
      
      year    = (byte) ((temYearHi - 48) *10 +  (temYearLo - 48));
      month   = (byte) ((temMonthHi - 48) *10 +  (temMonthLo - 48));
      date    = (byte) ((temDateHi - 48) *10 +  (temDateLo - 48));
      hours   = (byte) ((temHoursHi - 48) *10 +  (temHoursLo - 48));
      minutes = (byte) ((temMinHi - 48) *10 +  (temMinLo - 48));
      seconds = (byte) ((temSecHi - 48) *10 + (temSecLo - 48));
      day     = (byte) (temDay - 48);

       Wire.beginTransmission(RTC); // address is DS3231 device address
       Wire.write(0x00); // start at register 0 읽을 위치를 설정하기 위해
       Wire.write(bcdConv(seconds));
       Wire.write(bcdConv(minutes));
       Wire.write(bcdConv(hours));
       Wire.write(bcdConv(day));
       Wire.write(bcdConv(date));
       Wire.write(bcdConv(month));
       Wire.write(bcdConv(year));
       Wire.endTransmission();
  }
}

// date를 가져오기
void getArduinoDate() {
  Wire.beginTransmission(RTC); // address is DS3231 device address
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(RTC, 7); // request seven bytes
 
  if(Wire.available()) {
    seconds = Wire.read();
    minutes = Wire.read(); 
    hours   = Wire.read();   
    day     = Wire.read();
    date    = Wire.read();
    month   = Wire.read(); 
    year    = Wire.read();
       
    seconds = (((seconds & B11110000)>>4)*10 + (seconds & B00001111)); 
    minutes = (((minutes & B11110000)>>4)*10 + (minutes & B00001111)); 
    hours   = (((hours & B00110000)>>4)*10 + (hours & B00001111)); 
    day     = (day & B00000111); // 1-7
    date    = (((date & B00110000)>>4)*10 + (date & B00001111)); 
    month   = (((month & B00010000)>>4)*10 + (month & B00001111)); 
    year    = (((year & B11110000)>>4)*10 + (year & B00001111));
    Serial.print(year);
    Serial.print("/");
    Serial.print(month);
    Serial.print("/");
    Serial.print(date);
    Serial.print(" - ");
    Serial.print(hours); 
    Serial.print(":"); 
    Serial.println(minutes); 
  
  }
  else {
    //oh noes, no data!
  }
}

void motionSave() {
  String inputValue = "";

  String dataTemp;
  
  String hourTemp = "";
  String minuteTemp = "";
  String angleTemp = "";
  String repeatTemp = "";

//  String tempStr = "";

  int count = 0;
  int splitIndex = 0;
  
  while (BTSerial.available()) {
    char inChar = (char)BTSerial.read();
    inputValue += inChar;
      
    // asciII 'z'
    if (inChar == 0x7A) {
      Serial.println(inputValue);
      
      for(int i = 0; i < 4 ; i++) {
        // 구분자 ',' 나누기
        splitIndex = inputValue.indexOf(',');
        if(splitIndex > -1) {
          dataTemp= inputValue.substring(0, splitIndex);
          Serial.println(dataTemp);   
          //angle rom address 200
          EEPROM.write(200 + i, dataTemp.toInt());
          inputValue = inputValue.substring(splitIndex + 1);
        } else {
          dataTemp= inputValue.substring(0, inputValue.length() - 1);
          Serial.println(dataTemp);
          EEPROM.write(200 + i, dataTemp.toInt());
          break;
        }
        ++count;     
      }      
    }
    delay(5);
  }
}

void currentTimeSet() 
{
  if (Serial.available()) {      
    char a = (char)Serial.read();
    
    if (a == 'h') {   //If command = "T" Set Date
      set3231Date();
      get3231Date();
      Serial.println(" ");
    }
  }
}

void set3231Date()
{
  Serial.println("set Time");
  year    = (byte) ((Serial.read() - 48) *10 +  (Serial.read() - 48));
  month   = (byte) ((Serial.read() - 48) *10 +  (Serial.read() - 48));
  date    = (byte) ((Serial.read() - 48) *10 +  (Serial.read() - 48));
  hours   = (byte) ((Serial.read() - 48) *10 +  (Serial.read() - 48));
  minutes = (byte) ((Serial.read() - 48) *10 +  (Serial.read() - 48));
  seconds = (byte) ((Serial.read() - 48) *10 + (Serial.read() - 48));
  day     = (byte) (Serial.read() - 48);

  Wire.beginTransmission(RTC);
  Wire.write(0x00); //start at register 0
  Wire.write(bcdConv(seconds));
  Wire.write(bcdConv(minutes));
  Wire.write(bcdConv(hours));
  Wire.write(bcdConv(day));
  Wire.write(bcdConv(date));
  Wire.write(bcdConv(month));
  Wire.write(bcdConv(year));
  Wire.endTransmission();
}

void get3231Date()
{
  Wire.beginTransmission(RTC ); 
  Wire.write(0x00); 
  Wire.endTransmission();
  Wire.requestFrom(RTC, 7); 
 
  if(Wire.available()) {
    seconds = Wire.read(); 
    minutes = Wire.read(); 
    hours   = Wire.read();   
    day     = Wire.read();
    date    = Wire.read();
    month   = Wire.read(); 
    year    = Wire.read();
       
    seconds = (((seconds & B11110000)>>4)*10 + (seconds & B00001111)); // convert BCD to decimal
    minutes = (((minutes & B11110000)>>4)*10 + (minutes & B00001111)); // convert BCD to decimal
    hours   = (((hours & B00110000)>>4)*10 + (hours & B00001111)); 
    day     = (day & B00000111); // 1-7
    date    = (((date & B00110000)>>4)*10 + (date & B00001111)); // 1-31
    month   = (((month & B00010000)>>4)*10 + (month & B00001111)); //msb7 is century overflow
    year    = (((year & B11110000)>>4)*10 + (year & B00001111));
  }
  else {
    //oh noes, no data!
  }
  printD3231();
}

void printD3231(){
  Serial.print(", 20");
  Serial.print(year, DEC);
  Serial.print("/");
  Serial.print(month, DEC);
  Serial.print("/");
  Serial.print(date, DEC);
  Serial.print(" - ");
  Serial.print(hours, DEC); 
  Serial.print(":"); 
  Serial.print(minutes, DEC); 
  Serial.print(":"); 
  Serial.println(seconds, DEC);
  delay(1000);
}

byte bcdConv(byte dec)
{
  return ((dec/10*16) + (dec%10));
}

void moveDelay() {
  delay(500);
}

void eepromRead(){
  // if 's'가 프리픽스로 들어오면 메모리에서 읽어서 값들을 블루투스로 전달한다.
  BTSerial.write(83);  // prifix "s"
  for(int i = romStartAddr; i < romStartAddr + motionArrLength; i++){
     BTSerial.write(EEPROM.read(i)); 
  }

  // 날자 Date 전송하기.
  BTSerial.write(year);
  BTSerial.write(month);
  BTSerial.write(date);
  BTSerial.write(hours);
  BTSerial.write(minutes);
  BTSerial.write('e');
}

void angleAct(int angle){
  servo.attach(9);
  servo.write(angle);
  moveDelay();
  servo.detach();
}
