#include "esp_camera.h"
#include "board_config.h"
#include <ESP32Servo.h> // LIBRERÍA DE SERVOS

// =======================================================
// CONFIGURACIÓN DE PINES PARA SERVOS (MODO PRUEBA)
// =======================================================
#define PIN_SERVO_IZQ 3  // Señal Servo Izquierdo
#define PIN_SERVO_DER 14 // Señal Servo Derecho

Servo servoIzq;
Servo servoDer;

// --- MEDIDOR DE FPS ---
unsigned long tiempo_ultimo_fotograma = 0;
// ----------------------

void setup() {
  Serial.begin(115200);
  delay(3000); 
  
  Serial.println("\n=========================================");
  Serial.println("  SISTEMA DE VISIÓN (MODO PRUEBA SERVOS) ");
  Serial.println("=========================================\n");

  // Asignar los pines a los objetos Servo
  servoIzq.attach(PIN_SERVO_IZQ);
  servoDer.attach(PIN_SERVO_DER);
  
  // Ponerlos en posición neutral (90 grados / parados) al arrancar
  servoIzq.write(90);
  servoDer.write(90);

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
  s->set_whitebal(s, 0);       
  s->set_awb_gain(s, 0);       
  s->set_saturation(s, 2);     
  s->set_brightness(s, -1);    
  s->set_contrast(s, 1);       
}

// Función de control adaptada para Servos
// Traduce el PWM bruto (-255 a 255) a grados de servo (0 a 180)
void moverMotores(int velIzq, int velDer) {
  // 1. Blindaje matemático (-255 a 255)
  velIzq = constrain(velIzq, -255, 255);
  velDer = constrain(velDer, -255, 255);

  // 2. Mapeo a grados (0 a 180)
  // -255 = 0 grados (Marcha atrás máxima)
  // 0    = 90 grados (Parado)
  // 255  = 180 grados (Marcha adelante máxima)
  int anguloIzq = map(velIzq, -255, 255, 0, 180);
  int anguloDer = map(velDer, -255, 255, 0, 180);

  // 3. Enviar la señal física a los servos
  servoIzq.write(anguloIzq);
  servoDer.write(anguloDer);
}

// Función de caza optimizada con ENTEROS (Calibrada para pantalla)
bool isTargetFast(uint8_t r, uint8_t g, uint8_t b) {
  // 1. Brillo mínimo: la pantalla emite mucha luz (R y B altos)
  if (r > 120 && b > 120) {
    // 2. Dominancia: Rojo y Azul deben superar al Verde.
    if (g < (r - 25) && g < (b - 25)) {
      // 3. Equilibrio: Rojo y azul deben ser similares para ser Fucsia
      if (abs(r - b) < 60) {
        return true;
      }
    }
  }
  return false;
}

// Variables globales simplificadas
enum EstadoRobot {
  ESTADO_BUSQUEDA,
  ESTADO_ATAQUE
};
EstadoRobot estado_actual = ESTADO_BUSQUEDA;

int ultima_X_conocida = 160; 

const int CENTRO_CAMARA_X = 160; 
const float Kp_CURVATURA = 1.2; 
const int PWM_BASE_MAX = 255;   
const int PWM_BUSQUEDA = 120;   

void loop() {
  unsigned long tiempo_inicio = millis();

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  uint16_t *pixels = (uint16_t *)fb->buf;
  int width = fb->width;
  int limite_suelo = fb->height - 60; 
  
  long m00 = 0, m10 = 0, m01 = 0; 

  // FASE 1: Binarización + Acreción + Momentos EN UNA SOLA PASADA
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

  // Medición de Rendimiento
  unsigned long tiempo_fin = millis();
  unsigned long tiempo_fotograma = tiempo_fin - tiempo_inicio;
  float fps = 1000.0 / tiempo_fotograma;

  Serial.println("\n--- TELEMETRÍA DE COMBATE ---");
  Serial.printf("[SISTEMA] Rendimiento: %.1f FPS (%.0f ms por frame)\n", fps, (float)tiempo_fotograma);

  // MFS Y NAVEGACIÓN
  if (m00 > 30) {
    estado_actual = ESTADO_ATAQUE;
    int centro_x = m10 / m00;
    ultima_X_conocida = centro_x; 

    int error_x = centro_x - CENTRO_CAMARA_X;
    float correccion_giro = error_x * Kp_CURVATURA; 

    int velocidad_ataque = PWM_BASE_MAX;
    if (m00 > 2000) velocidad_ataque = 150; 
    if (m00 > 5000) velocidad_ataque = 0;   

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