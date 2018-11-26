/**
 * @file  Arduino_MQTT_Temperature.ino
 * 
 * @brief MQTT経由で温度・湿度センサーの値をモニタリングします
 *
 * @author IoT Algyan in Hiroshima. K.Nakamura.
 *
 * @date   2018.11.03
*/

/** 
 * 温度・湿度センサー : HTS221
 * ライブラリ         : STM32duino_HTS221
 * 
 * WIFIドライバー     : STM32duino_ISM43362-M3G-L44
 * MQTTクライアント   : PubSubClient
 */

#include <SPI.h>
#include <WiFiST.h>
#include <PubSubClient.h>
#include <HTS221Sensor.h>
#include <LPS22HBSensor.h>
#include <LIS3MDLSensor.h>
#include <math.h>


// WiFi 設定 (SSID)
char* ssid              = "Buffalo-G-AE22";
//char* ssid              = "kei.nak5";
// WiFi 設定 (パスワード)
const char* password    = "12345678";
//const char* password    = "kei.nak5";
// MQTT サーバ (ＩＰアドレス)
#define AIO_SERVER      "192.168.11.2"
// MQTT サーバ (ポート番号)
#define AIO_SERVERPORT  1883
// MQTT サーバ (ユーザーＩＤ)
#define AIO_USERNAME    "user"
// MQTT サーバ (パスワード)
#define AIO_KEY         "pass"

// SPIバス接続ライブラリ(WiFiで使用)
SPIClass SPI_3(PC12, PC11, PC10);

// FiWiモジュールの定義
WiFiClass WiFi(&SPI_3, PE0, PE1, PE8, PB13);
WiFiClient STClient;

// FiWiステータス
int status = WL_IDLE_STATUS;

// HTS221センサーオブジェクト
HTS221Sensor  *HumTemp;

// LPS22HBセンサーオブジェクト
LPS22HBSensor *PressTemp;

// ３軸地磁気センサー
LIS3MDLSensor *Magneto;

TwoWire *dev_i2c;
#define I2C2_SCL    PB10
#define I2C2_SDA    PB11


// MQTTクライアント
PubSubClient client(STClient);
long lastMsg = 0;
char publishMsg[512];
char cid[32] = {0};

/**
 * セットアップ
 */
void setup() {
  // LEDピン設定
  pinMode(LED_BUILTIN, OUTPUT);

  // シリアル通信初期化
  Serial.begin(115200);

  //  WiFi のセットアップ
  setup_wifi();

  // MQTTクライアント接続設定
  client.setServer(AIO_SERVER, AIO_SERVERPORT);
  client.setCallback(callback);

  // I2Cバス接続ライブラリの初期化
  dev_i2c = new TwoWire(I2C2_SDA, I2C2_SCL);
  dev_i2c->begin();

  // 温度・湿度センサーライブラリの初期化
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

/**
 *  WiFi のセットアップ
 */
void setup_wifi() {

  delay(10);

  // WiFiステータスのチェック (WL_NO_SHIELD の場合は起動できない)
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi module not detected");
    // don't continue:
    while (true);
  }

  // WiFiファームウェアバージョンの取得とチェック
  String fv = WiFi.firmwareVersion();
  Serial.print("Firmware version: ");
  Serial.println(fv);


  // WiFiが接続できるまでループする
  Serial.print("Attempting to connect to network: ");
  Serial.println(ssid);
  while (status != WL_CONNECTED) {
    Serial.print(".");
    // WiFiネットワークに接続
    status = WiFi.begin(ssid, password);
    // ２秒ウェイト
    delay(2000);
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("SSID : ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // クライアントIDを作成
  sprintf(cid, "cid_%d", (int)WiFi.localIP()[3]);
  Serial.print("cid : ");
  Serial.println(cid);
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

/**
 * MQTT接続(再接続)
 * MQTTは切断される場合があるので切断されている場合に再接続を行う処理が必須となる。
 */
void reconnect() {
  // MQTT接続ループ
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // MQTT接続処理
    if (client.connect("STM32Client", AIO_USERNAME, AIO_KEY)) {
      Serial.println("connected");
      // 接続時のメッセージ (以下は Topicが "user/feeds/hello", Payloadが "Hi, I'm STM32 user!"となる)
      client.publish(AIO_USERNAME"/feeds/hello", "Hi, I'm STM32 user!");
      // resubscribe 購読設定
      client.subscribe(AIO_USERNAME"/feeds/onoff");
    } else {
      // 失敗した場合は  5 秒 スリープして再チャレンジ
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/**
 * メインループ処理
 */
void loop() {
  float temperature = 0.0;
  float humidity = 0.0;
  float pressure = 0.0;
  // ３軸地磁気センサデータ用バッファ [0] -> X, [1] -> Y, [2] -> Z に対応
  int32_t magnetometer[3] = {0};

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // 前回から 10000msec (10秒) 経過していれば、Publishメッセージを送信
  long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;

    // 気温の取得
    HumTemp->GetTemperature(&temperature);
    // 湿度の取得
    HumTemp->GetHumidity(&humidity);
    // 気圧の取得
    PressTemp->GetPressure(&pressure);
    // ３軸地磁気センサのセンサデータの読み出し
    Magneto->GetAxes(magnetometer);

    // publishメッセージ(json)の構築
    snprintf(publishMsg, 512, "{\"cid\":%s,\"temperature\":%4.1f, \"humidity\":%u, \"pressure\":%4.1f, \"MagX\":%d, \"MagY\":%d, \"MagZ\":%d}",
            cid, temperature, (unsigned int)humidity, pressure, magnetometer[0], magnetometer[1], magnetometer[2]);

    // publish メッセージを送信
    client.publish(AIO_USERNAME"/feeds/data", publishMsg);

    Serial.print("Publish : ");
    Serial.println(publishMsg);
  }
}
