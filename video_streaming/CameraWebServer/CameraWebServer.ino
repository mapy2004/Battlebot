#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "board_config.h"
#include <DHT.h>
#include <esp_now.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

#define DHTPIN 40
#define DHTTYPE DHT11   
DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long Vm_startMillis = 0;  //some global variables available anywhere in the program
unsigned long Vm_currentMillis = 0;
const unsigned long Vm_period = 1000;

const int analogPin = 14; // salida div resistivo bateria
const float dividerRatio = 0.244; // Your measured ratio
const float refVoltage = 3.3;     // Measure your 3V3 pin and update this for 100% accuracy
const float UMBRAL_BATERIA_BAJA = 10;
float temperatura = 0;   // variable global temperatura
camera_config_t config;
//------- variables ESP-NOW -------------
uint8_t macESPPrincipal[] = {0xE8, 0xF6, 0x0A, 0x89, 0xDC, 0x1C};
typedef struct {
  bool avisoBateriaBaja;
} MensajeAviso;
MensajeAviso aviso;
bool avisoBateriaEnviado = false;
// ===========================
// Enter your WiFi credentials
// ===========================

const char *ssid = "iphone_de_lucasduck";
const char *password = "BELGICA931";


void startCameraServer();
void setupLedFlash();
//void leerTemperatura();
//======= funcion saca porcentaje bateria ================
float getBatteryPercent(float batteryVoltage){
  float cellVoltage = batteryVoltage / 3.0; // LiPo 3S

  if (cellVoltage >= 4.20) return 100;
  if (cellVoltage <= 3.30) return 0;

  return (cellVoltage - 3.30) * 100.0 / (4.20 - 3.30);
}
//=========================================================

// ====== funcion envio de datos ESP-NOW =====================
void enviarAvisoBateriaBaja() {

  aviso.avisoBateriaBaja = true;

  esp_err_t resultado = esp_now_send(
    macESPPrincipal,
    (uint8_t *) &aviso,
    sizeof(aviso)
  );

  if (resultado == ESP_OK) {
    Serial.println("Aviso de bateria baja enviado");
  } else {
    Serial.println("Error enviando aviso");
  }
}
//============================================================
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  dht.begin(); // iniciar sensor temp
  Serial.println("Iniciando programa...");

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000; // cambiado, antes: de 2mill
  config.frame_size = FRAMESIZE_QVGA; // cambiado, antes:FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode =  CAMERA_GRAB_LATEST; // cambiado, antes: CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;// cambiado, antes: CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 15;
  config.fb_count = 2;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 12;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_QVGA;//  QVGA
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_hmirror(s, 1); // espejo horizontal
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash();
#endif  
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  IPAddress local_IP(172, 20, 10, 10);
  IPAddress gateway(172, 20, 10, 1);
  IPAddress subnet(255, 255, 255, 240);

if (!WiFi.config(local_IP, gateway, subnet)) {
  Serial.println("Error configurando IP estatica");
}
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    if (millis() - t0 > 15000) {
      Serial.println("\nNo se pudo conectar al WiFi");
      Serial.print("Estado WiFi: ");
      Serial.println(WiFi.status());
      return;
    }
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP asignada: ");
  Serial.println(WiFi.localIP());

  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());

  Serial.print("Subnet: ");
  Serial.println(WiFi.subnetMask());

  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP()); //http://172.20.10.10
  Serial.println("' to connect");

  // --- incializacion pantalla 
  Wire.begin(1, 2);   // SDA = GPIO1, SCL = GPIO2
  Serial.println("pines I2C asignados");
 if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
   Serial.println(F("SSD1306 allocation failed"));
   while (true);
 }
  display.setRotation(2);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Battery Monitor");
  display.display();

  Serial.println("OLED OK");
//---- atenuiacion ADC
  analogReadResolution(12);
  analogSetPinAttenuation(analogPin, ADC_11db);
  pinMode(analogPin, INPUT);


// ===== INICIALIZACION ESP-NOW =====

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error iniciando ESP-NOW");
    return;
  }

  esp_now_peer_info_t peerInfo = {};

  memcpy(peerInfo.peer_addr, macESPPrincipal, 6);
  Serial.print("Canal WiFi actual: ");
  Serial.println(WiFi.channel());
  peerInfo.channel = WiFi.channel();

  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Error añadiendo peer");
    return;
  }

  Serial.println("ESP-NOW iniciado en ESP secundario");



  delay(1000);

}

void loop() {
  // ---- lectura de bateria y mostrar en displays ------
  Vm_currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
  if (Vm_currentMillis - Vm_startMillis >= Vm_period)  //test whether the period has elapsed
  {

   Vm_startMillis = millis();

   // medir temp 
  // leerTemperatura();
  // 1. Calculate Voltage
  // hacer una media de las lecturas
  long sum = 0;
  for (int i = 0; i < 20; i++) {
    sum += analogRead(analogPin);
    delay(2);
  }
  int rawValue = sum / 20;
  float pinVoltage = (rawValue / 4095.0) * refVoltage;
  float batteryVoltage = pinVoltage / dividerRatio;
  float batteryPercent = getBatteryPercent(batteryVoltage);


  Serial.print("rawValue = ");
  Serial.print(rawValue);
  Serial.print(" | pinVoltage = ");
  Serial.print(pinVoltage);
  Serial.print(" V | batteryVoltage = ");
  Serial.print(batteryVoltage);
  Serial.println(" V");
  //Serial.print("Temperatura: ");
  //Serial.print(temperatura);
  // 2. Update Display
  display.clearDisplay();

  

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("LIPO: ");
  display.print(batteryVoltage, 2);
  display.println(" V");

  // display.setCursor(0,12);
  // display.print("Temperatura: ");
  // display.print(temperatura);
  // display.print(" C");

  display.setTextSize(1);
  display.setCursor(0, 12);
  display.print("Battery: ");
  display.print(batteryPercent, 0);
  display.println(" %");

  display.setTextSize(1);
  display.setCursor(0, 24);
  if (batteryVoltage < UMBRAL_BATERIA_BAJA ) {
    display.print("LOW BATTERY!");   
    display.invertDisplay(true);

    if (!avisoBateriaEnviado){
        enviarAvisoBateriaBaja();
      avisoBateriaEnviado = true;
    }
  } else {
    display.invertDisplay(false);
    display.print("STATUS OK");
  }
  
  display.display();
  }
}

