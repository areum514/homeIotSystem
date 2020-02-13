#include <SPI.h>              // include SPI library
#include <Adafruit_GFX.h>     // include adafruit graphics library
#include <Adafruit_PCD8544.h> // include adafruit PCD8544 (Nokia 5110) library
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>

#define DEBUG 0
#define DEBUG_WITHOUT_INTERNET 0
#define DEBUG_STANDALONE 0
#define DEBUG_WITH_STATIC_IP 0
//Define Pin
#define TX 1
#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12
#define D7 13
#define D8 15

//Define general/constant
#define BOARD_RATE 9600
#define MyRoomLocation "Room2"
#define DURATION_DEFAULT 1000
int curr_time = 999999;

//Define LED
#define LED_pin D1
#define LED_ON HIGH
#define LED_OFF LOW
bool LED_STATE = LED_OFF;
bool LED_activate = false;

//Define BUZZER
#define BUZZER_pin D2
#define BUZZER_ON HIGH
#define BUZZER_OFF LOW
#define DURATION_BUZZER_GASMIDDLE 2000
#define DURATION_BUZZER_GASHIGH 300
bool BUZZER_STATE = BUZZER_OFF;
bool BUZZER_activate = false;
bool BUZZER_activate_ON = false;
bool BUZZER_activate_OFF = false;
int BUZZER_duration = DURATION_DEFAULT;
int BUZZERON_prev_time = -999999;
int BUZZEROFF_prev_time = -999999;

//Define LCD
Adafruit_PCD8544 display = Adafruit_PCD8544(D5, D7, D6, D8, D0);
#define DURATION_LCD_MODEVIEW 5000
#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2
#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH 16
bool LCD_activate = false;
bool LCD_mode_view = false;
bool LCD_warnGAS_view = false;
bool LCD__view = false;
int LCD_modeview_prev_time = -999999;
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

//Define BUTTON
#define BUTTON_ON LOW
#define BUTTON_OFF HIGH
#define LEAVING_ON HIGH
#define LEAVING_OFF LOW
bool BUTTON_STATE = BUTTON_OFF;
bool pre_BUTTON_STATE = BUTTON_OFF;
bool LEAVING_STATE = LEAVING_OFF;

//Define BME280
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 BME; // I2C
bool BME_check = false;
float temperature, humidity;
int BME_prev_time = -999999;

//Define GASsensor
#define GAS_pin A0
#define GAS_MIDDLE 250
#define GAS_HIGH 350
String payload_gas = "";
#if DEBUG
#define DELAY_GAS_LEAVINGON 1000
#define DELAY_GAS_LEAVINGOFF 1000
#define DELAY_BME_LEAVINGON 1000
#define DELAY_BME_LEAVINGOFF 1000
#define DELAY_DOOROPEN_LEAVINGON 1000
#define DELAY_DOOROPEN_LEAVINGOFF 1000
#define DELAY_FLAME_LEAVINGON 10000
#define DELAY_FLAME_LEAVINGOFF 10000
#else
#define DELAY_GAS_LEAVINGON 300000
#define DELAY_GAS_LEAVINGOFF 150000
#define DELAY_BME_LEAVINGON 1000
#define DELAY_BME_LEAVINGOFF 1000
#define DELAY_DOOROPEN_LEAVINGON 1000 // NODE2 에서 설정한 딜레이와 동일하게
#define DELAY_DOOROPEN_LEAVINGOFF 1000// NODE2 에서 설정한 딜레이와 동일하게
#define DELAY_FLAME_LEAVINGON 1000    // NODE2 에서 설정한 딜레이와 동일하게
#define DELAY_FLAME_LEAVINGOFF 1000   // NODE2 에서 설정한 딜레이와 동일하게
#endif
int GAS_value;
bool GAS_check = false, GAS_middle = false, GAS_high = false, GAS_DETECTED = false;
bool GAS_DETECTED_BUZZER = false;
int GAS_prev_time = -999999;

WiFiServer server(80);
#define WEBHOOK_PROTOCOL "http://"
#define NODE1_IP "192.168.0.104"
#define WEBHOOK_PATH_HUMIDITY WEBHOOK_PROTOCOL NODE1_IP "/Humidity/"
#define WEBHOOK_PATH_TEMP     WEBHOOK_PROTOCOL NODE1_IP "/Temperature/"
#define WEBHOOK_PATH_GAS      WEBHOOK_PROTOCOL NODE1_IP "/Gas/"
bool FLAME_DETECTED = false;
bool FLAME_DETECTED_BUZZER = false;
bool DOOROPEN_DETECTED = false;
bool DOOROPEN_DETECTED_BUZZER = false;
int FLAME_prev_time = -999999;
int DOOROPEN_prev_time = -999999;

#define IFTTT_HOST "maker.ifttt.com"
#define IFTTT_KEY "bvdK0S9W6_lsDU0IS-ze_t"
// Wifi
const char *wifi_ssid = "HGU_IoT_3"; // 사용하는 공유기 이름
const char *wifi_password = "csee1414";       // 공유기 password 입력

// MQTT
#define mqtt_broker "192.168.0.92" //MQTT broker address(자신의 Raspberry Pi3 IP), 노트북을 통한 모바일 핫스팟에서 IP check
#define mqtt_user "iot"
#define mqtt_pwd "csee1414"

// 개별 nodeMCU 설정시 수정해야하는 부분(숫자를 자신의 주어진 번호로 모두 수정) - 자신의 NodeMCU NAME
//Publish를 위한 Topic
const char *mqtt_nodeName = "myhome";
const char *pub_dht = "myhome/room2/dht22";
const char *pub_dht_t = "myhome/room2/dht22_t";
const char *pub_dht_h = "myhome/room2/dht22_h";
const char *pub_gas = "myhome/room2/gas";
//Subscribe Topic
const char *sub_topic2 = "myhome/room2"; // #로 하면 모든 토픽으로부터 수신을 한다.
// 해당코드에서 1을 자신의 학번으로 수정하면 됨.
#define TOPIC_GREETINGS "Hello from nodeMCU at NTH405"
const char *pub_door = "myhome/room1/door";
const char *pub_flame = "myhome/room1/flame";
const char *pub_mode = "myhome/room2/mode";
const char *pub_wifi1 = "myhome/room1/wifi";
const char *pub_wifi2 = "myhome/room2/wifi2";

WiFiClient wifiClient; //클라이언트로 작동
char data[80];

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
PubSubClient mqtt_client(mqtt_broker, 1883, callback, wifiClient);
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

void setup()
{
  Serial.begin(BOARD_RATE);
  InitWiFi();
  server.begin(); // setup webserver
  pinMode(TX, INPUT);
  //LED begin
  pinMode(LED_pin, OUTPUT);
  //BUZZER begin
  pinMode(BUZZER_pin, OUTPUT);
  //LCD begin
  display.begin();
  display.setContrast(50);
  display.clearDisplay(); // clears the screen and buffer
  //BME begin
  Wire.begin(D3, D4);
  bool status = BME.begin(0x76);
  BME_prev_time = millis();
  temperature = BME.readTemperature();
  humidity = BME.readHumidity();
  mqtt_client.setServer(mqtt_broker, 1883);
  mqtt_client.setCallback(callback);
  //  //Connection to MQTT broker
  mqtt_client.connect(mqtt_nodeName, mqtt_user, mqtt_pwd);
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    listen_webhook();
    mqtt_client.publish(pub_wifi1, "WIFI ON", 1);
  }
  else
  {
    mqtt_client.publish(pub_wifi1, "WIFI OFF", 1);
    InitWiFi();
  }
  // String data from Node#1 (Define at Level 2)
  String Door_data = "", Fire_data = "";
  // String data to Node#1
  String Button_data = "", BME_data = "", GAS_data = "";
  String RoomLocation = "";

  define_checkCondition();

  Button_data = check_BUTTON();
  BME_data = check_BME();
  GAS_data = check_GAS();

  define_controlCondition();

  control_LED();
  control_BUZZER();
  control_LCD(RoomLocation);

  mqtt_client.loop();
  delay(100);
}

void define_checkCondition()
{
  //Input data check condition (Add check condition of other sensors at Level 2)
  curr_time = millis();
  //BUTTON_check = curr_time-BUTTON_prev_time>DELAY_BUTTON; //add if necessary
  if (LEAVING_STATE == LEAVING_ON)
  {
    BME_check = (curr_time - BME_prev_time) > DELAY_BME_LEAVINGON;
    GAS_check = (curr_time - GAS_prev_time) > DELAY_GAS_LEAVINGON;
    FLAME_DETECTED = (curr_time - FLAME_prev_time < DELAY_FLAME_LEAVINGON);
    DOOROPEN_DETECTED = (curr_time - DOOROPEN_prev_time < DELAY_DOOROPEN_LEAVINGON);
  }
  else if (LEAVING_STATE == LEAVING_OFF)
  {
    BME_check = (curr_time - BME_prev_time) > DELAY_BME_LEAVINGOFF;
    GAS_check = (curr_time - GAS_prev_time) > DELAY_GAS_LEAVINGOFF;
    FLAME_DETECTED = (curr_time - FLAME_prev_time < DELAY_FLAME_LEAVINGOFF);
    DOOROPEN_DETECTED = (curr_time - DOOROPEN_prev_time < DELAY_DOOROPEN_LEAVINGOFF);
  }
}

//Input 1.Button ('Leaving mode' control)
String check_BUTTON()
{
  String BUTTON_data = "";
  DynamicJsonDocument doc(1024);
  BUTTON_STATE = digitalRead(TX);
  if (BUTTON_STATE == BUTTON_ON && pre_BUTTON_STATE != BUTTON_ON)
  {
    LEAVING_STATE = !LEAVING_STATE;
    LCD_modeview_prev_time = millis();
    pre_BUTTON_STATE = BUTTON_ON;
    doc["RoomLocation"] = MyRoomLocation;
    if (LEAVING_STATE == LEAVING_ON)
    {
      doc["LeavingState"] = "LeavingModeON";
    }
    else
    {
      doc["LeavingState"] = "LeavingModeOFF";
    }
  }
  else
    pre_BUTTON_STATE = BUTTON_OFF;
  serializeJson(doc, BUTTON_data);
  return BUTTON_data;
}

//Input 2.BME (temperature and humidity)
String check_BME()
{
  String BME_data = "";
  DynamicJsonDocument doc(1024);
  if (BME_check)
  {
    temperature = BME.readTemperature();
    humidity = BME.readHumidity();

#if !DEBUG_STANDALONE
    String webhook_link_t(WEBHOOK_PATH_TEMP + String(temperature));
    String webhook_link_h(WEBHOOK_PATH_HUMIDITY + String(humidity));
    http_connection(webhook_link_t);
    http_connection(webhook_link_h);

    String payload_dht = "{";
    payload_dht += "Hum: ";
    payload_dht += humidity;
    payload_dht += ", ";
    payload_dht += "Temp: ";
    payload_dht += temperature;
    payload_dht += "C";
    payload_dht += "}";
    payload_dht.toCharArray(data, (payload_dht.length() + 1));
    mqtt_client.publish(pub_dht, data, 1);
    sprintf(data, "%3.1f", temperature);
    mqtt_client.publish(pub_dht_t, data, 1);
    sprintf(data, "%3.0f", humidity);
    mqtt_client.publish(pub_dht_h, data, 1);
#endif
    BME_prev_time = millis();
    doc["RoomLocation"] = MyRoomLocation;
    doc["Temperature"] = temperature;
    doc["Humidity"] = humidity;
  }
  return BME_data;
}

//Input 3.GAS
String check_GAS()
{
  String GAS_data = "";
  DynamicJsonDocument doc(1024);
  if (GAS_check)
  {
    GAS_value = analogRead(GAS_pin);
    GAS_prev_time = millis();

    if (GAS_value >= GAS_MIDDLE && GAS_value < GAS_HIGH)
    {
#if !DEBUG_STANDALONE
      String webhook_link_g(WEBHOOK_PATH_GAS + String(GAS_value));
      http_connection(webhook_link_g);
      mqtt_client.publish(pub_gas, "GAS MIDDLE detected", 1);
#endif
      GAS_middle = HIGH;

    }
    else GAS_middle = LOW;

    if (GAS_value >= GAS_HIGH) {
      send_event("GasFlagon");
#if !DEBUG_STANDALONE
      String webhook_link_g(WEBHOOK_PATH_GAS + String(GAS_value));
      http_connection(webhook_link_g);
#endif
      GAS_high = HIGH;
      payload_gas = "Gas detected!!!!!";
      payload_gas.toCharArray(data, (payload_gas.length() + 1));
      mqtt_client.publish(pub_gas, "GAS HIGH detected", 1);
    }
    else {
      GAS_high = LOW;
      mqtt_client.publish(pub_gas, "Gas undetected", 1);
    }

    GAS_DETECTED = (GAS_middle || GAS_high);
    GAS_DETECTED_BUZZER = true;
    doc["RoomLocation"] = MyRoomLocation;
    doc["GasValue"] = GAS_value;
  }
  return GAS_data;
}

void define_controlCondition()
{
  //Output data control condition (modify each condition at Level 2)
  LED_activate = (LEAVING_STATE == LEAVING_OFF && GAS_DETECTED) || (LEAVING_STATE == LEAVING_OFF && FLAME_DETECTED) || (LEAVING_STATE == LEAVING_OFF && DOOROPEN_DETECTED);
  if (GAS_middle)
    BUZZER_duration = DURATION_BUZZER_GASMIDDLE;
  else if (GAS_high)

  BUZZER_duration = DURATION_BUZZER_GASHIGH;
  BUZZER_activate = (LEAVING_STATE == LEAVING_OFF && GAS_DETECTED) || (LEAVING_STATE == LEAVING_OFF && FLAME_DETECTED) || (LEAVING_STATE == LEAVING_ON && DOOROPEN_DETECTED);
  LCD_activate = (LEAVING_STATE == LEAVING_OFF || LCD_mode_view);
  LCD_mode_view = (curr_time - LCD_modeview_prev_time < DURATION_LCD_MODEVIEW);
  LCD_warnGAS_view = (GAS_DETECTED);
  LCD__view = (LCD_activate && !(LCD_mode_view || LCD_warnGAS_view));
  /* Level2 addition
    LCD_warnDoor_view = ;
    LCD_warnFire_view = ;
  */
}

//Output 1.LED
void control_LED()
{
  //<CASE> - 1)GAS (add case when struct Level 2)
  if (LED_activate)
    LED_STATE = LED_ON;
  else
    LED_STATE = LED_OFF;
  digitalWrite(LED_pin, LED_STATE);
}

//Output 2.BUZZER
//<CASE> - 1)GAS[middle/high] (add case when struct Level 2)
void control_BUZZER()
{
  if (GAS_high && GAS_DETECTED_BUZZER)
  {
    // 3옥타브 도
    GAS_DETECTED_BUZZER = false;
    tone(BUZZER_pin, 131);
  }
  else if (GAS_middle && GAS_DETECTED_BUZZER)
  {
    // 4옥타브 도
    GAS_DETECTED_BUZZER = false;
    tone(BUZZER_pin, 261);
  }
  else if (FLAME_DETECTED && FLAME_DETECTED_BUZZER)
  {
    // 3옥타브 미
    FLAME_DETECTED_BUZZER = false;
    tone(BUZZER_pin, 164);
  }
  else if (DOOROPEN_DETECTED && DOOROPEN_DETECTED_BUZZER)
  {
    // 3옥타브 솔
    DOOROPEN_DETECTED_BUZZER = false;
    tone(BUZZER_pin, 184);
  }
  else{
    noTone(BUZZER_pin);
  }
}

//Output 3.LCD
void control_LCD(String RoomLocation)
{
    //<CASE> - 1)mode view   2)warnGAS view   3) view   4)non-view (add case when struct Level 2)
    display.clearDisplay();
    if (LCD_activate)
    {
        display.setTextSize(0.8);
        display.setTextColor(BLACK);
        display.setCursor(0, 0);
        if (LCD_mode_view)
        {
            display.setTextSize(1);
            display.setTextColor(WHITE, BLACK);
            display.setCursor(0, 17);
            display.print("Leav. Mode ");
            if (LEAVING_STATE == LEAVING_ON)
                display.println("ON");
            else
                display.println("OFF");
        }
        else
        {
            display.print("Location:");
            if (RoomLocation == "")
                display.println(MyRoomLocation);
            else
                display.println(RoomLocation);
        }
        if (LCD_warnGAS_view)
        {
            display.println("GAS Detected!");
            display.print("GAS: ");
            display.print(GAS_value);
            display.print(" < ");
            display.println(GAS_MIDDLE);
        }
        else if (LCD__view)
        {
            display.print("Temp: ");
            display.print(temperature, 1);
            // display.print(BME.readTemperature(), 1);
            display.println(" *C");
            display.print("Hum: ");
            display.print(humidity, 1);
            // display.print(BME.readHumidity(), 1);
            display.println(" %");
            //TEST - GAS
            //       display.print("(TEST)GAS: ");
            //       display.println(GAS_value);
            //TEST - LEAVINGMODE
            //display.print("(TEST)LEAVINGMODE: ");
            //display.println(LEAVING_STATE);
        }
        if (FLAME_DETECTED)
        {
            display.println("FLAME DETECTED");
        }
        if (DOOROPEN_DETECTED)
        {
            display.println("DOOROPEN DETECTED");
        }
        if (WiFi.status() == WL_CONNECTED)
        {
            display.println("WiFi: ON");
            display.print(WiFi.localIP());
        }
        else
        {
            display.print("WiFi: OFF");
        }
    }
    display.display();
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
        if (ct == 10000)
            return;
    }
    String request = client.readStringUntil('\r');
    if (request.indexOf("/Flame") != -1)
    {
        FLAME_prev_time = millis();
        FLAME_DETECTED_BUZZER = true;
    }
    if (request.indexOf("/DoorOpen") != -1)
    {
        DOOROPEN_prev_time = millis();
        DOOROPEN_DETECTED_BUZZER = true;
    }
    client.flush();
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

void send_event(const char *event)
{
    WiFiClient client;
    const int httpPort = 80;
    if (!client.connect(IFTTT_HOST, httpPort))
    {
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
