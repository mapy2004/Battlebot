#include "esp_camera.h"
#include "board_config.h"
#include <WiFi.h>

// =======================================================
// CONFIGURACIÓN DE PINES MOTORES (2x DRV8871)
// =======================================================
// Rueda Izquierda (DRV8871)
#define PIN_IZQ_IN1 1       // IN1 DRV8871 (Motor Izquierdo)
#define PIN_IZQ_IN2 2       // IN2 DRV8871 (Motor Izquierdo)

// Rueda Derecha (DRV8871)
#define PIN_DER_IN1 3       // IN1 DRV8871 (Motor Derecho)
#define PIN_DER_IN2 47      // IN2 DRV8871 (Motor Derecho)

// --- MEDIDOR DE FPS ---
unsigned long tiempo_ultimo_fotograma = 0;
// ----------------------

// ----- credenciales punto WIFI -------
const char* ssid = "Battlebot_CAM";
const char* password = "12345678";

void setup() {
  Serial.begin(115200);
  delay(3000); 

  //---- creacion punto   wifi----- 
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);

  bool ap_ok = WiFi.softAP(ssid, password);

  if (ap_ok) {
    Serial.println("Punto WiFi creado correctamente");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("IP del AP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("ERROR: no se pudo crear el punto WiFi");
  }

  Serial.println("\n=========================================");
  Serial.println("  SISTEMA DE VISIÓN Y TRACCIÓN INICIADO  ");
  Serial.println("=========================================\n");

  // Inicializar pines de motores como salidas
  pinMode(PIN_IZQ_IN1, OUTPUT);
  pinMode(PIN_IZQ_IN2, OUTPUT);
  pinMode(PIN_DER_IN1, OUTPUT);
  pinMode(PIN_DER_IN2, OUTPUT);
  
  moverMotores(0, 0);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  
  config.pixel_format = PIXFORMAT_RGB565; 
  config.frame_size = FRAMESIZE_QVGA; 
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) return;
  
  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  
  s->set_whitebal(s, 1);        
  s->set_awb_gain(s, 1);        
  s->set_saturation(s, 3);      

  s->set_brightness(s, -1);    
  s->set_contrast(s, 1);       
}

void moverMotores(int velIzq, int velDer) {
  velIzq = constrain(velIzq, -255, 255);
  velDer = constrain(velDer, -255, 255);

  if (velIzq >= 0) {
    analogWrite(PIN_IZQ_IN1, velIzq);
    analogWrite(PIN_IZQ_IN2, 0);
  } else {
    analogWrite(PIN_IZQ_IN1, 0);
    analogWrite(PIN_IZQ_IN2, abs(velIzq)); 
  }

  if (velDer >= 0) {
    analogWrite(PIN_DER_IN1, velDer);
    analogWrite(PIN_DER_IN2, 0);
  } else {
    analogWrite(PIN_DER_IN1, 0);
    analogWrite(PIN_DER_IN2, abs(velDer)); 
  }
}

// ======================================================================
// FÓRMULA DE CAZA: SINTONÍA FINA ANTI-PIEL ROSADA
// ======================================================================
bool isTargetFast(uint8_t r, uint8_t g, uint8_t b) {
  if (r > 80 && b > 50) { 
    
    // EL AJUSTE QUIRÚRGICO: 
    // Subimos la exigencia del Azul sobre el Verde de +5 a +12. 
    // La piel rosada tiene azul, pero rara vez supera al verde por más de 10 puntos.
    // El bote fucsia lo superará fácilmente por 20 o 30.
    if (b > (g + 12) && r > (g + 25)) {
      
      // Cerramos un pelín el embudo de 100 a 85 para evitar rosas pálidos/carne
      if (abs(r - b) < 85) {
        return true;
      }
    }
  }
  return false;
}
// ======================================================================

enum EstadoRobot {
  ESTADO_BUSQUEDA,
  ESTADO_ATAQUE
};
EstadoRobot estado_actual = ESTADO_BUSQUEDA;

int ultima_X_conocida = 160; 

// ======================================================================
// PARÁMETROS TÁCTICOS
// ======================================================================
const int CENTRO_CAMARA_X = 160; 
const float Kp_CURVATURA = 1.2; 
const int PWM_BASE_MAX = 255;   
const int PWM_BUSQUEDA = 120;   
const int AREA_MINIMA_ATAQUE = 250; 
// ======================================================================

void loop() {
  unsigned long tiempo_inicio = millis();

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  uint16_t *pixels = (uint16_t *)fb->buf;
  int width = fb->width;
  int limite_suelo = fb->height - 60; 
  
  long m00 = 0, m10 = 0, m01 = 0; 

  for (int y = 0; y < limite_suelo; y += 2) {
    for (int x = 0; x < width; x += 2) {
      uint16_t raw_pixel = pixels[y * width + x];
      uint16_t pixel_real = (raw_pixel >> 8) | (raw_pixel << 8);
      
      uint8_t r = (pixel_real & 0xF800) >> 8;
      uint8_t g = (pixel_real & 0x07E0) >> 3;
      uint8_t b = (pixel_real & 0x001F) << 3;
      
      if (isTargetFast(r, g, b)) {
        m00 += 4;   
        m10 += x * 4;   
        m01 += y * 4;   
      }
    }
  }

  unsigned long tiempo_fin = millis();
  unsigned long tiempo_fotograma = tiempo_fin - tiempo_inicio;
  float fps = 1000.0 / tiempo_fotograma;

  Serial.println("\n--- TELEMETRÍA DE COMBATE ---");
  Serial.printf("[SISTEMA] Rendimiento: %.1f FPS (%.0f ms por frame)\n", fps, (float)tiempo_fotograma);

  if (m00 > AREA_MINIMA_ATAQUE) { 
    estado_actual = ESTADO_ATAQUE;
    int centro_x = m10 / m00;
    ultima_X_conocida = centro_x; 

    int error_x = centro_x - CENTRO_CAMARA_X;
    float correccion_giro = error_x * Kp_CURVATURA; 

    int velocidad_ataque = PWM_BASE_MAX; 

    int pwm_izq = velocidad_ataque + correccion_giro;
    int pwm_der = velocidad_ataque - correccion_giro;
    
    moverMotores(pwm_izq, pwm_der);

    Serial.printf("[ATAQUE] X:%d | Area:%ld | L:%d R:%d\n", centro_x, m00, pwm_izq, pwm_der);
    
  } else {
    estado_actual = ESTADO_BUSQUEDA;

    int pwm_izq = 0;
    int pwm_der = 0;
    
    if (ultima_X_conocida < CENTRO_CAMARA_X - 20) {
      pwm_izq = -PWM_BUSQUEDA; 
      pwm_der = PWM_BUSQUEDA;  
      Serial.println("[BUSQUEDA] Rotando a la IZQUIERDA");
      
    } else if (ultima_X_conocida > CENTRO_CAMARA_X + 20) {
      pwm_izq = PWM_BUSQUEDA;  
      pwm_der = -PWM_BUSQUEDA; 
      Serial.println("[BUSQUEDA] Rotando a la DERECHA");
      
    } else {
      pwm_izq = PWM_BUSQUEDA;
      pwm_der = -PWM_BUSQUEDA;
      Serial.println("[BUSQUEDA] Rotando a la DERECHA (Default)");
    }

    moverMotores(pwm_izq, pwm_der);
  }

  esp_camera_fb_return(fb); 
}
