#include <SPI.h>               // include SPI library
#include <Adafruit_GFX.h>      // include adafruit graphics library
#include <Adafruit_PCD8544.h>  // include adafruit PCD8544 (Nokia 5110) library
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>

#define DEBUG 1

#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12
#define D7 13
#define D8 15
#define TX 1

#define MyRoomLocation "Room1"
#define FLAME_pin D4
#define DOOR_pin D3
#define BUZZER_pin D2
#define Switch TX
#define LED_pin D1

#define DOOR_OPEN_MSG "DOOR OPEN!!"
#define DOOR_CLOSED_MSG "DOOR closed"
#define FLAME_UNDETECTED_MSG "FLAME undetected"
#define FLAME_DETECTED_MSG "FLAME detected!"
//LCD
Adafruit_PCD8544 display = Adafruit_PCD8544(D5, D7, D6, D8, D0);
static const unsigned char PROGMEM logo16_glcd_bmp[] =
{ B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000
};

void callback(char *topic, byte *payload, unsigned int length) //Web으로 부터 수신한 Message에 따른 동작 제어 함수
{
  char message_buff[100]; //initialise storage buffer
  String msgString;
  int i = 0;

  Serial.println("Message arrived: topic: " + String(topic));
  Serial.println("Length: " + String(length, DEC));

  //create character buffer with ending null terminator (string)
  for (i = 0; i < length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';

  msgString = String(message_buff);
  Serial.println("Payload: " + msgString);

}
const char *wifi_ssid = "HGU_IoT_3"; // 사용하는 공유기 이름
const char *wifi_password = "csee1414";       // 공유기 password 입력

// MQTT
#define mqtt_broker "192.168.0.92"
#define mqtt_user "iot"
#define mqtt_pwd "csee1414"

// 개별 nodeMCU 설정시 수정해야하는 부분(숫자를 자신의 주어진 번호로 모두 수정) - 자신의 NodeMCU NAME
//Publish를 위한 Topic
const char *mqtt_nodeName = "myhome";


WiFiClient wifiClient; //클라이언트로

PubSubClient mqtt_client(mqtt_broker, 1883, callback, wifiClient);



int STATE_DOOR = 0; // 1이면 문열림, 0이면 문닫힘
int STATE_LED = 0; // 1이면 LED켜짐, 0이면 LED꺼짐
int STATE_BUZZER_FLAME = 0; // 1이면 Buzzer켜짐, 0이면 Buzzer꺼짐. buzzerinterval(밀리초)와 연계
int STATE_BUZZER_DOOR = 0;
int STATE_FLAME = 0; // 0이면 불꽃감지됨, 1이면 불꽃감지 안됨
int STATE_SWITCH = 0; // 0이면 버튼 눌림, 1이면 버튼 떼짐. 버튼 눌림의 현재 상태를 나타냄.
int STATE_LEAVING = 0; // 1이면 외출모드, 0이면 외출모드가 아님
int Gasflag = 0;
int lapcount = 0; // 버튼 누름 상태 지속시간 저장용 변수
int switchtime = 0; // 버튼 처음 눌렀을 때 시간 저장, = millis() 사용
int switchflag = 0; // 0이면 버튼을 아직 안 누른 것, 1이면 버튼을 누른 것. 버튼 눌림의 과거 시간, 즉 u(-t) 상태의 저장.
int buzzertimeflame = 0; // 불꽃 감지에 의해 부저를 처음 울리기 시작한 시간 저장, = millis() 사용. 불꽃이 감지되지 않을때까지 갱신됨.
int buzzertimedoor = 0; // 문열림 감지에 의해 부저를 처음 울리기 시작한 시간 저장, millis() 사용. 문이 닫힐때까지 갱신됨.
int buzzerinterval = 0; // 외출모드 활성화에 따른 부저 울리는 시간(밀리초)를 다르게 저장하기 위한 변수
int buzzertonecount = 0; // 불꽃 감지에 의해 부저를 울릴 때, 음 두 개를 번갈아 출력하기 위해 사용하는 시간 간격 저장용 변수
int buzzernexttone = 0; // 음정을 바꾸기 위한 변수


String hum = "";
String temp = "";

WiFiServer server(80);
#define WEBHOOK_PROTOCOL "http://"
#define NODE2_IP "192.168.0.103"
#define WEBHOOK_PATH_FLAME    WEBHOOK_PROTOCOL NODE2_IP "/Flame/"
#define WEBHOOK_PATH_DOOROPEN WEBHOOK_PROTOCOL NODE2_IP "/DoorOpen/"


//FOR IFTTT

#define IFTTT_HOST "maker.ifttt.com"
#define IFTTT_KEY "bvdK0S9W6_lsDU0IS-ze_t"
String payload_door = "";
String payload_flame = "";

const char *pub_gas = "myhome/room2/gas";
const char *pub_door = "myhome/room1/door";
const char *pub_flame = "myhome/room1/flame";
const char *pub_mode = "myhome/room2/mode";
const char *pub_wifi1 = "myhome/room1/wifi";
const char *pub_wifi2 = "myhome/room2/wifi2";

const char *sub_topic2 = "myhome";

void InitWiFi()
{
  Serial.println(); //시리얼모니터창에 보기 좋게 줄을 바꿈
  Serial.println();
  Serial.println("Connectiong to WiFi..");
  // attempt to connect to WiFi network
  WiFi.begin(wifi_ssid, wifi_password);
  int ct = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    ct++;
    delay(500);
    Serial.print(".");
    if (ct == 10) return;
  }
  // 접속이 되면 출력
  Serial.println();
  Serial.print("Connected to AP: ");
  //접속 정보를 출력
  Serial.println(WiFi.localIP());
}


void setup() {
  display.begin();
  display.setContrast(50);
  display.clearDisplay();
  Serial.begin(9600);
  print_lcd("PreWiFi");
  InitWiFi();
  print_lcd("PostWiFi");
  pinMode (DOOR_pin, INPUT_PULLUP);
  pinMode (LED_pin, OUTPUT);
  pinMode (BUZZER_pin, OUTPUT);
  pinMode (FLAME_pin, INPUT);
  pinMode (Switch, INPUT_PULLUP);
  digitalWrite(LED_pin, LOW);
  server.begin(); // setup webserver
  mqtt_client.setServer(mqtt_broker, 1883);
  mqtt_client.setCallback(callback);
  mqtt_client.connect(mqtt_nodeName, mqtt_user, mqtt_pwd);
}

void print_lcd(String msg) {
  // put your main code here, to run repeatedly:
  display.setTextSize(0.8);
  display.setTextColor(BLACK);
  display.setCursor(0, 0);
  display.println(msg);
  display.display();
}

bool show_ip = false;
char data[80];
void loop() { //<<(1), (2)>>
  if (WiFi.status() == WL_CONNECTED)
  {
    mqtt_client.publish(pub_wifi2, "WIFI ON", 1);
    if (!show_ip)
    {
      show_ip = true;
      display.print("IP:"); // 디스플레이로
      display.println(WiFi.localIP());
    }
    listen_webhook();
  }
  else
  {
    Serial.println("WIFI OFF");
    InitWiFi();
    mqtt_client.publish(pub_wifi2, "WIFI OFF", 1);
  }

  int pSTATE_LEAVING = STATE_LEAVING; int pSTATE_FLAME = STATE_FLAME; int pSTATE_DOOR = STATE_DOOR; int pSTATE_SWITCH = STATE_SWITCH;
  // 과거 상태를 저장하기. 이는 시리얼 모니터 출력의 if문에서 사용하여, 바뀐 상태가 있는 경우에만 출력을 하도록 하기 위함임. (깔끔하게 하기위한 용도)
  STATE_FLAME = digitalRead(FLAME_pin); // 불꽃감지센서 데이터 읽어오기
  STATE_DOOR = digitalRead(DOOR_pin); // 문열림감지센서 데이터 읽어오기
  STATE_SWITCH = digitalRead(Switch); // 버튼 데이터 읽어오기
  //<<(3)>>
  // 외출모드를 바꾸는 버튼 눌림 구문을 시리얼 모니터 출력 구문 전에 실행해야, 시리얼 출력 구문에서 외출모드상태가 바로 반영이 되므로 여기서 실행함.
  if (!switchflag && !STATE_SWITCH) { // 버튼을 누르지 않아왔는데, 지금 버튼이 눌린 순간, 즉 버튼을 처음 누르는 순간
    switchflag = 1; // 이제 (다음 if 구문의 논리 판별을 위한)버튼의 과거 상태는 누름이 됨
    //Serial.println("*** Button pressed, saving current time. ***");
    switchtime = millis();
  }
  if (switchflag && STATE_SWITCH) { // 버튼을 눌러왔는데, 지금 버튼을 뗀 순간
    switchflag = 0; // 이제 (다음 if 구문의 논리 판별을 위한)버튼의 과거 상태는 뗌이 됨
    lapcount = 0;

    if (millis() - switchtime > 5000) { // 버튼을 누른 시간이 5초를 넘었다면
      display.println("Over 5 seconds, toggling outside mode. ");
      STATE_LEAVING = !STATE_LEAVING; // 외출 모드 토글
      if (STATE_LEAVING) {
        display.clearDisplay();
        display.println("Now outside. ***"); // 외출 모드가 켜지면 바깥이라고 시리얼 모니터에 출력
        display.display();
      }
      else {
        display.clearDisplay();
        display.println("Now inside. ***"); // 외출 모드가 꺼지면 안쪽이라고 시리얼 모니터에 출력
        display.display();
      }
    }
    else display.println("Under 5 seconds, nothing happened. ***"); // 버튼을 누른 시간이 5초가 되지 않았다면, 아무 일도 없었다고 출력
  }
  if (switchflag && !STATE_SWITCH) { // 버튼을 계속 누르고 있을 때
    if (lapcount++ && !(lapcount % 10)) tone(BUZZER_pin, 262, 100); // 1초마다 알림
  }
  // 외출모드 상태에 따라 buzzerinterval을 바꿔준다. 이 if문은 외출모드를 바꾸는 버튼 눌림 구문 바로 뒤에 붙여줘야 한다.
  //#if DEBUG
  //  if (STATE_LEAVING) buzzerinterval = 1000; else buzzerinterval = 1000; // buzzerinteral이 유일하게 바뀌는 곳. 외출모드이면 5초, 아니면 1초
  //#else
  //  if (STATE_LEAVING) buzzerinterval = 10000; else buzzerinterval = 5000; // buzzerinteral이 유일하게 바뀌는 곳. 외출모드이면 5초, 아니면 1초
  //#endif
  if (STATE_LEAVING)
    buzzerinterval = 10000;
  else
    buzzerinterval = 5000; // buzzerinteral이 유일하게 바뀌는 곳. 외출모드이면 5초, 아니면 1초

  //<<(5)>>
  if (!STATE_FLAME) { // 불꽃이 감지되었을 때. 디지털 핀의 기본 상태가 HIGH이고, 불꽃 감지 센서는 불꽃이 감지되었을 때 LOW를 보내준다.

    http_connection(WEBHOOK_PATH_FLAME);
    //    payload_flame +=  "Flame Detected!";
    //    payload_flame.toCharArray(data, (payload_flame.length() + 1));
    mqtt_client.publish(pub_flame, FLAME_DETECTED_MSG, 1);

    STATE_LED = HIGH; // LED 상태를 켬으로 변경
    STATE_BUZZER_FLAME = HIGH; // 부저 상태를 울림으로 변경
    buzzertimeflame = millis(); // 현재 시간을 저장. '부저를 울리기 시작한' 현재 시간이 아닌 이유는, 불꽃이 감지되지 않을 때까지 계속해서 갱신하기 위함
  }
  else { // 불꽃이 감지되지 않았을 때
    if (!STATE_DOOR) STATE_LED = LOW; // LED 상태를 꺼짐으로 변경
    if (millis() - buzzertimeflame > buzzerinterval) STATE_BUZZER_FLAME = LOW; // 현재 시간과 마지막으로 불꽃이 켜진 시간의 차가
    // 위 buzzerinterval에서 설정한 부저를 울릴 시간 길이(외출모드일 때 5초, 아닐 때 1초)보다 커지면, 부저 상태를 안 울림으로 변경
  } // 부저가 꺼진 이후에도 계속 LOW를 입력하지만, 작동에는 문제가 없음
  /* 바로 위 주석과 관련, 만약 부저가 울리고 있는 상태가 필요하다면,
     전역변수로 int buzzerflag = 0; 을 선언하고, 위 if(!STATE_FLAME) 구문 마지막에 buzzerflag = 1; 을 넣고,
     else 구문 안의 if(millis() - buzzertimeflame > buzzerinterval) STATE_BUZZER_FLAME = LOW; 라는 if 구문 안에
     buzzerflag = 0; 을 추가하면 됨.
     아무튼 현재 상태로는 비록 부저가 꺼진 이후에도 계속 millis() - buzzertimeflame이 커지므로,
     STATE_BUZZER_FLAME = LOW; 가 계속 실행이 되긴 하나,
     현재 상태로는 작동에 문제도, 위에서 언급한 논리를 넣는다 해도 STATE_BUZZER_FLAME = LOW; 의 반복 실행은 막으나 실제로 보이는 차이점도 없음.
  */
  //<<(6)>>
  if (STATE_DOOR) { // 문이 열렸을 때
    send_event("DoorFlagon");

    http_connection(WEBHOOK_PATH_DOOROPEN);
    payload_door =  "DOOR OPEN Detected!";
    payload_door.toCharArray(data, (payload_door.length() + 1));
    mqtt_client.publish(pub_door, data, 1);

    STATE_LED = HIGH;
    STATE_BUZZER_DOOR = HIGH;
    buzzertimedoor = millis();
  }
  else { // 문이 닫혔을 때
    mqtt_client.publish(pub_door, DOOR_CLOSED_MSG, 1);

    if (STATE_FLAME) STATE_LED = LOW;
    if (STATE_LEAVING) { // 외출 모드인 경우,
      if (millis() - buzzertimedoor > buzzerinterval) STATE_BUZZER_DOOR = LOW; // 계속 부저를 울리다가 정해진 시간 길이가 지나면 끔.
    }
    else STATE_BUZZER_DOOR = LOW; // 외출 모드가 아닐 땐 기다리지 않고 바로 끔
  }
  //<<(7)>>
  digitalWrite(LED_pin, STATE_LED); // LED 상태에 따라 LED를 켜거나 끔

  //<<(8)>>
  if (STATE_BUZZER_FLAME && !STATE_BUZZER_DOOR) { // i) 불꽃 감지됨, 문 닫힘
    send_event("FlameFlagon");

    mqtt_client.publish(pub_flame, FLAME_DETECTED_MSG, 1);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(FLAME_DETECTED_MSG);
    display.display();
    if (!buzzernexttone) {
      tone(BUZZER_pin, 131, 100);
      if (buzzertonecount++ > 4) {
        buzzertonecount = 0;
        buzzernexttone = 1;
      }
    }
    else {
      tone(BUZZER_pin, 262, 100);
      if (buzzertonecount++ > 4) {
        buzzertonecount = 0;
        buzzernexttone = 0;
      }
    }
  }
  else if (!STATE_BUZZER_FLAME && STATE_BUZZER_DOOR) { // ii) 불꽃 감지 안 됨, 문 열림
    tone(BUZZER_pin, 262, 100);
    mqtt_client.publish(pub_flame, FLAME_UNDETECTED_MSG, 1);
    mqtt_client.publish(pub_door, DOOR_OPEN_MSG, 1);
    display.setCursor(0, 1);
    display.clearDisplay();
    display.println(DOOR_OPEN_MSG);
    display.display();
  }
  else if (STATE_BUZZER_FLAME && STATE_BUZZER_DOOR) { // iii) 불꽃 감지됨, 문 열림
    send_event("DoorFlagon");
    send_event("FlameFlagon");
    mqtt_client.publish(pub_flame, FLAME_DETECTED_MSG, 1);
    mqtt_client.publish(pub_door, DOOR_OPEN_MSG, 1);
    display.setCursor(0, 1);
    display.clearDisplay();
    display.println(DOOR_OPEN_MSG);
    display.println(FLAME_DETECTED_MSG);
    display.display();
    if (!buzzernexttone) {
      tone(BUZZER_pin, 131, 100);
      if (buzzertonecount++ > 1) {
        buzzertonecount = 0;
        buzzernexttone = 1;
      }
    }
    else {
      tone(BUZZER_pin, 262, 100);
      if (buzzertonecount++ > 1) {
        buzzertonecount = 0;
        buzzernexttone = 0;
      }
    }
  }
  else { // iv) 불꽃 감지 안 됨, 문 닫힘
    mqtt_client.publish(pub_flame, FLAME_UNDETECTED_MSG, 1);
    mqtt_client.publish(pub_door, DOOR_CLOSED_MSG, 1);
    display.clearDisplay();
    display.setCursor(0, 0);
    if (Gasflag) {
      //////######################################################################################
      display.println("Gas detected");
    }
    else {
      display.println("Yunsuk office");
    }
    display.print("Temp:");
    if (temp.length() == 0)
      display.println("NULL");
    else
      display.println(temp);
    display.print("Hum:");
    if (hum.length() == 0)
      display.println("NULL");
    else
      display.println(hum);
    if (WiFi.status() == WL_CONNECTED)
    {
      display.println("WiFi: ON");
      display.print(WiFi.localIP());
    }
    else
    {
      display.print("WiFi: OFF");
    }
    display.display();
    noTone(BUZZER_pin);
    buzzertonecount = 0; buzzernexttone = 0;
  }
  delay(100); // void loop() 함수 반복 주기 0.1초
  mqtt_client.loop();
}

String http_connection(String url)
{
  String payload;
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0)
      payload = http.getString();
    http.end();
  }
  return payload;
}

void listen_webhook()
{
  WiFiClient client = server.available();
  if (!client)
  {
    delay(100);
    return;
  }
  int ct = 0;
  while (!client.available())
  {
    ct++;
    delay(100);
    Serial.print(",");
    if (ct > 10000) return;
    if (ct % 3 == 0)
      display.print("/");
    else if (ct % 3 == 1)
      display.print("|");
    else
      display.print("\\");
  }

  String request = client.readStringUntil('\r');
  if (request.indexOf("/Humidity") != -1)
  {
    hum = getValue(getValue(request, '/', 2), ' ', 0);
  }
  if (request.indexOf("/Temperature") != -1)
  {
    temp = getValue(getValue(request, '/', 2), ' ', 0);
  }
  if (request.indexOf("/Gas") != -1) {
    Gasflag = 1;
  } else {
    Gasflag = 0;
  }

  display.display();
  client.flush();
}

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
void send_event(const char *event) {
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(IFTTT_HOST, httpPort)) {
    return;
  }
  String url = "/trigger/";
  url += event;
  url += "/with/key/";
  url += IFTTT_KEY;
  String mJson = String("{\"value1\": \"") + "IoT System Design... \"}";
  client.println(String("POST ") + url + " HTTP/1.1");
  client.println(String("Host: ") + IFTTT_HOST);
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(mJson.length());
  client.println();
  client.println(mJson);
  delay(300);
  client.stop();
}
