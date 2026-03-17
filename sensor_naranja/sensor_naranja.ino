#include "esp_camera.h"
#include "board_config.h"

// Función para convertir RGB565 a HSV y detectar naranja
bool isOrange(uint16_t rgb565) {
  // 1. Extraer canales RGB (RGB565 a RGB888)
  uint8_t r = (rgb565 & 0xF800) >> 8;
  uint8_t g = (rgb565 & 0x07E0) >> 3;
  uint8_t b = (rgb565 & 0x001F) << 3;

  // 2. Convertir a rangos de 0 a 1 para cálculos
  float fr = r / 255.0;
  float fg = g / 255.0;
  float fb = b / 255.0;

  float cmax = max(fr, max(fg, fb));
  float cmin = min(fr, min(fg, fb));
  float diff = cmax - cmin;

  float h = 0, s = 0, v = cmax;

  // 3. Calcular Matiz (Hue)
  if (diff == 0) {
    h = 0;
  } else if (cmax == fr) {
    h = 60 * fmod(((fg - fb) / diff), 6);
  } else if (cmax == fg) {
    h = 60 * (((fb - fr) / diff) + 2);
  } else if (cmax == fb) {
    h = 60 * (((fr - fg) / diff) + 4);
  }
  if (h < 0) h += 360;

  // 4. Calcular Saturación
  if (cmax != 0) s = diff / cmax;

  // 5. FILTRO NARANJA: Ajusta estos valores según tu iluminación
  if (h >= 10 && h <= 45 && s >= 0.4 && v >= 0.4) {
    return true; // Es un 1 en la matriz
  }
  return false; // Es un 0 en la matriz
}

void setup() {
  Serial.begin(115200);
  
  // TRUCO CLAVE: Esperar 3 segundos para que el PC abra el puerto USB
  delay(3000); 
  
  Serial.println("\n=========================================");
  Serial.println("  INICIANDO SISTEMA DE VISIÓN BATTLEBOT  ");
  Serial.println("=========================================\n");

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
  config.frame_size = FRAMESIZE_QQVGA;    
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("❌ Error de cámara: 0x%x\n", err);
    return;
  }
  
  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  
  Serial.println("✅ Cámara lista. Empezando ciclo de rastreo...");
}

// Variable para controlar el tiempo del latido en la terminal
unsigned long ultimoLatido = 0;

void loop() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Fallo al capturar fotograma");
    delay(100);
    return;
  }

  long m00 = 0; 
  long m10 = 0; 
  long m01 = 0; 

  uint16_t *pixels = (uint16_t *)fb->buf;
  int width = fb->width;
  int height = fb->height;
  int pixel_index = 0;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      uint16_t current_pixel = pixels[pixel_index];
      
      if (isOrange(current_pixel)) {
        m00 += 1;   
        m10 += x;   
        m01 += y;   
      }
      pixel_index++;
    }
  }

  // Si encuentra una mancha naranja sólida (más de 15 píxeles para ignorar ruido)
  if (m00 > 15) {
    int centro_x = m10 / m00;
    int centro_y = m01 / m00;
    
    // Imprime la detección de forma limpia y clara
    Serial.printf("\r[!] OBJETIVO FIJADO -> X: %d, Y: %d | Area: %ld      \n", centro_x, centro_y, m00);
  } else {
    // Si no ve nada, imprime un latido (un punto) cada 500ms
    if (millis() - ultimoLatido > 500) {
      Serial.print(".");
      ultimoLatido = millis();
    }
  }

  esp_camera_fb_return(fb); 
  delay(30); // Pausa táctica de 30ms para trabajar a unos 30 FPS estables
}