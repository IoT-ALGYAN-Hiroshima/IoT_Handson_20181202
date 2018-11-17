/*
 B-L475E-IOT01A MQTT example to publish on https://io.adafruit.com broker
 See https://learn.adafruit.com/mqtt-adafruit-io-and-you/overview
 This sketch demonstrates the capabilities of the pubsub library in combination
 with the B-L475E-IOT01A board.
 It will use several components of the board and you need to install corresponding libraries :
   - WiFi (ISM43362-M3G-L44) : https://github.com/stm32duino/WiFi-ISM43362-M3G-L44
   - Temperature and humidity sensor (HTS221) : https://github.com/stm32duino/HTS221
   - Arduino Client for MQTT: https://github.com/knolleary/pubsubclient
 You can find more information on this board here :
   http://www.st.com/en/evaluation-tools/b-l475e-iot01a.html
 You will need to create some feeds in your Adafruit IO Dashboard.
 See https://learn.adafruit.com/mqtt-adafruit-io-and-you/getting-started-on-adafruit-io
 to know how register and create your Adafruit IO dashboard and feeds:
   - hello: Text block
   - onoff: Toggle block
   - temp: Stream block
   - hum: Gauge block
 A screenshot of the dashboard is available in the sketch directory: dashboard_adafruit.png
  It connects to the Adafruit IO's MQTT server (a.k.a broker) server then:
  - publishes announcement "Hi, I'm STM32 user!" to the topic 'AIO_USERNAME"/feeds/hello"'
  - subscribes to the topic AIO_USERNAME"/feeds/onoff"', switching LED_BUILTIN state
  - publishes temperature and humidity from the HTS221 sensor to the topic
    'AIO_USERNAME"/feeds/temp"' and 'AIO_USERNAME"/feeds/hum"' every 10 seconds
 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.
*/
#include <SPI.h>
#include <WiFiST.h>
#include <PubSubClient.h>
#include <HTS221Sensor.h>
// LPS22HBセンサーライブラリをインクルード
#include <LPS22HBSensor.h>
// ３軸地磁気センサーライブラリ
#include <LIS3MDLSensor.h>
// Ｍａｔｈヘッダ
#include <math.h>


// Update these with values suitable for your network.
//char* ssid              = "SPWN_H36_66D7AE";
//const char* password    = "tgfd3ad4jagb931";
//char* ssid              = "kei.nak5";
//const char* password    = "kei.nak5";
char* ssid              = "Buffalo-G-AE22";
const char* password    = "12345678";

// Adafruit.io Setup
#define AIO_SERVER      "192.168.11.2"
//#define AIO_SERVER      "192.168.137.1"
//#define AIO_SERVER      "192.168.100.103"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "user"
#define AIO_KEY         "pass"

// WiFi module setup
SPIClass SPI_3(PC12, PC11, PC10);
WiFiClass WiFi(&SPI_3, PE0, PE1, PE8, PB13);
WiFiClient STClient;
int status = WL_IDLE_STATUS; // the Wifi radio's status

// LPS22HBセンサーオブジェクト
LPS22HBSensor *PressTemp;

HTS221Sensor  *HumTemp;
// ３軸地磁気センサー
LIS3MDLSensor *Magneto;

TwoWire *dev_i2c;
#define I2C2_SCL    PB10
#define I2C2_SDA    PB11


// i2c sensors
PubSubClient client(STClient);
long lastMsg = 0;
char msg[8];
char msg2[8];
char publishMsg[512];

void setup() {
  pinMode(LED_BUILTIN, OUTPUT); // Initialize the LED_BUILTIN pin as an output
  Serial.begin(115200);
  setup_wifi();
  client.setServer(AIO_SERVER, AIO_SERVERPORT);
  client.setCallback(callback);

  // Initialize I2C bus.
  dev_i2c = new TwoWire(I2C2_SDA, I2C2_SCL);
  dev_i2c->begin();

  // Initlialize components.
  HumTemp = new HTS221Sensor (dev_i2c);
  HumTemp->Enable();

  // LPS22HBセンサーオブジェクトのインスタンス生成
  // (LPS22HB Sensorでは I2C を使用します)
  PressTemp = new LPS22HBSensor(dev_i2c);
  PressTemp->Enable();
  
  // ３軸地磁気センサー初期化
  Magneto = new LIS3MDLSensor(dev_i2c);
  Magneto->Enable();
}

void setup_wifi() {

  delay(10);

  // Initialize the WiFi module:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi module not detected");
    // don't continue:
    while (true);
  }

  // Print firmware version:
  String fv = WiFi.firmwareVersion();
  Serial.print("Firmware version: ");
  Serial.println(fv);

//  if (fv != "C3.5.2.3.BETA9") {
//    Serial.println("Please upgrade the firmware");
//  }

  // Attempt to connect to Wifi network:
  Serial.print("Attempting to connect to network: ");
  Serial.println(ssid);
  while (status != WL_CONNECTED) {
    Serial.print(".");
    // Connect to network:
    status = WiFi.begin(ssid, password);
    // Wait 10 seconds for connection:
    delay(10000);
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {

#if 1
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
#else
  UNUSED(topic);
#endif
  if (length > 1) {
    // Switch on/off the LED (payload messages can be 'ON' or 'OFF')
    if ((char)payload[1] == 'N') {
      digitalWrite(LED_BUILTIN, HIGH); // Turn the LED on
    } else {
      digitalWrite(LED_BUILTIN, LOW);  // Turn the LED off
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // Note - the default maximum packet size is 128 bytes. If the
    // combined length of clientId, username and password exceed this,
    // you will need to increase the value of MQTT_MAX_PACKET_SIZE in
    // PubSubClient.h
    if (client.connect("STM32Client", AIO_USERNAME, AIO_KEY)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(AIO_USERNAME"/feeds/hello", "Hi, I'm STM32 user!");
      // ... and resubscribe
      client.subscribe(AIO_USERNAME"/feeds/onoff");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  float temperature, humidity;
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
    HumTemp->GetTemperature(&temperature);
//    dtostrf(temperature, 2, 2, msg);
//    Serial.print("Publish temperature ");
//    Serial.println(msg);

    HumTemp->GetHumidity(&humidity);
//    snprintf (msg, 8, "%u", (unsigned int)humidity);
//    Serial.print("Publish humidity ");
//    Serial.println(msg);
    // 気圧の取得
    float pressure = 0.0;
    PressTemp->GetPressure(&pressure);

    // ３軸地磁気センサデータ用バッファ [0] -> X, [1] -> Y, [2] -> Z に対応
    int32_t magnetometer[3] = {0};
    // ３軸地磁気センサのセンサデータの読み出し
    Magneto->GetAxes(magnetometer);

    dtostrf(temperature, 2, 2, msg);
    dtostrf(pressure, 2, 2, msg2);
    
    snprintf(publishMsg, 512, "{\"temperature\":%s, \"humidity\":%u, \"pressure\":%s, \"MagX\":%d, \"MagY\":%d, \"MagZ\":%d}", msg, (unsigned int)humidity, msg2, magnetometer[0], magnetometer[1], magnetometer[2]);

    // publish メッセージを送信
    client.publish(AIO_USERNAME"/feeds/data", publishMsg);

    Serial.print("Publish : ");
    Serial.println(publishMsg);
  }
}