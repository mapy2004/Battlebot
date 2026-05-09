########## EXPLICACION DE CARPETAS Y CODIGOS #########################################

El ESP32-S3-principal tiene implementado el código de la carpeta "control_auto_manual".
Funcionalidades:
1) modo automatico: Modo por defecto. Utiliza el sistema de detección de color para detectar al enemigo. En caso de ser detectado entra en modo ataque (enciende la sierra y avanzan los motores de las ruedas para perseguir al enemigo). En caso de no detectar ningún enemigo cercano se mantiene en modo búsqueda (apaga la sierra y mueve el robot para explorar el terreno).
2) modo manual: Control total del robot a traves del mando Xbox conectado por bluetooth. Control de motores de las ruedas a través del joystick izquierdo del mando, control de la sierra con el trigger derecho. Activación del modo manual: conectando el mando por bluetooth y presionando el boton (A) durante 3 segundos. Para volver al modo automatio basta con volverá presionar el botón (A) odesconectar el mando bluetooth.


El ESP32-S3-secundario tiene implementado el código de la carpeta "video_streaming".
Funcionalidades:
1) Video streaming: Se conecta al un punto WIFI (creado por un móvil) y crea un servidor web desde donde se puede ver la retrasmisión de video en tiempo real desde otro dispositivo móvil accediendo a la dirección: http://172.20.10.10
2) Lectura de temperatura de motores y voltaje y porcentaje de batería mostrados en un display.


	