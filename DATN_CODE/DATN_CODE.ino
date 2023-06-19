#include <Adafruit_Fingerprint.h>
#include <Keypad.h>
#include <stdlib.h>
#include <EEPROM.h>
#include <HardwareSerial.h>
#include "DFRobotDFPlayerMini.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>


#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <FirebaseESP32.h>
#define mySerial Serial2
#define RXp2 16
#define TXp2 17
#define RELAY_PIN 27
#define DOOR_SENSOR_PIN 13
#define pushButton_pin  21


#define ROW_NUM     4 //3four rows
#define COLUMN_NUM  4 // four columns

#define PRIMARY_ID_EEPROM_ADDRESS 1
#define CHILD_ID_EEPROM_ADDRESS 2
#define CHILD_ID_START 2
#define CHILD_ID_END 127

#define EEPROM_ADDR_SSID 310
#define EEPROM_ADDR_PASSWORD 330

#define BUTTON_PIN 22

const char* ssid = "ESP32-AP";
const char* passwordwifi = "";

//
//// Replace with your Firebase project's URL and secret

#define FIREBASE_HOST"https://fir-lockdoor-3bc15-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH"de5cgdiU79ai2bQkYRL3EbjJvXsyxs74YAOVW7pE"

const char* server = "onesignal.com";
const char* apiKey = "MGMxNjk4NjAtYjZiMC00NDMyLThlZTEtYzZiMDJiYjMyMDI2"; // replace with your OneSignal REST API Key
const char* appId = "9a0343db-f309-44cc-9e4f-796ca19ac255"; // replace with your OneSignal App ID

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
HardwareSerial mySerial1(1);
DFRobotDFPlayerMini myDFPlayer;


// Define keypad layout, battery and pins

const int analogInPin = 36; // GPIO pin number of analog input
const float cutoffVoltage = 3.2;
const float maxVoltage = 4.2;
float calibration = 0.4;

char keys[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte pin_rows[ROW_NUM]      = {4, 0, 2, 15};
byte pin_column[COLUMN_NUM] = {12, 14, 33, 32};   // GIOP16, GIOP4, GIOP0, GIOP2 connect to the column pins
Keypad keypad = Keypad( makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM );

// định nghia variables và constants
char  password [20] = "567890";
char  verify_password [20] = "";
char  input_password [20] = "";
char  temp_password[20] = "";
bool isChangingPassword = false; // added variable to check if changing password
bool addTempPassword = false;
bool tPass = false;

int door_status = 0 ;
int relay_status ;

int  opening_time;
int  set_volume;

uint8_t primaryID = 1;
uint8_t lastChildID;
bool check_mk_cb = 0;

int redPin = 5;
int greenPin = 18;
int bluePin = 19;
bool checkAddNewID;
int ActionCode = 0 ;
int count_openingDoor = 0;
int count_checkFingerPrint = 0;
bool low_battery_notification = true;

unsigned long previousMillis = 0;   // biến lưu thời điểm trước đó
const long interval = 200;

// Function prototypes
uint8_t readLastChildIDFromEEPROM();
uint8_t findAvailableID();
bool checkIDAvailable(uint8_t id);
void saveIDToEEPROM(uint8_t id);
void deleteIDFromEEPROM(uint8_t id);
uint8_t readIDFromEEPROM(uint8_t addr);
void writeIDToEEPROM(uint8_t addr, uint8_t id);

uint8_t getFingerprintEnroll(uint8_t id);
uint8_t getFingerprintID();
uint8_t readnumber(void);
void registerPrimaryFingerprint();
uint8_t loginWithPrimaryFingerprint();
void addChildFingerprint();
void deleteChildFingerprint();

void displayRGBLEDs(uint8_t status_led);
void speaker(uint8_t status_speaker);
void writeToFirebase(int id, int actionCode);
void PassCodeFunc();
void setupWifi();
void disConectFunc();
void SearchingWifi();
void disConect_PassCodeFunc();
void sendMessage();
void ButtonFunc();

void IRAM_ATTR door() {
  while (pushButton_pin == 0 ) {
  }
  count_checkFingerPrint = 0;
  door_status = 1;
  digitalWrite(RELAY_PIN, HIGH);
}


// Define FirebaseESP32 data object
FirebaseData firebaseData;
WiFiClient wifiClient;


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");


void setup() {
  // Set up pins
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(DOOR_SENSOR_PIN, INPUT_PULLUP);

  pinMode(pushButton_pin, INPUT_PULLUP);
  attachInterrupt(pushButton_pin, door, RISING);


  mySerial1.begin(9600, SERIAL_8N1, 25, 26);//RX 25, TX 26
  if (!myDFPlayer.begin(mySerial1))
  {
    Serial.println(F("Không thể khởi động:"));
    Serial.println(F("1.Kiểm tra lại kết nối"));
    Serial.println(F("2.Lắp lại thẻ nhớ"));
    while (true)
    {
      delay(0);
    }
  }
  Serial.println(F("DFPlayer Mini đang hoạt động"));

  myDFPlayer.volume(20);  //cài đặt volume từ 0 đến 30

  Serial.begin(9600);

  // Initialize EEPROM
  EEPROM.begin(512);

  // Wait for serial connection
  while (!Serial);

  // Initialize fingerprint sensor
  Serial.println("\n\nAdafruit Fingerprint sensor enrollment");
  finger.begin(57600);

  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) {
      delay(1);
    }
  }

  // tìm kiếm  last child ID và  the  child ID kế tiếp khả thi
  lastChildID = readLastChildIDFromEEPROM();
  uint8_t nextChildID = findAvailableID();

  // Check if there is an available child ID
  if (nextChildID == 0) {
    Serial.println("No available child ID.");
    return;
  }

  // Print out the last child ID and the next available child ID
  Serial.print("Last child ID: ");
  Serial.println(lastChildID);
  Serial.print("Next available child ID: ");
  Serial.println(nextChildID);


  bool check_pri = EEPROM.read(1);
  if (check_pri == 1) {
    check_mk_cb = 1;
  }

  char readPassword[7] = "567890";
  for (int i = 0; i < 6; i++) {
    readPassword[i] = EEPROM.read(200 + i);
    Serial.println(readPassword[i]);
  }
  readPassword[6] = '\0';
  Serial.print("\n");
  Serial.print(readPassword);
  Serial.print("\n");
  strcpy(password, readPassword);
  Serial.println(password);

  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Địa chỉ IP của AP: ");
  Serial.println(IP);

  // Khởi tạo web server
  AsyncWebServer server(80);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    String html = "<html><head><title>WiFi Configuration</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<style>";
    html += "body {font-family: Arial, Helvetica, sans-serif;}";
    html += "label, input {display: block; margin: 10px; width: 100%;}";
    html += "input[type=submit] {background-color: #4CAF50; border: none; color: white; padding: 10px 20px; text-align: center; text-decoration: none; display: block; font-size: 16px; margin: 10px auto; cursor: pointer; border-radius: 5px;}";
    html += "</style></head><body>";
    html += "<h1>WiFi Configuration</h1>";
    html += "<form method='post' action='/save'>";
    html += "<label>SSID:</label><input type='text' name='ssid'>";
    html += "<label>Password:</label><input type='password' name='password'>";
    html += "<input type='submit' value='Save'>";
    html += "</form></body></html>";
    request->send(200, "text/html", html);
  });




  // Đăng ký xử lý yêu cầu POST "/save"
  server.on("/save", HTTP_POST, [](AsyncWebServerRequest * request) {
    String ssid = "";
    String passwordwifi = "";

    // Lấy giá trị của tham số POST có tên là "ssid"
    if (request->hasArg("ssid")) {
      ssid = request->arg("ssid");
    };

    // Lấy giá trị của tham số POST có tên là "password"
    if (request->hasArg("password")) {
      passwordwifi = request->arg("password");
    }

    // Lưu tên và mật khẩu mạng Wi-Fi vào EEPROM hoặc bộ nhớ flash
    for (int i = 0; i < ssid.length(); i++) {
      EEPROM.write(EEPROM_ADDR_SSID + i, ssid[i]);
    }
    EEPROM.write(EEPROM_ADDR_SSID + ssid.length(), '\0'); // Kết thúc chuỗi

    for (int i = 0; i < passwordwifi.length(); i++) {
      EEPROM.write(EEPROM_ADDR_PASSWORD + i, passwordwifi[i]);
    }
    EEPROM.write(EEPROM_ADDR_PASSWORD + passwordwifi.length(), '\0'); // Kết thúc chuỗi

    EEPROM.commit(); // Lưu dữ liệu vào EEPROM

    String html = "<html><head><title>Save Success</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<style>";
    html += "body { font-family: Arial, Helvetica, sans-serif; background-color: #f2f2f2;}";
    html += "h1 { text-align: center; margin-top: 50px; }";
    html += "a { display: block; text-align: center; margin-top: 30px; font-size: 16px; color: #4CAF50; }";
    html += "</style></head><body>";
    html += "<div>";
    html += "<h1>Save Successfully!</h1>";
    html += "<a href='/'>Back to home</a>";
    html += "</div></body></html>";
    request->send(200, "text/html", html);

  });

  // Bắt đầu cung cấp dịch vụ web
  server.begin();
  connectToWifi();
}




// Đọc ID con cuối cùng từ EEPROM
uint8_t readLastChildIDFromEEPROM() {
  uint8_t lastID = 0;
  // Lặp lại tất cả các ID con có thể có và tìm ID lớn nhất tồn tại trong EEPROM
  for (uint8_t id = CHILD_ID_START; id <= CHILD_ID_END; id++) {
    uint8_t value = readIDFromEEPROM(id);
    if (value != 0 && id > lastID) {
      lastID = id;
    }
  }
  // Đọc ID con cuối cùng đã được lưu vào EEPROM
  lastChildID = EEPROM.read(CHILD_ID_END);
  // Nếu ID con cuối cùng đã lưu không giống với ID con cao nhất tồn tại trong EEPROM, cập nhật lastChildID trong EEPROM
  if (lastChildID != lastID) {
    lastChildID = lastID;
    EEPROM.write(CHILD_ID_END, lastChildID);
    EEPROM.commit();
  }
  return lastChildID;
}

// Tìm ID con tiếp theo có sẵn
uint8_t findAvailableID() {
  for (uint8_t id = CHILD_ID_START; id <= CHILD_ID_END; id++) {
    if (checkIDAvailable(id)) {
      return id;
    }
  }
  return 0; // không tìm thấy ID nào có sẵn
}

// Kiểm tra xem một ID con cụ thể có sẵn trong EEPROM không
bool checkIDAvailable(uint8_t id) {
  uint8_t value = readIDFromEEPROM(id);
  return (value == 0);
}

// Lưu ID con vào EEPROM
void saveIDToEEPROM(uint8_t id) {
  writeIDToEEPROM(id, 1);
}

// Xóa ID con khỏi EEPROM
void deleteIDFromEEPROM(uint8_t id) {
  writeIDToEEPROM(id, 0);
}

// Đọc ID con từ EEPROM
uint8_t readIDFromEEPROM(uint8_t id) {
  return EEPROM.read(id);
}

// Ghi ID con vào EEPROM
void writeIDToEEPROM(uint8_t id, uint8_t value) {
  EEPROM.write(id , value);
  EEPROM.commit();
}


// This function registers a fingerprint with the given ID
uint8_t getFingerprintEnroll(uint8_t id) {
  checkAddNewID = 0;
  int p = -1;
  Serial.println(id);
  // Wait until finger is detected
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_OK) {
    }
  }
  // Convert image to template
  p = finger.image2Tz(1);
  if (p == FINGERPRINT_OK) {
  } else {
    return p;
  }
  // Wait for finger removal
  speaker(32);
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  // Ask user to place same finger again
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_OK) {
    }
  }
  // Convert image to template
  p = finger.image2Tz(2);
  if (p == FINGERPRINT_OK) {
  } else {
    return p;
  }

  // Create a model from templates
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
  } else {
    displayRGBLEDs(4); // Display red LED to indicate failure
    speaker(24);
    return p;
  }
  // Store the model in the sensor with the given ID
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    check_mk_cb = 1;
    displayRGBLEDs(3); // Display green LED to indicate success
    speaker(25);
    checkAddNewID = 1;
  } else {
    return p;
  }
  return 1;
}


uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      break;
    case FINGERPRINT_NOFINGER:
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      return p;
    case FINGERPRINT_IMAGEFAIL:
      return p;
    default:
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      break;
    case FINGERPRINT_IMAGEMESS:
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      return p;
    case FINGERPRINT_FEATUREFAIL:
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      return p;
    default:
      return p;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    door_status = 1;
    displayRGBLEDs(5);
    count_checkFingerPrint = 0;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    displayRGBLEDs(6);
    speaker(3);
    count_checkFingerPrint++;
    return p;
  } else {
    return p;
  }

  // found a match!
  speaker(2);
  if (WiFi.status() == WL_CONNECTED) {
    int id = finger.fingerID;
    ActionCode = 1;
    writeToFirebase(id, ActionCode);
  }
  return finger.fingerID;
}


// Hàm này đọc số được nhập từ keypad, được sử dụng để nhập ID cho vân tay
uint8_t readnumber(void) {
  char key = keypad.getKey();
  char num[4] = {'\0'}; // mảng chứa các ký tự được nhập
  int ID_Num = 0; // biến chứa số được nhập
  int index = 0; // chỉ mục của mảng ký tự

  while (1) {
    if (key != NO_KEY) { // nếu có phím được nhấn
      speaker(1);
      if (key == '#') { // nếu phím # được nhấn, chuyển đổi chuỗi ký tự thành số nguyên
        ID_Num = atoi(num);
        return ID_Num;
      }
      else if (index < 2) { // nếu vẫn còn chỗ trống trong mảng ký tự
        num[index++] = key; // thêm ký tự mới vào mảng
      }
    }
    key = keypad.getKey(); // đọc phím tiếp theo từ keypad
  }
}

// Hàm này thực hiện đăng ký vân tay chính
void registerPrimaryFingerprint() {
  while (!getFingerprintEnroll(primaryID)); // gọi hàm getFingerprintEnroll để thực hiện đăng ký vân tay
  EEPROM.write(PRIMARY_ID_EEPROM_ADDRESS, 1); // lưu ID của vân tay chính vào EEPROM
  EEPROM.commit(); // lưu trữ trạng thái của EEPROM
}

// Hàm đăng nhập bằng vân tay ID #1
uint8_t loginWithPrimaryFingerprint() {
  speaker(22);
  uint32_t startTime = millis();
  while (millis() - startTime < 10000) { // cho phép 40 giây để nhập vân tay
    displayRGBLEDs(1);
    uint8_t p = finger.getImage(); // Lấy ảnh vân tay từ cảm biến
    if (p == FINGERPRINT_OK) {
      p = finger.image2Tz();
      if (p != FINGERPRINT_OK) {
        return 0; // Lỗi chuyển đổi ảnh vân tay sang mã
      }

      // Tìm kiếm và so sánh vân tay đang quét với vân tay đã đăng ký với ID #1
      uint16_t fingerID = 0;
      p = finger.fingerFastSearch();
      if (p == FINGERPRINT_OK) {
        fingerID = finger.fingerID;
        if (fingerID == 1) {
          return 1; // Đăng nhập thành công
        } else {
          displayRGBLEDs(6);
          speaker(3);
          return 0; // Không tìm thấy vân tay đã đăng ký
        }
      } else {
        return 0; // Lỗi tìm kiếm vân tay
      }
    }
  }
  return 0; // Hết thời gian cho phép để nhập vân tay
}

// Hàm đăng ký vân tay con
void addChildFingerprint() {
  uint8_t id = findAvailableID(); // Tìm ID trống để đăng ký
  if (id == 0) {
    return;
  }
  while (!getFingerprintEnroll(id)); // Nhập vân tay
  if (checkAddNewID) {
    saveIDToEEPROM(id); // Lưu ID vào bộ nhớ EEPROM
    lastChildID = EEPROM.read(CHILD_ID_END);
    ActionCode = 3;
    writeToFirebase(id, ActionCode);

    String ID = "id_" + String(id);
    if (Firebase.set(firebaseData, "/Danh_Sach_Id/" + ID , "Name"));
  }
}

// Hàm xóa vân tay con
void deleteChildFingerprint() {
  uint8_t deleteID = readnumber(); // đọc ID cần xóa từ đầu vào người dùng
  bool check_deleteID;
  check_deleteID = checkIDAvailable(deleteID);
  if (deleteID == 0 || deleteID == primaryID || deleteID > 127 || check_deleteID) { // nếu ID là 0 hoặc ID là ID chính, báo lỗi và thoát
    speaker(27);
    return;
  }
  if (finger.deleteModel(deleteID) == FINGERPRINT_OK) {
    uint8_t addr = deleteID;
    EEPROM.write(addr, 0); // xóa dữ liệu dấu vân tay
    EEPROM.commit();
    speaker(28);
    ActionCode = 4;
    writeToFirebase(deleteID, ActionCode);
    String ID = "id_" + String(deleteID);
    Firebase.deleteNode(firebaseData, "/Danh_Sach_Id/" + ID);
  }
}

void speaker(uint8_t status_speak) {
  switch (status_speak) {
    case 1:
      myDFPlayer.playMp3Folder(1); //  PIP
      delay (300);
      break;
    case 2:
      myDFPlayer.playMp3Folder(2); // TINH TINH
      delay (300);
      break;
    case 3:
      myDFPlayer.playMp3Folder(3); // TEEEE
      delay (1000);
      break;
    case 4:
      myDFPlayer.playMp3Folder(4); // KET NOI WIFI THANH CONG
      delay (2000);
      break;
    case 5:
      myDFPlayer.playMp3Folder(5); // KET NOI WIFI THAT BAI
      delay (2000);
      break;
    case 6:
      myDFPlayer.playMp3Folder(6); // NHAP MAT KHAU VA NHAN #
      delay (3000);
      break;
    case 7:
      myDFPlayer.playMp3Folder(7); // MK SAI VUI LONG NHAP LAI
      delay (2000);
      break;
    case 8:
      myDFPlayer.playMp3Folder(8); // NHAP MAT KHAU CU
      delay (1000);
      break;
    case 9:
      myDFPlayer.playMp3Folder(9); // NHAP MAT KHAU MOI
      delay (1000);
      break;
    case 10:
      myDFPlayer.playMp3Folder(10); // NHAP KHAU CU SAI
      delay (3000);
      break;
    case 11:
      myDFPlayer.playMp3Folder(11); // DOI MAT KHAU THANH CONG
      delay (2000);
      break;
    case 12:
      myDFPlayer.playMp3Folder(12); // DOI MAT KHAU THAT BAI
      delay (3000);
      break;
    case 13:
      myDFPlayer.playMp3Folder(13); // NHAP MAT KHAU DE THEM AMT KHAU TAM
      delay (3000);
      break;
    case 14:
      myDFPlayer.playMp3Folder(14); //MAT KHAU TAM SAI NHAN B DE THAO TAC LAI
      delay (3000);
      break;
    case 15:
      myDFPlayer.playMp3Folder(15); // TAOJ MAT KHAU TAM THAN CONG
      delay (2000);
      break;
    case 16:
      myDFPlayer.playMp3Folder(16); // NHAP MAT KHAU TAM
      delay (1000);
      break;
    case 17:
      myDFPlayer.playMp3Folder(17); // XOA MAT KHAU TAM
      delay (1000);
      break;
    case 18:
      myDFPlayer.playMp3Folder(18); // THOAT KHOI CHE DO KEKPAD
      delay (2000);
      break;
    case 19:
      myDFPlayer.playMp3Folder(19); // NHAP VAN TAY
      delay (2000);
      break;
    case 20:
      myDFPlayer.playMp3Folder(20); // LUU VAN TAY MAIN THAT BAI
      delay (1000);
      break;
    case 21:
      myDFPlayer.playMp3Folder(21); // LUU VAN TAY MAIN THANH CONG
      delay (1000);
      break;
    case 22:
      myDFPlayer.playMp3Folder(22); // VUI LONG NHAP VAN TAY MAIN
      delay (1000);
      break;
    case 23:
      myDFPlayer.playMp3Folder(23); // VUI LONG NHAP VAN TAY CON MUON THEM
      delay (2000);
      break;
    case 24:
      myDFPlayer.playMp3Folder(24); // LUU VAN TAY THAT BAI
      delay (3000);
      break;
    case 25:
      myDFPlayer.playMp3Folder(25); // LUU VAN TAY THANH CONG
      delay (1000);
      break;
    case 26:
      myDFPlayer.playMp3Folder(26); // NHAP ID VAN TAY MUON XOA
      delay (3000);
      break;
    case 27:
      myDFPlayer.playMp3Folder(27); // ID KHONG HOP LE
      delay (1000);
      break;
    case 28:
      myDFPlayer.playMp3Folder(28); // XOA THANH CONG VAN TAY CON CO ID
      delay (1000);
      break;
    case 29:
      myDFPlayer.playMp3Folder(29); // XOA TOAN BO VAN TAY TRONG HE THONG
      delay (3000);
      break;
    case 30:
      myDFPlayer.playMp3Folder(30); // XOA WIFI CU
      delay (1000);
      break;
    case 31:
      myDFPlayer.playMp3Folder(31); // XOA WIFI CU
      delay (2000);
      break;
    case 32:
      myDFPlayer.playMp3Folder(32); // XOA WIFI CU
      delay (1000);
      break;
    case 33:
      myDFPlayer.playMp3Folder(33); // XOA WIFI CU
      delay (2000);
      break;
    case 34:
      myDFPlayer.playMp3Folder(34); // XOA WIFI CU
      delay (1000);
      break;
    case 35:
      myDFPlayer.playMp3Folder(35); // XOA WIFI CU
      delay (1900);
      break;
    case 36:
      myDFPlayer.playMp3Folder(36); // XOA WIFI CU
      delay (3900);
      break;
    case 37:
      myDFPlayer.playMp3Folder(37); // XOA WIFI CU
      delay (1900);
      break;
  }
}


void displayRGBLEDs(uint8_t status_led) {
  switch (status_led) {
    case 0:
      digitalWrite(redPin, HIGH);
      digitalWrite(greenPin, HIGH);
      digitalWrite(bluePin, LOW);
      break;
    case 1: // Đang chờ đặt ngón tay, blink LED xanh dương
      digitalWrite(redPin, HIGH);
      digitalWrite(greenPin, HIGH);
      digitalWrite(bluePin, HIGH);
      for (int i = 0; i < 10; i++) {
        digitalWrite(bluePin, HIGH);
        delay(100);
        digitalWrite(bluePin, LOW);
        delay(100);
      }
      break;
    case 3: // Lưu vân tay thành công, nhấp nháy LED xanh lá cây
      digitalWrite(redPin, HIGH);
      digitalWrite(greenPin, HIGH);
      digitalWrite(bluePin, HIGH);
      for (int i = 0; i < 10; i++) {
        digitalWrite(greenPin, HIGH);
        delay(100);
        digitalWrite(greenPin, LOW);
        delay(100);
      }
      break;
    case 4: // Lỗi lưu vân tay, nhấp nháy LED đỏ
      digitalWrite(redPin, HIGH);
      digitalWrite(greenPin, HIGH);
      digitalWrite(bluePin, HIGH);
      for (int i = 0; i < 10; i++) {
        digitalWrite(redPin, HIGH);
        delay(100);
        digitalWrite(redPin, LOW);
        delay(100);
      }
      break;
    case 5:
      digitalWrite(redPin, HIGH);
      digitalWrite(greenPin, HIGH);
      digitalWrite(bluePin, HIGH);
      for (int i = 0; i < 2; i++) {
        digitalWrite(greenPin, HIGH);
        delay(100);
        digitalWrite(greenPin, LOW);
        delay(100);
      }
      break;
    case 6: //Do
      digitalWrite(redPin, LOW);
      digitalWrite(greenPin, HIGH);
      digitalWrite(bluePin, HIGH);
      break;
    case 7: // Vàng
      digitalWrite(redPin, LOW);
      digitalWrite(greenPin, LOW);
      digitalWrite(bluePin, HIGH);
      break;
    case 8: // tim
      digitalWrite(redPin, LOW);
      digitalWrite(greenPin, HIGH);
      digitalWrite(bluePin, LOW);
      break;
    default:
      break;
  }
}


// Hàm nhập mật khẩu
void PassCodeFunc() {
  strcpy(input_password, "");
  int count = 0;
  // Vòng lặp vô hạn để đợi người dùng nhập mật khẩu
  while (1) {
    displayRGBLEDs(8);
    // Đọc phím nhấn trên keypad
    char key = keypad.getKey();
    if (count == 5) {
      speaker(33);
      ActionCode = 9;
      writeToFirebase(-1, ActionCode);
      delay(10000);
      count = 0;
    }
    if (key) {
      speaker(1);
      // Kiểm tra xem chế độ thay đổi mật khẩu có đang được bật hay không
      if (isChangingPassword) {
        // Nếu phím nhấn là "#" thì người dùng đã hoàn tất việc thay đổi mật khẩu
        if (key == '#') {
          if (strlen(input_password) != 6) {
            speaker(12);
            isChangingPassword = false; // tắt chế độ thay đổi mật khẩu
          }
          else {
            strcpy(verify_password, password);
            strcpy(password, input_password);
            speaker(35);  //vui long xac nhan lai mat khau
            if (check_OldPass()) {
              speaker(11);
              isChangingPassword = false;
              strcpy(input_password, ""); // Xóa mật khẩu đang nhập
              ActionCode = 5;
              writeToFirebase(-1, ActionCode);
              for (int i = 0; i < 7; i++) {
                EEPROM.write(200 + i, password[i]);
              }
              EEPROM.commit();
              strcpy(input_password, ""); // xóa mật khẩu mới đã nhập để chuẩn bị cho lần nhập kế tiếp
            }
            else {
              speaker(36);  // Xac nhan mat khau khong dung, nhan phim A de thao tac lai
              strcpy(password, verify_password);
              isChangingPassword = false; // tắt chế độ thay đổi mật khẩu
            }
          }
        }
        // Nếu phím nhấn là "*" thì xóa toàn bộ mật khẩu mới đã nhập
        else if (key == '*') {
          strcpy(input_password, "");
          speaker(34);
        }
        // Nếu phím nhấn là một ký tự khác thì thêm ký tự đó vào mật khẩu mới đã nhập
        else {
          strncat(input_password, &key, 1); // thêm ký tự mới vào cuối mật khẩu mới
        }
      }


      else if (addTempPassword) {
        if (key == '#') { // nếu người dùng ấn nút "#"
          if (strlen(input_password) != 6) { // nếu độ dài mật khẩu ít hơn 6 ký tự
            speaker(14);
            addTempPassword = false; // tắt chế độ thêm mật khẩu tạm thời
          }
          else {
            strcpy(temp_password, input_password); // lưu mật khẩu tạm thời mới vào biến temp_password
            ActionCode = 6;
            writeToFirebase(-1, ActionCode);
            speaker(15);
            tPass = true; // bật cờ cho phép sử dụng mật khẩu tạm thời

            addTempPassword = false; // tắt chế độ thêm mật khẩu tạm thời
            strcpy(input_password , ""); // xóa mật khẩu đã nhập trong input_password
          }
        }
        else if (key == '*') { // nếu người dùng ấn nút "*"
          strcpy(input_password, ""); // xóa mật khẩu đã nhập trong input_password
          speaker(34);
        }
        else { // nếu người dùng ấn một phím số
          strncat (input_password , &key, 1); // thêm ký tự vừa ấn vào mật khẩu trong input_password
        }
      }

      else {
        if (key == '*') {
          strcpy(input_password, ""); // Xóa mật khẩu đang nhập
          speaker(34);
        }
        else if (key == 'A') {
          speaker(8);
          if (check_OldPass()) {
            speaker(9);
            isChangingPassword = true;
            strcpy(input_password, ""); // Xóa mật khẩu đang nhập
          }
          else {
            displayRGBLEDs(6);
            speaker(3);
            speaker(10);
          }
        }
        else if (key == 'B') {
          speaker(8);
          if (check_OldPass()) {
            addTempPassword = true;
            strcpy(input_password, ""); // Xóa mật khẩu đang nhập
            speaker(16);
          }
          else {
            displayRGBLEDs(6);
            speaker(3);
            speaker(13);
          }
        }
        else if (key == 'C') {
          tPass = false;
          speaker(17);
          ActionCode = 7;
          writeToFirebase(-1, ActionCode);
        }
        else if (key == '#') {
          if ((strstr(input_password, password) != NULL) || ((strstr(input_password, temp_password) != NULL) && tPass)) {
            digitalWrite(RELAY_PIN, HIGH); // lock the door
            displayRGBLEDs(5);
            count = 0;
            speaker(2);
            door_status = 1;
            if ((strstr(input_password, temp_password) != NULL) && tPass ) {
              ActionCode = 11;
              writeToFirebase(-1, ActionCode);
            }
            else {
              ActionCode = 2;
              writeToFirebase(-1, ActionCode);
            }
            previousMillis = 0;
            return;
          }
          else {
            displayRGBLEDs(6);
            speaker(3);
            speaker(7);
            count++;
          }
          strcpy(input_password, ""); // Xóa mật khẩu đang nhập
        }
        else if (key == 'D') {
          speaker(18);
          previousMillis = 0;
          return;
        }
        else {
          strncat(input_password, &key, 1); // Thêm ký tự mới vào chuỗi mật khẩu đang nhập
        }
      }
    }
  }
}


bool check_OldPass() {
  strcpy(input_password, "");
  while (1) {
    // Đọc phím nhấn trên keypad
    char key = keypad.getKey();
    if (key) {
      // In phím đã được nhập trên Serial Monitor

      if (key == '*') {
        strcpy(input_password, ""); // Xóa mật khẩu đang nhập
        speaker(34);
      }
      else if (key == '#') {
        if (strcmp(input_password, password) == 0) {
          strcpy(input_password, ""); // Xóa mật khẩu đang nhập
          return true;
        }
        else {
          strcpy(input_password, ""); // Xóa mật khẩu đang nhập
          return false;
        }
      }
      else {
        speaker(1);
        strncat(input_password, &key, 1); // Thêm ký tự mới vào chuỗi mật khẩu đang nhập
      }
    }
  }
}

void deleteAllFingerprints() {
  for (uint8_t id = CHILD_ID_START; id <= CHILD_ID_END; id++) {
    deleteIDFromEEPROM(id);
  }
  lastChildID = 0;
  check_mk_cb = 0;
  EEPROM.write(CHILD_ID_END, lastChildID);
  EEPROM.commit();
  finger.emptyDatabase();
  ActionCode = 10;
  writeToFirebase(-1, ActionCode);
  //  String ID = "id_" + String(deleteID);
  Firebase.deleteNode(firebaseData, "/Danh_Sach_Id/");
}

void connectToWifi() {
  // Read SSID from EEPROM
  char ssid[32];
  char passwordwifi[64];

  for (int i = 0; i < 32; i++) {
    ssid[i] = EEPROM.read(EEPROM_ADDR_SSID + i);
    if (ssid[i] == '\0') {
      break;
    }
  }

  for (int i = 0; i < 64; i++) {
    passwordwifi[i] = EEPROM.read(EEPROM_ADDR_PASSWORD + i);
    if (passwordwifi[i] == '\0') {
      break;
    }
  }

  // Kết nối đến mạng Wi-Fi
  const char* ssid_char = ssid;
  const char* password_char = passwordwifi;

  WiFi.begin(ssid_char, password_char);
  Serial.print("Connecting to Wi-Fi");

  // Thêm time-out để kiểm tra kết nối đến Wi-Fi
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 120000) { // time-out sau 30 giây
    delay(1000);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    speaker(4);
    Serial.println();
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());


    // Connect to Firebase
    Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
    Firebase.reconnectWiFi(true);
    timeClient.begin();
    timeClient.setTimeOffset(25200);
    Serial.println("KET NOI FIREBASE THANH CONG");

  } else {
    Serial.println();
    speaker(5);
    Serial.println("Failed to connect to Wi-Fi network");
    // Xử lý trường hợp kết nối Wi-Fi không thành công ở đây
  }
}

String getAction(int ActionCode) {
  String action = ""; // Khởi tạo giá trị mặc định là chuỗi rỗng
  switch (ActionCode) {
    case 1:
      action = "mở_cửa_bằng_vân_tay";
      break;
    case 2:
      action = "mở_cửa_bằng_mật_khẩu";
      break;
    case 3:
      action = "thêm_vân_tay";
      break;
    case 4:
      action = "xóa_vân_tay";
      break;
    case 5:
      action = "thay_đổi_mật_khẩu";
      break;
    case 6:
      action = "thêm_mật_khẩu_tạm";
      break;
    case 7:
      action = "xóa_mật_khẩu_tạm";
      break;
    case 8:
      action = "đóng_cửa";
      break;
    case 9:
      action = "hệ_thống_tạm_khóa";
      break;
    case 10:
      action = "xóa_hết_vân_tay";
      break;
    case 11:
      action = "mở_cửa_bằng_mật_khẩu_tạm";
      break;
    default:
      break;
  }
  return action;
}



void writeToFirebase(int id, int actionCode) {
  String ID = "id_" + String(id);
  String timeStampAndDayStamp = getTimeStampAndDayStamp();
  String action = getAction(actionCode);

  if (Firebase.set(firebaseData, "/History/" + timeStampAndDayStamp , "Hoạt_động_" + action + (id != -1 ? "_" + ID : ""))) {
    Serial.print("Data was successfully written to Firebase");
  } else {
    Serial.print("There was an error writing data to Firebase");
  }
}


String getTimeStampAndDayStamp() {
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }

  // The formattedDate comes with the following format:
  // 2018-05-28T16:00:13Z
  // We need to extract date and time
  String formattedDate = timeClient.getFormattedDate();

  // Extract date
  int splitT = formattedDate.indexOf("T");
  String dayStamp = formattedDate.substring(0, splitT);

  // Extract time
  String timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1);

  return dayStamp + " " + timeStamp ;
}


float getBatteryPercentage() {
  int sensorValue = analogRead(analogInPin);
  float voltage = ((sensorValue * 3.3) / 4095) * 2 + calibration;
  float bat_percentage = (voltage - cutoffVoltage) * 100.0 / (maxVoltage - cutoffVoltage);

  return constrain(bat_percentage, 0, 100);
}


void writeToFirebasepin() {
  float pin = getBatteryPercentage();
  if (Firebase.setFloat(firebaseData, "/TQLockDoor/BatteryPercent" , pin)) {
    Serial.println("Data was successfully written to Firebase");
  } else {
    Serial.println("There was an error writing data to Firebase");
  }
}

void writeToFirebase_door_status() {
  int sensor = digitalRead(DOOR_SENSOR_PIN);
  if (Firebase.setFloat(firebaseData, "/TQLockDoor/DoorState" , sensor)) {
    Serial.println("Data was successfully written to Firebase");
  } else {
    Serial.println("There was an error writing data to Firebase");
  }
}

void disConect_PassCodeFunc() {
  strcpy(input_password, "");
  while (1) {
    displayRGBLEDs(8);
    // Đọc phím nhấn trên keypad
    char key = keypad.getKey();
    if (key) {
      speaker(1);
      Serial.println(key);
      if (key == '*') {
        speaker(34);
        strcpy(input_password, ""); // Xóa mật khẩu đang nhập
      }
      else if (key == '#') {
        if ( strstr(input_password, password) != NULL ) {
          displayRGBLEDs(5);
          speaker(2);
          digitalWrite(RELAY_PIN, HIGH); // lock the door
          door_status = 1 ;
          return ;
        }
        else {
          displayRGBLEDs(6);
          speaker(3);
          speaker(7);
          displayRGBLEDs(8);
        }
        strcpy(input_password, ""); // Xóa mật khẩu đang nhập
      }
      else if (key == 'D') {
        speaker(18);
        return;
      }
      else {
        strncat(input_password, &key, 1); // Thêm ký tự mới vào chuỗi mật khẩu đang nhập
      }
    }
  }
}

void SearchingWifi() {
  unsigned long disC_previousMillis = 0;   // biến lưu thời điểm trước đó
  const long disC_interval = 60000;
  char ssid[32];

  unsigned long currentMillis = millis();   // lấy thời gian hiện tại

  if ((currentMillis - disC_previousMillis >= disC_interval) &&  (digitalRead(DOOR_SENSOR_PIN) == LOW)) {
    for (int i = 0; i < 32; i++) {
      ssid[i] = EEPROM.read(EEPROM_ADDR_SSID + i);
      if (ssid[i] == '\0') {
        break;
      }
    }
    Serial.print("Scanning for WiFi network: ");
    Serial.println(ssid);
    int numWifi = WiFi.scanNetworks();
    for (int i = 0; i < numWifi; i++) {
      if (strcmp(WiFi.SSID(i).c_str(), ssid) == 0) {
        connectToWifi();
        break;
      }
    }
    disC_previousMillis = disC_previousMillis;   // lưu thời điểm cập nhật hiện tại để sử dụng cho lần cập nhật tiếp theo
  }
}

void disConectFunc() {
  myDFPlayer.volume(20);
  while (WiFi.status() != WL_CONNECTED) {
    SearchingWifi();
    char key = keypad.getKey();
    displayRGBLEDs(7);

    uint8_t p = finger.getImage();
    if (p == FINGERPRINT_OK) {
      getFingerprintID();
    }

    if (key != NO_KEY) {
      speaker(1);
      if (key == '5') {
        speaker(6);
        disConect_PassCodeFunc();
      }
    }
    if (door_status == 1) {
      digitalWrite(RELAY_PIN, HIGH);
      delay(3000);
      door_status = 0;
    }
    if (digitalRead(DOOR_SENSOR_PIN) == HIGH ) {
      digitalWrite(RELAY_PIN, HIGH); // unlock the door
    }

    if (digitalRead(DOOR_SENSOR_PIN) == LOW ) {
      digitalWrite(RELAY_PIN, LOW);
    }
  }
}


void sendMessage(int case_notification)
{
  static unsigned long last_notification_time = 0;
  const unsigned long notification_interval = 60000; // Thời gian giữa hai lần gửi thông báo là 1 phút
  unsigned long current_time = millis();
  // Nếu chưa đến thời điểm gửi thông báo tiếp theo thì không làm gì
  if (current_time - last_notification_time < notification_interval) {
    return;
  }
  // Nếu đến thời điểm gửi thông báo, ghi nhận thời gian hiện tại và gửi thông báo
  last_notification_time = current_time;
  String payload;
  int httpCode;
  HTTPClient http;
  http.begin("https://onesignal.com/api/v1/notifications");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Basic " + String(apiKey));

  switch (case_notification) {
    case 1:
      payload = "{\"app_id\":\"9a0343db-f309-44cc-9e4f-796ca19ac255\",\"include_player_ids\":[\"5dd88e1f-a50b-4158-9bd1-336ee5a4a365\"],\"contents\":{\"en\":\"CANH BAO: He thong tam khoa do co nguoi co tinh mo cua\"},\"name\":\"INTERNAL_CAMPAIGN_NAME\"}";
      httpCode = http.POST(payload);
      break;
    case 2:
      payload = "{\"app_id\":\"9a0343db-f309-44cc-9e4f-796ca19ac255\",\"include_player_ids\":[\"5dd88e1f-a50b-4158-9bd1-336ee5a4a365\"],\"contents\":{\"en\":\"CANH BAO: Cua mo qua lau\"},\"name\":\"INTERNAL_CAMPAIGN_NAME\"}";
      httpCode = http.POST(payload);
      break;
    case 3:
      payload = "{\"app_id\":\"9a0343db-f309-44cc-9e4f-796ca19ac255\",\"include_player_ids\":[\"5dd88e1f-a50b-4158-9bd1-336ee5a4a365\"],\"contents\":{\"en\":\"CANH BAO: Sap het pin vui long sac !!\"},\"name\":\"INTERNAL_CAMPAIGN_NAME\"}";
      httpCode = http.POST(payload);
      break;
  }

  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    Serial.println("Notification sent successfully");
    Serial.println(response);
  } else {
    Serial.println("Failed to send notification");
    Serial.println(httpCode);
    String response = http.getString();
    Serial.println(response);
  }
  http.end();
}


void ButtonFunc(char key) {
  // Nếu nhấn phím * thì đăng nhập hoặc đổi mật khẩu
  if (key == '*') {
    if (check_mk_cb == 0 )
    {
      speaker(8);
      if (check_OldPass()) {
        speaker(19);
        registerPrimaryFingerprint(); // Ghi nhận dấu vân tay chính
      }
      else {
        displayRGBLEDs(6);
        speaker(3);
      }
    }
    else {
      if (loginWithPrimaryFingerprint() == 1) {
        speaker(19);
        delay(1000);
        registerPrimaryFingerprint(); // Ghi nhận dấu vân tay chính
      }
    }
  }

  // Nếu nhấn phím # thì thêm dấu vân tay con
  if (key == '#') {
    if (loginWithPrimaryFingerprint() == 1) { // Đăng nhập bằng dấu vân tay chính
      delay(1000);
      speaker(23);
      addChildFingerprint(); // Thêm dấu vân tay con
    }
  }
  // Nếu nhấn phím 0 thì xóa dấu vân tay con
  if (key == '0') {
    if (loginWithPrimaryFingerprint() == 1) { // Đăng nhập bằng dấu vân tay chính
      delay(1000);
      speaker(26);
      deleteChildFingerprint();
    }
  }
  if (key == 'B') {
    if (loginWithPrimaryFingerprint() == 1) { // Đăng nhập bằng dấu vân tay chính
      for (int i = EEPROM_ADDR_SSID; i < 330; i++) {
        EEPROM.write(i, 0);
      }
      EEPROM.commit();
      for (int i = EEPROM_ADDR_PASSWORD; i < 350; i++) {
        EEPROM.write(i, 0);
      }
      EEPROM.commit();
      speaker(30);
    }
  }
  // Nếu nhấn phím 5 thì đăng nhập bằng mật khẩu
  if (key == '5') {
    speaker(6);
    PassCodeFunc();
  }

  // Nếu nhấn phím 7 thì xóa toàn bộ dấu vân tay
  if (key == 'C') {
    if (loginWithPrimaryFingerprint() == 1) { // Đăng nhập bằng dấu vân tay chính
      delay(1000);
      speaker(29);
      deleteAllFingerprints();
    }
  }
}

void loop() {
  static unsigned long last_opendoor_time = 0;
  const unsigned long interval = 60000;
  unsigned long current_time = millis();
  unsigned long time_elapsed = current_time - last_opendoor_time;
  float bat_percentage = getBatteryPercentage();
  static float last_percentage = bat_percentage;
  char key = keypad.getKey();
  if (WiFi.status() != WL_CONNECTED) {
    disConectFunc();
  }
  displayRGBLEDs(0); // Đọc trạng thái cửa từ Firebase và hiển thị trên Serial Monitor

  unsigned long currentMillis = millis();   // lấy thời gian hiện tại
  if ((currentMillis - previousMillis >= 1000) &&  (digitalRead(DOOR_SENSOR_PIN) == LOW)   ) {   // nếu đã đến thời điểm cập nhật
    Serial.println("Firebase");
    Firebase.getString(firebaseData, "/TQLockDoor/Speaker_Volume");
    set_volume = firebaseData.stringData().toInt();
    myDFPlayer.volume(set_volume);

    Firebase.getString(firebaseData, "/TQLockDoor/opening_door_time");
    opening_time = firebaseData.stringData().toInt();

    Firebase.getString(firebaseData, "/TQLockDoor/Door");
    door_status = firebaseData.stringData().toInt();

    previousMillis = currentMillis;   // lưu thời điểm cập nhật hiện tại để sử dụng cho lần cập nhật tiếp theo
  }

  Serial.println("after Firebase");
  // Đọc giá trị từ keypad và xử lý tương ứng với từng phím
  
  if (key != NO_KEY) {
    speaker(1);
    ButtonFunc(key);
  }

  // Quét và xử lý dấu vân tay
  uint8_t p = finger.getImage();
  if (p == FINGERPRINT_OK) {
    getFingerprintID();
  }
  if (count_checkFingerPrint == 5) {
    speaker(33);
    ActionCode = 9;
    writeToFirebase(-1, ActionCode);
    sendMessage(1);
    for (int i = 0; i < 9; i++) {
      if (count_checkFingerPrint == 0) {
        break;
      }
      speaker(31);
      delay(100);
    }
    speaker(2);
    count_checkFingerPrint = 0;
  }

  if (digitalRead(DOOR_SENSOR_PIN) == HIGH) {
    writeToFirebase_door_status();
    if (time_elapsed >= opening_time * interval) {
      sendMessage(2);
      speaker(31);
    }
  }
  if (digitalRead(DOOR_SENSOR_PIN) == LOW) {
    writeToFirebase_door_status();
    last_opendoor_time = current_time;
    digitalWrite(RELAY_PIN, LOW);
  }

  if (door_status == 1) {
    digitalWrite(RELAY_PIN, HIGH);
    delay(3000);
    if (digitalRead(DOOR_SENSOR_PIN) == HIGH ) {
      Firebase.set(firebaseData, "/TQLockDoor/Door" , 0);
      digitalWrite(RELAY_PIN, HIGH); // unlock the door
      door_status = 0;
    }
    else {
      door_status = 0;
      Firebase.set(firebaseData, "/TQLockDoor/Door" , digitalRead(DOOR_SENSOR_PIN) );
      count_openingDoor = 0;
      digitalWrite(RELAY_PIN, LOW); // unlock the door
    }
  }
  if ( low_battery_notification == true && bat_percentage <= 20  &&  digitalRead(DOOR_SENSOR_PIN) == HIGH  ) {
    speaker(37);
    low_battery_notification = false;
  }
  else {
    if (digitalRead(DOOR_SENSOR_PIN) == HIGH ) {
      if (time_elapsed  > interval * 0.2) {
        digitalWrite(RELAY_PIN, LOW);
      }
      else {
        digitalWrite(RELAY_PIN, HIGH);
      }
    }
  }
  if (bat_percentage == 30 || bat_percentage == 20 || bat_percentage == 0 ) {
    sendMessage(3);
  }
  if (abs(last_percentage - bat_percentage) >= 10 && digitalRead(RELAY_PIN) == LOW   &&   digitalRead(DOOR_SENSOR_PIN) == LOW ) {
    last_percentage = bat_percentage;
    writeToFirebasepin();
  }
}
