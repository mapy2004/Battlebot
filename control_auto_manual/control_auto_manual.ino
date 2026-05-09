#include "esp_camera.h"
#include "board_config.h"
#include <WiFi.h>
#include <Bluepad32.h>


// =======================================================
// CONFIGURACIÓN DE PINES BTS7960 (100% SEGUROS)
// =======================================================
#define PIN_IZQ_ADELANTE 1  // RPWM Motor Izquierdo
#define PIN_IZQ_ATRAS    2  // LPWM Motor Izquierdo
#define PIN_DER_ADELANTE 3  // RPWM Motor Derecho
#define PIN_DER_ATRAS    47 // LPWM Motor Derecho

#define PIN_SIERRA_ADELANTE 42  // RPWM SIERRA
#define PIN_SIERRA_ATRAS   40    // LPWM SIERRA
#define PIN_SIERRA_REN  41 // Right enable sierra
#define PIN_SIERRA_LEN  39 // left enable sierra

#define SPEAKER 21 // pin buzzer
#define CH_SPK  7 //canal buzzer


// Canales PWM reservados para motores y sierra
#define CH_IZQ_ADELANTE     1
#define CH_IZQ_ATRAS        2
#define CH_DER_ADELANTE     3
#define CH_DER_ATRAS        4
#define CH_SIERRA_ADELANTE  5
#define CH_SIERRA_ATRAS     6

#define PWM_FREQ 20000
#define PWM_RES  8

// --- MEDIDOR DE FPS ---
unsigned long tiempo_ultimo_fotograma = 0;
// ----------------------

// ----- credenciales punto WIFI -------
const char* ssid = "Battlebot_CAM";
const char* password = "12345678";

// ======= funciones conexion bluetooth mando =============
ControllerPtr mando = nullptr;

enum ModoRobot{
  MODO_MANUAL,
  MODO_AUTOMATICO
};
enum EstadoRobot {
  ESTADO_BUSQUEDA,
  ESTADO_ATAQUE
};
ModoRobot modo_actual = MODO_AUTOMATICO; // modo automatico por defecto
EstadoRobot estado_actual = ESTADO_BUSQUEDA;

void onConnectedController(ControllerPtr ctl) {
  mando = ctl;
  Serial.println("Mando conectado");
}

void onDisconnectedController(ControllerPtr ctl) {
  if (mando == ctl) {
    mando = nullptr;
    Serial.println("Mando desconectado");
  }
}
// ===================================================


void setup() {
  Serial.begin(115200);
  delay(3000); 

 // ------- creacion conexion bluetooth -------------------
   BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.forgetBluetoothKeys();   // útil al probar emparejamientos nuevos
  Serial.println("Esperando mando Xbox...");
 // --------------------------------------------------------

 //----------------------------------------------------------
  ledcSetup(CH_SPK, 2000, 8); // canal, frecuencia, resolución
  ledcAttachPin(SPEAKER, CH_SPK);
  
 //----------------------------------------------------------
  
  // ------ configuracion SIERRA --------------------------
  ledcSetup(CH_SIERRA_ADELANTE, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_SIERRA_ADELANTE, CH_SIERRA_ADELANTE);

  ledcSetup(CH_SIERRA_ATRAS, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_SIERRA_ATRAS, CH_SIERRA_ATRAS);

  pinMode(PIN_SIERRA_REN, OUTPUT);
  pinMode(PIN_SIERRA_LEN, OUTPUT);

  // habilitar driver
  digitalWrite(PIN_SIERRA_REN, HIGH);
  digitalWrite(PIN_SIERRA_LEN, HIGH);

  moverSierra(0);
//--------------------------------------------------------


  // ----- configuracion e inicializacion pines motores y camara ----------------
  Serial.println("\n=========================================");
  Serial.println("  SISTEMA DE VISIÓN Y TRACCIÓN INICIADO  ");
  Serial.println("=========================================\n");

  // Motor izquierdo
  ledcSetup(CH_IZQ_ADELANTE, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_IZQ_ADELANTE, CH_IZQ_ADELANTE);

  ledcSetup(CH_IZQ_ATRAS, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_IZQ_ATRAS, CH_IZQ_ATRAS);

  // Motor derecho
  ledcSetup(CH_DER_ADELANTE, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_DER_ADELANTE, CH_DER_ADELANTE);

  ledcSetup(CH_DER_ATRAS, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_DER_ATRAS, CH_DER_ATRAS);
  
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
  //--------------------------------------------------------------     
}

//========= Función de control de potencia bruta de motores ==========
void moverMotores(int velIzq, int velDer) {
  velIzq = constrain(velIzq, -255, 255);
  velDer = constrain(velDer, -255, 255);

  // IZQUIERDO
    if (velIzq >= 0) {
      ledcWrite(CH_IZQ_ADELANTE, velIzq);
      ledcWrite(CH_IZQ_ATRAS, 0);
    } else {
      ledcWrite(CH_IZQ_ADELANTE, 0);
      ledcWrite(CH_IZQ_ATRAS, abs(velIzq));
    }

    // DERECHO
    if (velDer >= 0) {
      ledcWrite(CH_DER_ADELANTE, velDer);
      ledcWrite(CH_DER_ATRAS, 0);
    } else {
      ledcWrite(CH_DER_ADELANTE, 0);
      ledcWrite(CH_DER_ATRAS, abs(velDer));
    }
}
// ==================================================================

// ===== funcion control movimiento SIERRA =======================
void moverSierra(int potencia) { // potencia 0 al 255

  potencia = constrain(potencia, 0, 255); // limitar valor de potencia

  ledcWrite(CH_SIERRA_ADELANTE, potencia);
  ledcWrite(CH_SIERRA_ATRAS, 0);
}
//===================================================================


// === Función de caza optimizada con ENTEROS (Ultra-rápida) ===========
// Aproximación de Fucsia/Magenta: Alto Rojo, Alto Azul, Bajo Verde
// Función de caza optimizada con ENTEROS (Calibrada para pantalla)
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
// ==================================================================


// ===== Función lectura valores mando =================
//  variables globales mando 

int joystickX = 0;
int joystickY = 0;
int gatilloDer = 0;
int gatilloIzq = 0;

bool botonA = false;
bool botonB = false;
bool botonX = false;
bool botonY = false;

// ====================================
void leerMando(){
   
if (mando && mando->isConnected()) {

    // Lectura joystick izquierdo
    joystickX = mando->axisX();
        // Invierto Y para que joystick arriba sea avance positivo
    joystickY = -mando->axisY();
    // Zona muerta
    if (abs(joystickX) < 30) joystickX = 0;
    if (abs(joystickY) < 30) joystickY = 0;

    // Lectura botones
    botonA = mando->a();
    botonB = mando->b();
    botonX = mando->x();
    botonY = mando->y();

    // lectura de gatillos 0->1023
    gatilloDer = mando->throttle();
    gatilloIzq = mando->brake();


    // comprobacion
   // Serial.print("X: ");
   // Serial.print(joystickX);

    //Serial.print("  Y: ");
    //Serial.print(joystickY);

    //Serial.print("  A: ");
    //Serial.print(botonA);

    //Serial.print("  B: ");
    //Serial.println(botonB);

    //Serial.print("  gatillo derecho: ");
    //Serial.println(gatilloDer);

    //Serial.print("  gatillo izquierdo: ");
    //Serial.println(gatilloIzq);


  }else {

    joystickX = 0;
    joystickY = 0;

    gatilloDer= 0;
    gatilloIzq= 0;

    botonA = false;
    botonB = false;
    botonX = false;
    botonY = false;
}
}
// ========================================================

// === funcion cambio modo auto o manual ==================
void actualizarModo() {

  static unsigned long tiempoInicioA = 0;
  static bool cambioRealizado = false;

  const unsigned long TIEMPO_CAMBIO = 3000; // 3 segundos

  // Si no hay mando conectado, vuelve a automático
  if (!mando || !mando->isConnected()) {
    if (modo_actual != MODO_AUTOMATICO) {
      moverMotores(0, 0);
      modo_actual = MODO_AUTOMATICO;
      Serial.println("Modo AUTOMATICO por defecto: mando desconectado");
      sonarBuzzer(1000, 1000); //avisar del cambio de modo con buzzer
    }

    tiempoInicioA = 0;
    cambioRealizado = false;
    return;
  }

  // Si se pulsa A drante 3 seg se cambia al modo manual
  if (botonA) {

    if (tiempoInicioA == 0) {
      tiempoInicioA = millis();
    }

    if (!cambioRealizado && millis() - tiempoInicioA >= TIEMPO_CAMBIO) {

      moverMotores(0, 0);

      if (modo_actual == MODO_AUTOMATICO) {
        modo_actual = MODO_MANUAL;
        sonarBuzzer(1000, 1000); // avisar del cambio de modo con buzzer
        Serial.println("Cambio a MODO MANUAL");
      } else {
        modo_actual = MODO_AUTOMATICO;
        sonarBuzzer(1000, 1000); // avisar del cambio de modo con buzzer
        Serial.println("Cambio a MODO AUTOMATICO");
      }

      cambioRealizado = true;
    }

  } else {
    tiempoInicioA = 0;
    cambioRealizado = false;
  }
}
// ========================================================

// ==== funcion control de la sierra segun modo de operacion ===
void actualizarSierra() {

  if (modo_actual == MODO_MANUAL) { // modo manual 

    int potenciaSierra = map(gatilloDer, 0, 1023, 0, 255); // mover sierra con gatillo derecho
    moverSierra(potenciaSierra);

  } else {
    moverSierra(50);// valor temporal sierra
  }
}
//==============================================================

// Variables globales simplificadas


int ultima_X_conocida = 160; 

const int CENTRO_CAMARA_X = 160; 
const float Kp_CURVATURA = 1.2; 
const int PWM_BASE_MAX = 255;   
const int PWM_BUSQUEDA = 120;   
const int AREA_MINIMA_ATAQUE = 250;  

void loop() {
  BP32.update();
  leerMando(); //  lectura mando bluetooth 
  actualizarModo(); // cambiar modo funcionamiento
  
  // --- FSM modos de funcionamiento -----
  switch(modo_actual){
    case MODO_AUTOMATICO:
    ModoAutomatico();
    break;

    case MODO_MANUAL:
    ModoManual();
    break;
  }
  actualizarSierra(); // mover sierra siempre
  actualizarSonidoEstado();
  actualizarBuzzer();
}

//======= funcion modo manual ============================
void ModoManual(){

  int avance = joystickY;
  int giro   = joystickX;

  int velIzq = avance + giro;
  int velDer = avance - giro;

  velIzq = constrain(velIzq, -512, 512);
  velDer = constrain(velDer, -512, 512);

  velIzq = map(velIzq, -512, 512, -255, 255);
  velDer = map(velDer, -512, 512, -255, 255);

  moverMotores(velIzq, velDer);
}
// ========================================================

// ======= funcion del modo automatico ============================
void ModoAutomatico(){

    Serial.println("Entrando en ModoAutomatico");
  unsigned long tiempo_inicio = millis();
// --------- lectura color camara -------------
  camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
    Serial.println("ERROR: no se pudo capturar frame de camara");
    return;
  }
  Serial.println("Frame capturado correctamente");

  uint16_t *pixels = (uint16_t *)fb->buf;
  int width = fb->width;
  int limite_suelo = fb->height - 60; 
  
  long m00 = 0, m10 = 0, m01 = 0; 

  // FASE 1: Binarización + Acreción + Momentos EN UNA SOLA PASADA
  // Subsampling: Saltamos de 2 en 2 píxeles para ir 4 veces más rápido
  for (int y = 0; y < limite_suelo; y += 2) 
  {
    for (int x = 0; x < width; x += 2) 
    {
      uint16_t raw_pixel = pixels[y * width + x];
      uint16_t pixel_real = (raw_pixel >> 8) | (raw_pixel << 8);
      
      uint8_t r = (pixel_real & 0xF800) >> 8;
      uint8_t g = (pixel_real & 0x07E0) >> 3;
      uint8_t b = (pixel_real & 0x001F) << 3;
      
      if (isTargetFast(r, g, b)) 
      {
        // Al encontrar un acierto, inflamos el área (acreción virtual)
        // Como saltamos de 2 en 2, cada acierto cuenta como 4 píxeles
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

 // Serial.println("\n--- TELEMETRÍA DE COMBATE ---");
 // Serial.printf("[SISTEMA] Rendimiento: %.1f FPS (%.0f ms por frame)\n", fps, (float)tiempo_fotograma);

  // MFS Y NAVEGACIÓN
  // Filtro adaptado al subsampling
  // ------- ESTADO DE ATAQUE -------
 if (m00 > AREA_MINIMA_ATAQUE) { 
    estado_actual = ESTADO_ATAQUE;
    int centro_x = m10 / m00;
    ultima_X_conocida = centro_x; 

    int error_x = centro_x - CENTRO_CAMARA_X;
    float correccion_giro = error_x * Kp_CURVATURA; 

    int velocidad_ataque = PWM_BASE_MAX;
   // if (m00 > 2000) velocidad_ataque = 150; 
   // if (m00 > 5000) velocidad_ataque = 0;   

    int pwm_izq = velocidad_ataque + correccion_giro;
    int pwm_der = velocidad_ataque - correccion_giro;
    
    moverMotores(pwm_izq, pwm_der);

    Serial.printf("[ATAQUE] X:%d | Area:%ld | L:%d R:%d\n", centro_x, m00, pwm_izq, pwm_der);

    // ------- ESTADO DE BUSQUEDA -------
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

//=========== funciones SONIDO BUZZER =====================
bool buzzerActivo = false;
unsigned long tiempoInicioBuzzer = 0;
unsigned long duracionBuzzer = 0;
void sonarBuzzer(int frecuencia, unsigned long duracion){

  ledcWriteTone(CH_SPK, frecuencia);

  tiempoInicioBuzzer = millis();

  duracionBuzzer = duracion;

  buzzerActivo = true;

}

void actualizarBuzzer(){

  if(buzzerActivo){

    if(millis() - tiempoInicioBuzzer >= duracionBuzzer){

      ledcWriteTone(CH_SPK, 0);

      buzzerActivo = false;

    }
  }
}
// ================================================================================


// ========= SONIDO SEGÚN ESTADO ============================================

unsigned long ultimoPitidoEstado = 0;

void actualizarSonidoEstado() {

  if (modo_actual != MODO_AUTOMATICO) {
    ultimoPitidoEstado = millis();
    return;
  }

  unsigned long intervalo;
  unsigned long duracion;
  int frecuencia;

  if (estado_actual == ESTADO_BUSQUEDA) {
    frecuencia = 1500;
    intervalo = 800;
    duracion = 80;
  }
  else if (estado_actual == ESTADO_ATAQUE) {
    frecuencia = 1500;
    intervalo = 200;
    duracion = 80;
  }
  else {
    return;
  }

  if (millis() - ultimoPitidoEstado >= intervalo) {
    sonarBuzzer(frecuencia, duracion);
    ultimoPitidoEstado = millis();
  }
}