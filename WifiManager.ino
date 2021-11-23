
#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#define TRIGGER_PIN 19

#define SETUP     1
#define COUNTING  2
#define RESET     3

#define PIN_IN 4
#define PIN_OUT 14

void TaskCounter( void *pvParameters );
void TaskRequestIn( void *pvParameters );
void TaskRequestOut( void *pvParameters );
void TaskButton( void *pvParameters );
void ShowLcd( void *pvParameters );

WiFiManager wm;
WiFiManagerParameter * custom_api;
WiFiManagerParameter * custom_api_key;

LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7);

struct Config {
  char api[64];
  char apiKey[64];
  int qtyNotSend;
} config;

int LCD_STATE = SETUP;
bool LCD_CLEAR = false;
bool SERVER_ERROR = false;


String ApName;
String password = "password";


String deviceId = "";

long QTY_IN_TAMPIL = 0;
long QTY_OUT_TAMPIL = 0;
long QTY_IN_POST = 0;
long QTY_OUT_POST = 0;

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  pinMode(PIN_IN, INPUT_PULLUP);
  pinMode(PIN_OUT, INPUT_PULLUP);

  ApName =  "ESP32_";

  deviceId = getDeviceId();

  //Setting WiFi
  WiFi.mode(WIFI_STA);

  // Init SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount FS");
    return;
  }
  loadConfig();


  // Setting WiFi Manager
  wifiConfig();

  LCD_STATE = COUNTING;

  xTaskCreatePinnedToCore(
    TaskCounter,              /* Task function. */
    "TaskCounter",            /* String with name of task. */
    10000,                     /* Stack size in bytes. */
    NULL,                     /* Parameter passed as input of the task */
    3,                        /* Priority of the task. */
    NULL,                     /* Task handle. */
    ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
    TaskRequestIn,            /* Task function. */
    "TaskRequestIn",          /* String with name of task. */
    10000,                     /* Stack size in bytes. */
    NULL,                     /* Parameter passed as input of the task */
    2,                        /* Priority of the task. */
    NULL,                     /* Task handle. */
    ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
    TaskRequestOut,           /* Task function. */
    "TaskRequestOut",         /* String with name of task. */
    10000,                     /* Stack size in bytes. */
    NULL,                     /* Parameter passed as input of the task */
    2,                        /* Priority of the task. */
    NULL,                     /* Task handle. */
    ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
    TaskButton,               /* Task function. */
    "TaskButton",             /* String with name of task. */
    10000,                     /* Stack size in bytes. */
    NULL,                     /* Parameter passed as input of the task */
    1,                        /* Priority of the task. */
    NULL,                     /* Task handle. */
    ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
    ShowLcd,                  /* Task function. */
    "ShowLcd",                /* String with name of task. */
    10000,                    /* Stack size in bytes. */
    NULL,                     /* Parameter passed as input of the task */
    1,                        /* Priority of the task. */
    NULL,                     /* Task handle. */
    ARDUINO_RUNNING_CORE);
}

void loop() {}

void TaskCounter(void *pvParameters)
{

  int DETECT_IN = 0;
  int DETECT_OUT = 0;
  int KODE_IN = 0;
  int KODE_OUT = 0;
  int TAMPUNG_IN = 0 ;
  for (;;) {
    DETECT_IN = digitalRead(PIN_IN);
    DETECT_OUT = digitalRead(PIN_OUT);

    if (DETECT_IN == LOW) {
      KODE_IN = 1;
      delay(100);
    } else if ((KODE_IN == 1) && (DETECT_IN == HIGH)) {
      ++TAMPUNG_IN;
      if (TAMPUNG_IN == 2) {
        QTY_IN_TAMPIL = QTY_IN_TAMPIL + 1;
        QTY_IN_POST = QTY_IN_POST + 1;
        TAMPUNG_IN = 0;
        Serial.println(QTY_IN_TAMPIL);
      }
      KODE_IN = 0;

    }

    if (DETECT_OUT == LOW) {
      KODE_OUT = 1;
      delay(100);
    } else if ((KODE_OUT == 1) && (DETECT_OUT == HIGH)) {
      QTY_OUT_TAMPIL = QTY_OUT_TAMPIL + 1;
      QTY_OUT_POST = QTY_OUT_POST + 1;
      KODE_OUT = 0;
      Serial.println(QTY_OUT_TAMPIL);
    }

    vTaskDelay(10);

  }
}


void TaskRequestIn(void *pvParameters)  // This is a task.
{
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (QTY_IN_POST > 0) {
        postIn(QTY_IN_POST);
      }
    } else {
      WiFi.begin();
    }
    vTaskDelay(10000);
  }
}


void TaskRequestOut(void *pvParameters)  // This is a task.
{
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (QTY_OUT_POST > 0) {
        PostOut(QTY_OUT_POST);
      }
    } else {
      WiFi.begin();
    }
    vTaskDelay(10000);
  }
}

void TaskButton(void *pvParameters)  // This is a task.
{
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  for (;;) {
    checkButton();
    //    Serial.println("Loop");
    vTaskDelay(10);
  }
}


void ShowLcd(void *pvParameters)  // This is a task.
{
  lcd.begin(16, 2);
  lcd.setBacklightPin(3, POSITIVE);
  lcd.setBacklight(HIGH);
  lcd.clear();
  for (;;) {
    if (LCD_CLEAR) {
      lcd.clear();
      LCD_CLEAR = false;
    }
    switch (LCD_STATE) {
      case SETUP: {
          lcd.setCursor(0, 0);
          lcd.print("Setup WiFi");
          lcd.setCursor(0, 1);
          lcd.print("192.168.4.1");
        } break;
      case COUNTING: {
          char show[40];
          sprintf(show, "In: %d Out: %d", QTY_IN_TAMPIL, QTY_OUT_TAMPIL);
          lcd.setCursor(0, 0);
          lcd.print(show);
          if (WiFi.status() == WL_CONNECTED) {
            if (!SERVER_ERROR) {
              lcd.setCursor(0, 1);
              lcd.print("Connected     ");
            } else {
              lcd.setCursor(0, 1);
              lcd.print("Server Error  ");
            }
          } else {
            lcd.setCursor(0, 1);
            lcd.print("Not Connected");
          }
        } break;
      case RESET: {
          lcd.setCursor(0, 0);
          lcd.print("Reset");
        } break;
    }
    vTaskDelay(100);
  }
}


void loadConfig() {
  if (SPIFFS.exists("/config.json")) {
    File file = SPIFFS.open("/config.json", "r");
    if (file) {
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, file);
      if (error) {
        Serial.println(F("Failed to read file, using default configuration"));
      }
      // Set To Stuct
      strlcpy(config.api, doc["api"], sizeof(config.apiKey));
      strlcpy(config.apiKey, doc["apiKey"], sizeof(config.apiKey));
      config.qtyNotSend = doc["qtyNotSend"];
    }
    file.close();
  }
}

void saveConfig() {
  //Save Stuct to SPIFFS
  if (SPIFFS.exists("/config.json")) {
    File file = SPIFFS.open("/config.json", "w");
    if (file) {
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, file);
      if (error)
        Serial.println(F("Failed to read file, using default configuration"));
      doc["api"] = config.api;
      doc["apiKey"] = config.apiKey;
      doc["qtyNotSend"] = config.qtyNotSend;
      if (serializeJson(doc, file) == 0) {
        Serial.println(F("Failed to write to file"));
      }
    }
    file.close();
  }
}

void wifiConfig() {
  Serial.println("\n Starting");
  // wm.resetSettings();

  // Add Custom Params
  custom_api = new WiFiManagerParameter("api", "Api Server", config.api, 64);
  custom_api_key = new WiFiManagerParameter("api_key", "Api Key", config.apiKey, 64);
  wm.addParameter(custom_api);
  wm.addParameter(custom_api_key);
  wm.setSaveParamsCallback(saveParamCallback);

  // Set Main Page Menu
  std::vector<const char *> menu = {"wifi", "info", "param", "sep", "restart", "exit"};
  wm.setMenu(menu);

  // set dark theme
  wm.setClass("invert");

  wm.setConfigPortalTimeout(30); // auto close configportal after n seconds

}

void checkButton() {
  // check for button press
  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    // poor mans debounce/press-hold, code not ideal for production
    delay(50);
    if ( digitalRead(TRIGGER_PIN) == LOW ) {
      Serial.println("Button Pressed");

      delay(3000); // reset delay hold
      if ( digitalRead(TRIGGER_PIN) == LOW ) {
        Serial.println("Button Held");
        Serial.println("Erasing Config, restarting");
        LCD_CLEAR = true;
        LCD_STATE = RESET;
        delay(2000);
        //        wm.resetSettings();
        ESP.restart();
      }
      LCD_CLEAR = true;
      LCD_STATE = SETUP;
      // start portal w delay
      Serial.println("Starting config portal");
      wm.setConfigPortalTimeout(60);

      if (!wm.startConfigPortal(ApName.c_str(), password.c_str())) {
        Serial.println("failed to connect or hit timeout");
        delay(3000);
        ESP.restart();
      } else {
        LCD_STATE = COUNTING;
        //if you get here you have connected to the WiFi
        Serial.println("connected...yeey :)");
      }
    }
  }
}

String getParam(String name) {
  //read parameter from server, for customhmtl input
  String value;
  if (wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback() {
  strlcpy(config.api, getParam("api").c_str(), sizeof(config.apiKey));
  strlcpy(config.apiKey, getParam("api_key").c_str(), sizeof(config.apiKey));
  saveConfig();
  ESP.restart();
  Serial.println("[CALLBACK] saveParamCallback fired");
  Serial.println("PARAM Api Key = " + getParam("api"));
  Serial.println("PARAM Api Key = " + getParam("api_key"));
}

void postIn(int qty) {
  HTTPClient http;

  http.begin(String(config.api) + "/chickenin/device");
  http.addHeader("Content-Type", "application/json");
  StaticJsonDocument<200> doc;

  doc["apiKey"] = config.apiKey;
  doc["deviceName"] = deviceId;
  doc["qty"] = qty;

  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody);

  if (httpResponseCode == 201 || httpResponseCode == 200) {
    SERVER_ERROR = false;
    String response = http.getString();
    QTY_IN_POST = QTY_IN_POST - qty;
    Serial.println(httpResponseCode);
    Serial.println(response);
  } else {
    SERVER_ERROR = true;
  }
}

void PostOut(int qty) {
  HTTPClient http;

  http.begin(String(config.api) + "/chickenout/device");
  http.addHeader("Content-Type", "application/json");
  StaticJsonDocument<200> doc;

  doc["apiKey"] = config.apiKey;
  doc["deviceName"] = deviceId;
  doc["qty"] = qty;

  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody);

  if (httpResponseCode == 201 || httpResponseCode == 200) {
    SERVER_ERROR = false;
    String response = http.getString();
    QTY_OUT_POST = QTY_OUT_POST - qty;
    Serial.println(httpResponseCode);
    Serial.println(response);
  } else {
    SERVER_ERROR = true;
  }
}

String getDeviceId() {
  char chipResult[40];
  uint64_t mac = ESP.getEfuseMac();
  sprintf(chipResult, "%04X%08X", (uint16_t)(mac >> 32),
          (uint32_t)mac); // print High 2 bytes
  return String(chipResult);
}
