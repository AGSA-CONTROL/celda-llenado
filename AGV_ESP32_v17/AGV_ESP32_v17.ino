/*
  ======================================================================
  SEGUIDOR DE LINEA CON CONTROL PID + HMI WEB + LECTOR QR GM65 - ESP32-S3
  ======================================================================
  VERSION 8 (celda de llenado v17): botones para elegir el almacén (1, 2, 3 o rotativo) en el panel
    y en la página del carro. El carro se detiene en CADA estación, lee su QR y descarga solo
    en el QR del almacén elegido (QR_PAUSA_PASO_MS = tiempo detenido en las demás).
  VERSION 7: CONEXIÓN FÁCIL.
    - Escribe el nombre y la contraseña de TU WiFi en WIFI_RED / WIFI_CLAVE (abajo) y súbelo.
    - El ESP32 encuentra SOLO la laptop del servidor (la laptop avisa por la red cada 2 s),
      ya no hace falta escribir la IP. Si la red bloquea ese aviso, escribe la IP en el panel.
    - La página AGV_Carro_v17.html (modo "Carro ESP32") muestra y maneja este carro desde
      cualquier laptop de la misma red, sin conectarse a la red "SeguidorLineaPID".
  VERSION 6: MODO SIMULADOR dentro del ESP32.
    - En Mantenimiento > "Modo de trabajo" eliges Físico o Simulador
      (se guarda en la memoria del ESP32).
    - SIMULADOR: el ESP32 se conecta igual al servidor (rol agv) y hace
      TODO el viaje del lote, pero con los motores APAGADOS: la posición
      avanza por tiempo, el "lector" lee QR-E1..QR-E3 y QR-HOME al pasar
      por cada estación, la rampa hace su secuencia y regresa a carga.
      Sirve para probar la comunicación con el HMI y la tienda sin pista.
    - FÍSICO: exactamente igual que la versión 5 (sigue la línea real).
  VERSION 5: (a) el seguimiento sigue SOLO la zona más sensible del QTR-8A
  (pico + vecinos, ver leerLineaPico) para que las manchas de la pista no
  desvíen al AGV; (b) CUALQUIER QR detiene al robot y manda la señal
  "qr" (con codigo y estacion) al panel y al servidor; si no es el
  destino se detiene un momento (QR_PAUSA_PASO_MS) y sigue, salvo QR-HOME
  que SIEMPRE lo detiene; no descarga.
  VERSION 4: integra la UI del carro AGV-01 (circuito con 3 almacenes,
  vasos, QR y registro) y el MANTENIMIENTO (sensores, servos, QR, motores,
  PID, red). El ESP32 ES el carro: calcula el lote y los eventos y se
  conecta al servidor de la laptop (rol "agv") para que el HMI de llenado
  y la tienda sigan funcionando. Ver la sección CARRO DE ENTREGA.
  VERSION 3: igual que la v2 + DIAGNOSTICO DEL LECTOR QR (sin tocar el
  resto del control):
    - Contador de bytes recibidos del GM65 (chip "Bytes del lector" en
      el HMI y comando 'q' por Serial). Si queda en 0 al escanear, el
      lector NO esta mandando nada por UART (modo USB-HID, TX/RX
      cruzados, alimentacion, baudrate).
    - Comando 'w': volcado en hexadecimal de todo lo que llega del lector.
    - Comando 't': manda "QR-E1" por UART1. Con un puente entre GPIO39 y
      GPIO40 (lector desconectado) prueba todo: UART, firmware y HMI.
    - Comando 'z9600' / 'z115200': cambia el baudrate del lector al vuelo.

  Seguidor de línea con QTR-8A ANALOGICO + 2 drivers BTS7960 (IBT-2) +
  encoders de cuadratura en cada motor + 2 servos de rampa + LED
  direccionable de estado + lector QR GM65 conectado por UART (Serial1).
  El lector va por UART, NO por USB: así el ESP32-S3 no necesita ser
  host USB (no se usa EspUsbHost) y el texto llega sin problemas de
  distribución de teclado.

  --------------------------------------------------------------------
  MAPA DE PINES (ESP32-S3)
    QTR-8A analógico S1..S8 : GPIO1..GPIO8   (ADC1, compatible con WiFi)
    Encoder izquierdo A / B : GPIO9 / GPIO10
    BTS izquierdo           : GPIO11 RPWM, 12 LPWM, 13 R_EN, 14 L_EN
    BTS derecho             : GPIO15 RPWM, 16 LPWM, 17 R_EN, 18 L_EN
    Encoder derecho A / B   : GPIO21 / GPIO38
    Servo compuerta         : GPIO41
    Servo elevación         : GPIO42
    LED NeoPixel            : GPIO48
    Lector QR GM65 (UART1)  : GPIO39 = RX del ESP32 (<- TX del GM65)
                              GPIO40 = TX del ESP32 (-> RX del GM65)
    Sensor IR de marca (piso) : GPIO47 (salida digital DO del módulo)

  ALIMENTACIÓN DEL QTR-8A (IMPORTANTE):
    Aliméntalo SOLO a 3.3V. Sus salidas analógicas llegan hasta el
    voltaje con que lo alimentes: a 5V pueden mandar hasta 5V a los
    pines del ESP32-S3 y dañarlos (no son tolerantes a 5V).

  --------------------------------------------------------------------
  VERIFICACIÓN DE PINES AL ARRANCAR
    Antes de mover un solo motor, el firmware:
      1) Revisa que ningún pin esté usado dos veces (duplicado).
      2) Confirma que el PWM (LEDC) se pudo configurar correctamente
         en los 4 pines de los drivers BTS7960.
    Si algo falla, el robot NO se mueve (motores bloqueados), el LED
    parpadea en magenta, y el detalle se imprime por Serial y se
    muestra también en el HMI web.

  --------------------------------------------------------------------
  CONTROL EN CASCADA (línea + velocidad por encoder)
    Hay DOS lazos PID, uno dentro del otro:
      1) PID de LÍNEA (externo, con el QTR-8A): a partir del error de
         posición decide cuánta velocidad debe llevar cada rueda. Su
         salida es una "velocidad objetivo" en pulsos de encoder por
         intervalo de control (CONTROL_INTERVAL_MS), NO es PWM.
      2) PID de VELOCIDAD (interno, uno por rueda, con su encoder):
         compara la velocidad objetivo contra los pulsos que contó el
         encoder y ajusta el PWM real.

  --------------------------------------------------------------------
  LED DE ESTADO (LED direccionable, RGB_LED_PIN)
    - Magenta parpadeando  -> ERROR de pines/PWM, robot bloqueado
    - Amarillo parpadeando -> calibrando sensores
    - Verde fijo           -> siguiendo la línea correctamente
    - Rojo parpadeando     -> línea perdida, recuperando
    - Azul fijo            -> detenido

  --------------------------------------------------------------------
  HMI WEB — CON PORTAL CAUTIVO
    El ESP32-S3 crea su propia red WiFi (modo Access Point):
      SSID: valor de AP_SSID      Password: valor de AP_PASS
    Al conectarte, el celular/PC debería abrir el panel solo. Si no,
    entra manualmente a http://192.168.4.1/
    Desde ahí puedes: ver en qué estación está el AGV según el último
    QR leído (mapa HOME y E1..E4), el historial de lecturas, el estado
    del sensor IR y de la zona de estación, los bytes recibidos del
    lector, las 8 barras del QTR-8A, recalibrar, iniciar/detener el
    seguimiento, ajustar los dos PID en vivo, ver posición/error/PWM/
    pulsos, y mover la rampa (compuerta y elevación por separado, o la
    secuencia completa con "Descargar").
    La página es autónoma (sin internet, fuentes ni librerías externas) y se
    adapta: una columna en el celular, dos columnas en pantallas anchas (>=900 px).

  --------------------------------------------------------------------
  LECTOR QR GM65 (UART1, GPIO39 RX / GPIO40 TX)
    Conexión:  GM65 VCC -> 5V (consume ~100-200 mA, mejor de la fuente
               de 5V que de un regulador pequeño), GM65 GND -> GND,
               GM65 TX -> GPIO39, GM65 RX -> GPIO40.
    NIVEL LÓGICO: mide con multímetro el voltaje de la salida TX del
               GM65 antes de conectarla. Los pines del ESP32-S3 NO son
               tolerantes a 5V; si sale a 5V usa un divisor de voltaje.
    Modo:      el lector debe estar en modo SERIAL (UART/TTL), no en
               USB teclado. Se cambia escaneando el código de
               configuración del manual. Parámetros esperados: 9600 8N1
               y fin de lectura con CR (si cambias algo, ajusta QR_BAUD
               o usa el comando z).
    Códigos:   QR-HOME y QR-E1 ... QR-E4 (los mismos que usa el HMI).
    HMI:       la telemetría incluye "qr" (último código), "qrHaceMs"
               y "qrBytes" (bytes recibidos del lector desde el arranque).

  --------------------------------------------------------------------
  SENSOR IR DE MARCA + ZONA DE ESTACIÓN (GPIO47)
    Un sensor infrarrojo mirando al piso detecta una marca NEGRA junto
    a cada base (estación / home). Al verla el robot sabe que el QR
    está cerca y se comporta así (solo mientras sigue la línea):
      1) Zona lenta: baja baseSpeed y maxSpeed al ZONA_LENTA_FACTOR
         (por defecto 40 %) para poder leer el QR sin perderlo.
      2) Al leer un QR válido (no hace falta estar en zona) se DETIENE el modo
         seguimiento de línea (running = false, motores a 0):
           QR-E1..QR-E4: corre la secuencia de la rampa (sube, abre,
                         descarga, cierra, baja) y queda DETENIDO
                         (pulsa Iniciar para que siga). Con
                         QR_REANUDAR_TRAS_DESCARGA = true retoma la
                         línea solo al terminar. Con
                         QR_DESCARGA_AUTO = false solo se detiene.
           QR-HOME     : se detiene en la base y queda detenido
                         (pulsa Iniciar para que salga).
      3) Si pasa la marca sin leer QR, vuelve a velocidad normal tras
         ZONA_MANTENER_MS.
      4) Tras detenerse, no vuelve a disparar hasta salir de la marca
         (IR sin negro durante IR_REARME_MS); así no se repite al
         reanudar ni al arrancar parado sobre una marca.
    La marca negra SOLO baja la velocidad. Con QR_REQUIERE_ZONA = false
    (por defecto) el QR detiene al robot EN CUANTO se lee, aunque el IR
    no haya marcado zona; con true, un QR leído fuera de la zona se ignora.

  --------------------------------------------------------------------
  ENVIAR A UNA ESTACIÓN (tarjeta "Enviar a estación" del HMI, o k0..k4
  por Serial: k0 = HOME, k1..k4 = E1..E4)
    El AGV arranca (con la rampa cerrada), sigue la línea, hace una
    pausa breve (QR_PAUSA_PASO_MS) en los QR de las demás estaciones y
    sigue (en QR-HOME SIEMPRE se detiene) y se detiene en cuanto lee el
    QR de la elegida; en E1..E4 corre la descarga. Con "Iniciar" a secas (sin
    destino) se detiene en el primer QR que lea. Detener cancela el
    destino. Mientras la rampa descarga no acepta un destino nuevo.
    INSTALACIÓN: la línea que sigue el QTR-8A también es negra, así que
    monta el sensor IR DESPLAZADO A UN LADO de la línea, de modo que
    solo vea las marcas negras junto a las bases y nunca la línea.
    Alimenta el módulo IR a 3.3V para que su salida no pase de 3.3V.
    Si el módulo da HIGH sobre negro deja IR_NEGRO_ES_ALTO = true; si da
    LOW sobre negro, ponlo en false. Ajusta la sensibilidad con el
    potenciómetro del módulo (comando 'q' muestra qué ve el IR).

  --------------------------------------------------------------------
  LIBRERÍAS REQUERIDAS (Arduino IDE -> Administrador de bibliotecas):
    - QTRSensors (Pololu)           -> lectura/calibración del QTR-8A
    - Adafruit NeoPixel             -> LED direccionable de estado
    - ESPAsyncWebServer + AsyncTCP  -> servidor web + WebSocket del HMI
    - ArduinoJson                   -> mensajes del HMI
    (los 2 servos de la rampa se manejan con LEDC directo: ya no se usa ESP32Servo)

  --------------------------------------------------------------------
  COMANDOS POR SERIAL (115200 baudios), con ENTER:
      p2.5 / i0.01 / d15   -> Kp / Ki / Kd del PID de línea
      b40 / m90            -> velocidad base / máxima objetivo (pulsos/intervalo)
      P1.5 / I0.05 / D0.05 -> Kp / Ki / Kd del PID de velocidad
      c         -> recalibra el QTR-8A (+ diagnóstico)
      s         -> inicia/detiene el seguimiento
      e         -> imprime pulsos de encoder (debug)
      a / x     -> abre / cierra la COMPUERTA a mano (con el robot detenido)
      u / j     -> sube / baja la ELEVACIÓN a mano (con el robot detenido)
      g         -> corre la secuencia completa de descarga (robot detenido)
      q         -> muestra el último QR leído, los bytes recibidos del lector
                   y el estado del sensor IR / zona
      w         -> activa/desactiva el volcado HEX de lo que manda el lector
      t         -> manda "QR-E1" por UART1 (prueba con puente GPIO39-GPIO40)
      z9600     -> cambia el baudrate del lector (z115200, z38400, ...)
      l120 / r-120 -> PRUEBA directa del motor izquierdo / derecho (PWM -255..255,
                      1.5 s, sin PID ni encoders; con el robot detenido y las
                      ruedas en el aire). o = parar la prueba.
  ======================================================================
*/

#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <QTRSensors.h>
#include <Adafruit_NeoPixel.h>
#include <strings.h>   // strncasecmp() para comparar los códigos QR
#include <esp_system.h> // esp_reset_reason(): saber por qué se reinició la placa
#include <WebSocketsClient.h> // cliente WebSocket: el carro se conecta al servidor (rol agv)
#include <Preferences.h>      // guarda SSID y servidor en la memoria del ESP32
#include <WiFiUdp.h>          // escucha el aviso del servidor para encontrar su IP solo

// Declarado aquí arriba a propósito: el generador automático de
// prototipos de Arduino inserta los prototipos justo después de los
// #include, y si este enum se define más abajo, "LedState" no existe
// todavía para esos prototipos y la compilación falla.
enum LedState { LED_ERROR, LED_CALIBRANDO, LED_SIGUIENDO, LED_LINEA_PERDIDA, LED_DETENIDO };

// ======================================================================
// WiFi del HMI (Access Point propio del robot)
// ======================================================================
const char* AP_SSID = "SeguidorLineaPID";
const char* AP_PASS = "linea1234";   // WPA2 requiere 8+ caracteres

// ======================================================================
// >>> TU RED WiFi: la MISMA red (2.4 GHz) donde está la laptop de la rueda <<<
//     Escribe aquí el nombre y la contraseña ANTES de subir el programa.
//     (También se pueden cambiar después en el panel: Mantenimiento > Red y servidor.)
// ======================================================================
const char* WIFI_RED   = "";        // ej. "MiCelular"
const char* WIFI_CLAVE = "";        // ej. "12345678"
const uint16_t PUERTO_BUSQUEDA = 3333;   // aviso del servidor por la red (no lo cambies)
// Servidor en la NUBE (link fijo). Si lo escribes aquí, el carro se conecta siempre a él.
// ej. "celda-llenado.onrender.com"  (sin https:// y sin / al final). Vacío = usa la laptop de la red.
const char* SERVIDOR_NUBE = "celda-llenado.onrender.com";
// SALA a la que entra el carro (la misma que va en el link: ...AGV_Carro_v17.html?sala=nombre).
// Minúsculas, sin espacios ni acentos. Vacío = sala "principal".
// En la nube el carro PIDE PERMISO: lo apruebas en la página del carro de esa sala.
const char* SALA         = "";      // ej. "equipo2"
const char* NOMBRE_CARRO = "Carro ESP32";   // así aparece en la solicitud de permiso
String permisoCarro = "";            // "", "esperando", "aprobado", "rechazado", "fuera"

// ======================================================================
// PINES - ESP32-S3 + QTR-8A ANALOGICO
// ======================================================================
// El QTR-8A entrega señales ANALOGICAS: S1..S8 van a GPIO1..GPIO8, que
// son canales ADC1 del ESP32-S3 (ADC1 sí se puede usar con WiFi activo).
//
// Notas de arranque del ESP32-S3:
//  - GPIO3 es pin de "strapping" (selección de fuente JTAG) y aquí lo
//    usa la salida S3 del QTR. Como entrada analógica el riesgo es
//    bajo, pero si algún día el ESP32 no arranca con el sensor
//    conectado, desconecta S3 y prueba.
//  - GPIO0, 45 y 46 (strapping) quedan libres.
//  - GPIO19/20 (USB nativo) no se usan.
//  - GPIO26-37 quedan libres (flash/PSRAM interna en módulos con PSRAM).

// QTR-8A ANALOGICO: S1 ... S8 (izquierda a derecha)
const uint8_t QTR8_PINS[8] = {1, 2, 3, 4, 5, 6, 7, 8};
#define QTR8_COUNT 8

// Drivers BTS7960
#define L_RPWM   11
#define L_LPWM   12
#define L_REN    13
#define L_LEN    14
#define R_RPWM   15
#define R_LPWM   16
#define R_REN    17
#define R_LEN    18

// Encoders de cuadratura
#define L_ENC_A  9
#define L_ENC_B  10
#define R_ENC_A  21
#define R_ENC_B  38

// Si al probar el robot una rueda cuenta el sentido invertido,
// cambia true/false aquí.
#define L_ENC_INVERT false
#define R_ENC_INVERT false

#define RGB_LED_PIN 48   // LED direccionable de estado

// Lector QR GM65 por UART1
#define QR_RX_PIN 40     // el ESP32 RECIBE aquí  <- TX del GM65
#define QR_TX_PIN 39     // el ESP32 ENVÍA aquí   -> RX del GM65

// Sensor infrarrojo mirando al piso (detecta la marca negra de la base)
#define IR_PIN 47        // salida digital (DO) del módulo IR

// RAMPA = DOS servos independientes:
//   - COMPUERTA: abre/cierra para dejar caer el producto.
//   - ELEVACIÓN: sube/baja el mecanismo de la rampa.
// Ambos servos se alimentan con 5V EXTERNO (no del pin 5V/3V3 del
// ESP32), con el GND unido al GND del ESP32.
#define SERVO_COMPUERTA_PIN 41
#define SERVO_ELEVACION_PIN 42

// ======================================================================
// PWM de motores (ESP32 LEDC)
// ======================================================================
const int PWM_FREQ = 20000;
const int PWM_RES  = 8;         // 0-255
const int PWM_MAX  = 255;

// ======================================================================
// Parámetros PID de LÍNEA (lazo externo, salida = velocidad objetivo
// en pulsos de encoder por intervalo de control, no PWM directo).
// En automático las velocidades objetivo se limitan a 0..maxSpeed.
// ======================================================================
float Kp = 2.5;
float Ki = 0.0;
float Kd = 18.0;

// OJO: estos NO son PWM (0-255) sino "pulsos de encoder por intervalo
// de CONTROL_INTERVAL_MS". Empieza bajo (p.ej. 20-40) y ve subiendo
// mientras observas (comando 'e' o el HMI) cuántos pulsos da tu
// encoder a velocidad de crucero real.
int baseSpeed = 40;    // velocidad objetivo "de crucero" en línea recta
int maxSpeed  = 90;    // techo de velocidad objetivo para cada rueda

const unsigned long CALIBRATION_TIME_MS = 5000;
const int LINE_CENTER = (QTR8_COUNT - 1) * 1000 / 2; // 3500 para 8 sensores

// ======================================================================
// Parámetros PID de VELOCIDAD (lazo interno, uno por rueda, salida =
// PWM real que se manda al BTS7960)
// ======================================================================
float KpVel = 1.5;
float KiVel = 0.05;
float KdVel = 0.05;

const unsigned long CONTROL_INTERVAL_MS   = 20;  // periodo de ambos PID
const unsigned long BROADCAST_INTERVAL_MS = 150; // periodo de telemetría al HMI

// ======================================================================
// Mantenimiento / chequeo del sensor QTR-8A
// ======================================================================
// Si (max-min) de calibración de un sensor es menor a esto, se marca
// como "REVISAR" (sensor sucio, desalineado o dañado). El valor crudo
// del ADC del ESP32-S3 va de 0 a 4095, así que 300 es una exigencia
// baja; súbelo si quieres un chequeo más estricto.
const uint16_t SENSOR_RANGO_MIN = 300;

bool     sensorOk[QTR8_COUNT];
uint16_t calibRango[QTR8_COUNT];

// ======================================================================
// Objetos y variables globales
// ======================================================================
QTRSensors qtr;
uint16_t sensorValues[QTR8_COUNT];

float lastError = 0;
float integral  = 0;
int   lastSideSeen = 1; // 1 = línea vista a la derecha, -1 = a la izquierda

// Estado del PID de velocidad, uno por rueda
float integralVelL = 0, lastErrorVelL = 0;
float integralVelR = 0, lastErrorVelR = 0;

bool running  = false; // arranca detenido; se inicia desde HMI/Serial
bool pinesOk  = true;  // false si verificarPines()/setupMotors() detectan un error
bool pausaPorQr = false; // true mientras el robot está detenido en una estación descargando

// ---- CARRO DE ENTREGA: estado del lote y puente con el servidor ----
enum CarroEstado { CARRO_CARGA, CARRO_SALIENDO, CARRO_DESCARGANDO, CARRO_REGRESANDO };
CarroEstado carroEstado = CARRO_CARGA;
bool    carroPausado    = false;   // detenido con lote en curso (conserva el destino)
int     carroVelPct     = 60;      // 20..100 %: velocidad durante el lote (la pone la UI)
int     carroModoDest   = 0;       // 0 = sin elegir (el carro NO sale), 1..3 = almacén elegido para el lote
bool    carroEsperaDest = false;   // lote cargado esperando que se elija el almacén
String  carroIdPend     = "null";  // id del lote que espera destino
int     carroSigRot     = 1;       // siguiente almacén en modo rotativo
int     carroDestino    = 0;       // almacén del lote en curso (1..3), 0 = ninguno
long    carroLoteN      = 0;       // contador de lotes
String  carroLoteIdJson = "null";  // id del lote tal como lo mandó el HMI (JSON)
int     carroVasos[6]   = {0, 0, 0, 0, 0, 0}; // colores de los vasos en el carro (1..3)
int     carroVasosN     = 0;
long    carroEntregas[3] = {0, 0, 0};
long    carroPiso       = 0;       // vasos aprobados que cayeron al piso
float   carroS          = 0;       // posición estimada en el dibujo (px)
bool    carroSim        = false;   // MODO SIMULADOR: el viaje se simula (motores apagados)
unsigned long simPausaHasta = 0;   // simulador: pausa breve en un QR de paso

// ---- Red y servidor ----
WebSocketsClient wsServidor;
Preferences prefs;
String   redSsid, redPass, srvHost;
uint16_t srvPort = 3000;
bool     srvIniciado = false, srvOk = false, hmiEnLinea = false;
WiFiUDP  udpBusca;
bool     udpIniciado = false;
bool     srvAuto = false;           // la IP del servidor se encontró sola
unsigned long srvDesdeMs = 0;       // desde cuándo intenta conectarse a esa IP

// La calibración dura 5 s. Si se hiciera dentro del callback del
// WebSocket bloquearía la tarea de red (AsyncTCP) y puede reiniciar el
// ESP32 por watchdog. Por eso el HMI solo levanta esta bandera y la
// calibración se ejecuta desde loop().
volatile bool pedirCalibracion = false;

// Último estado calculado (para telemetría del HMI, y para poder
// leer sensores/encoders aunque el robot esté detenido)
uint16_t posicionActual     = 0;
float    errorActual        = 0;
bool     lineaVisibleActual = false;
int      pwmIzqActual = 0, pwmDerActual = 0;
long     pulsosIzqActual = 0, pulsosDerActual = 0;
long     encTotIzq = 0, encTotDer = 0;   // pulsos acumulados (para probar los encoders)
unsigned long pruebaMotorHasta = 0;      // != 0 mientras corre una prueba manual de motores

unsigned long ultimoControl   = 0;
unsigned long ultimoBroadcast = 0;

// ======================================================================
// Rampa: DOS servos (compuerta + elevación)
// ======================================================================
// Regla de seguridad: la rampa SIEMPRE queda en posición de viaje
// (compuerta cerrada, elevación abajo) al arrancar, al iniciar el
// seguimiento y al dar Detener.
//
// PWM DE MOTORES Y SERVOS (LEDC, sin librería ESP32Servo):
// Los motores usan los canales LEDC 0-3 (timers 0 y 1, 20 kHz) y los
// servos los canales 4 y 5 (timer 2, 50 Hz). Así NUNCA comparten canal
// ni timer, que era el riesgo de usar ESP32Servo junto con los motores
// (la librería elige canales por su cuenta y podía pisar el de un motor).
const int RAMPA_CANAL_IZQ_RPWM = 0;
const int RAMPA_CANAL_IZQ_LPWM = 1;
const int RAMPA_CANAL_DER_RPWM = 2;
const int RAMPA_CANAL_DER_LPWM = 3;

const int COMPUERTA_CERRADA_GRADOS = 0;    // posicion de viaje/reposo
const int COMPUERTA_ABIERTA_GRADOS = 90;   // posicion de descarga (ajusta a tu mecanismo)
const int ELEVACION_ABAJO_GRADOS   = 0;    // posicion de viaje/reposo
const int ELEVACION_ARRIBA_GRADOS  = 90;   // posicion de descarga (ajusta a tu mecanismo)
const int SERVO_US_MIN = 500;              // ancho de pulso minimo (us)
const int SERVO_US_MAX = 2400;             // ancho de pulso maximo (us)

// Tiempos de cada fase de la secuencia automática (no bloqueante)
const unsigned long ELEVACION_TIEMPO_MS       = 1000; // lo que tarda en subir/bajar (90° a 120 °/s ≈ 750 ms)
const unsigned long ESPERA_ANTES_COMPUERTA_MS = 300;  // pausa tras subir, antes de abrir
const unsigned long RAMPA_DESCARGA_MS         = 3000; // compuerta abierta, cae el producto

const float SERVO_VEL_GRADOS_S = 120.0f;   // velocidad máxima de los servos (grados/s); bájala si el lector QR aún "pita"
const int SERVO_FREQ = 50;                 // Hz
const int SERVO_RES  = 14;                 // bits (0-16383) -> buena resolución a 50 Hz
const int SERVO_CANAL_COMPUERTA = 4;       // canales 4-5 = timer 2 (motores: 0-3)
const int SERVO_CANAL_ELEVACION = 5;

void writePwm(int pin, int channelLegacy, int value);   // definida más abajo

struct ServoLedc {
  int  pin = -1;
  int  canal = -1;
  bool ok = false;
  bool attach(int p, int c) {
    pin = p; canal = c;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ok = ledcAttachChannel(pin, SERVO_FREQ, SERVO_RES, canal);
#else
    ledcSetup(canal, SERVO_FREQ, SERVO_RES);
    ledcAttachPin(pin, canal);
    ok = true;
#endif
    return ok;
  }
  bool attached() const { return ok; }
  // Movimiento SUAVE: write() solo fija el objetivo y actualizar() (llamado en
  // cada vuelta del loop) acerca el servo a SERVO_VEL_GRADOS_S. Así no hay el
  // pico de corriente de un salto brusco, que bajaba el 5 V y reiniciaba el
  // lector QR (el "pitido").
  float         actual    = 0;
  int           objetivo  = 0;
  bool          iniciado  = false;
  unsigned long ultimoMs  = 0;

  void escribirDirecto(float grados) {
    if (!ok) return;
    grados = constrain(grados, 0.0f, 180.0f);
    uint32_t us   = SERVO_US_MIN + (uint32_t)((SERVO_US_MAX - SERVO_US_MIN) * grados / 180.0f);
    uint32_t duty = (uint32_t)(((uint64_t)us * ((1UL << SERVO_RES) - 1)) / 20000UL); // periodo = 20000 us
    writePwm(pin, canal, duty);
  }
  void write(int grados) {
    if (!ok) return;
    objetivo = constrain(grados, 0, 180);
    if (!iniciado) {            // primera orden (arranque): va directo
      iniciado = true;
      actual = objetivo;
      ultimoMs = millis();
      escribirDirecto(actual);
    }
  }
  void actualizar() {
    if (!ok || !iniciado) return;
    unsigned long ahora = millis();
    unsigned long dt = ahora - ultimoMs;
    if (dt == 0) return;
    ultimoMs = ahora;
    if ((int)actual == objetivo && fabsf(actual - objetivo) < 0.5f) return;
    float paso = SERVO_VEL_GRADOS_S * dt / 1000.0f;
    if (actual < objetivo)      actual = min(actual + paso, (float)objetivo);
    else if (actual > objetivo) actual = max(actual - paso, (float)objetivo);
    escribirDirecto(actual);
  }
};

ServoLedc servoCompuerta, servoElevacion;
bool  servoCompuertaOk = false;
bool  servoElevacionOk = false;

enum RampaFase {
  RAMPA_IDLE,                  // en reposo (compuerta cerrada, elevación abajo)
  RAMPA_SUBIENDO,               // subiendo la elevación
  RAMPA_ESPERANDO_COMPUERTA,    // pausa corta antes de abrir la compuerta
  RAMPA_DESCARGANDO,            // compuerta abierta, cayendo el producto
  RAMPA_CERRANDO,               // cerrando la compuerta
  RAMPA_BAJANDO                 // bajando la elevación, de regreso a reposo
};
RampaFase rampaFase = RAMPA_IDLE;
unsigned long rampaFaseInicioMs = 0;

const char* nombreFaseRampa(RampaFase f) {
  switch (f) {
    case RAMPA_SUBIENDO:            return "SUBIENDO_ELEVACION";
    case RAMPA_ESPERANDO_COMPUERTA: return "ESPERANDO";
    case RAMPA_DESCARGANDO:         return "DESCARGANDO";
    case RAMPA_CERRANDO:            return "CERRANDO_COMPUERTA";
    case RAMPA_BAJANDO:             return "BAJANDO_ELEVACION";
    default:                        return "REPOSO";
  }
}

// Controles manuales de cada servo por separado (útiles para probar o
// calibrar el mecanismo). Subir/abrir solo tienen efecto si no hay una
// secuencia automática en curso.
void abrirCompuertaManual() {
  if (rampaFase != RAMPA_IDLE) return;
  if (servoCompuertaOk) servoCompuerta.write(COMPUERTA_ABIERTA_GRADOS);
}
void cerrarCompuertaManual() {
  if (servoCompuertaOk) servoCompuerta.write(COMPUERTA_CERRADA_GRADOS);
}
void subirElevacionManual() {
  if (rampaFase != RAMPA_IDLE) return;
  if (servoElevacionOk) servoElevacion.write(ELEVACION_ARRIBA_GRADOS);
}
void bajarElevacionManual() {
  if (rampaFase != RAMPA_IDLE) return;   // no pisar la secuencia de descarga en curso
  if (servoElevacionOk) servoElevacion.write(ELEVACION_ABAJO_GRADOS);
}

// Regresa ambos servos a la posición de viaje y cancela cualquier
// secuencia en curso. Se llama por seguridad al arrancar, al dar
// Iniciar/Detener, etc.
void cerrarRampa() {
  rampaFase = RAMPA_IDLE;
  pausaPorQr = false;
  if (servoCompuertaOk) servoCompuerta.write(COMPUERTA_CERRADA_GRADOS);
  if (servoElevacionOk) servoElevacion.write(ELEVACION_ABAJO_GRADOS);
}

// Secuencia de descarga: sube, pausa, abre, descarga, cierra, baja.
// Todo no bloqueante (ver actualizarRampa()).
void iniciarSecuenciaRampa() {
  rampaFase = RAMPA_SUBIENDO;
  rampaFaseInicioMs = millis();
  if (servoElevacionOk) servoElevacion.write(ELEVACION_ARRIBA_GRADOS);
  Serial.println(F("Rampa: subiendo elevación..."));
}

// Dispara la secuencia completa a mano (botón "Descargar" del HMI o
// comando 'g'). Solo con el robot detenido y la rampa en reposo.
void iniciarDescargaManual() {
  if (running) {
    Serial.println(F("Detén el seguimiento (s) antes de descargar."));
    return;
  }
  if (rampaFase != RAMPA_IDLE) return;
  iniciarSecuenciaRampa();
}

// Se llama en cada vuelta del loop (no bloqueante). Avanza la fase de
// la secuencia según el tiempo transcurrido.
void actualizarRampa() {
  if (rampaFase == RAMPA_IDLE) return;
  unsigned long transcurrido = millis() - rampaFaseInicioMs;

  switch (rampaFase) {
    case RAMPA_SUBIENDO:
      if (transcurrido >= ELEVACION_TIEMPO_MS) {
        rampaFase = RAMPA_ESPERANDO_COMPUERTA;
        rampaFaseInicioMs = millis();
      }
      break;
    case RAMPA_ESPERANDO_COMPUERTA:
      if (transcurrido >= ESPERA_ANTES_COMPUERTA_MS) {
        if (servoCompuertaOk) servoCompuerta.write(COMPUERTA_ABIERTA_GRADOS);
        Serial.println(F("Rampa: compuerta abierta, descargando..."));
        rampaFase = RAMPA_DESCARGANDO;
        rampaFaseInicioMs = millis();
      }
      break;
    case RAMPA_DESCARGANDO:
      if (transcurrido >= RAMPA_DESCARGA_MS) {
        if (servoCompuertaOk) servoCompuerta.write(COMPUERTA_CERRADA_GRADOS);
        rampaFase = RAMPA_CERRANDO;
        rampaFaseInicioMs = millis();
      }
      break;
    case RAMPA_CERRANDO:
      // da un instante para que la compuerta termine de cerrar antes
      // de empezar a bajar la elevación
      if (transcurrido >= 900) {   // la compuerta ya se mueve suave (120 °/s): dale tiempo de cerrar
        if (servoElevacionOk) servoElevacion.write(ELEVACION_ABAJO_GRADOS);
        Serial.println(F("Rampa: bajando elevación..."));
        rampaFase = RAMPA_BAJANDO;
        rampaFaseInicioMs = millis();
      }
      break;
    case RAMPA_BAJANDO:
      if (transcurrido >= ELEVACION_TIEMPO_MS) {
        rampaFase = RAMPA_IDLE;
        Serial.println(F("Rampa: ciclo de descarga completo."));
        if (carroEstado == CARRO_DESCARGANDO) carroTrasDescarga();
      }
      break;
    default:
      break;
  }
}

void setupRampa() {
  // Servos en LEDC directo, canales 4 y 5 (los motores ya usan 0-3).
  servoCompuerta.attach(SERVO_COMPUERTA_PIN, SERVO_CANAL_COMPUERTA);
  servoCompuertaOk = servoCompuerta.attached();
  Serial.printf("Servo de compuerta: %s en GPIO%d (canal LEDC %d).\n",
                servoCompuertaOk ? "OK" : "ERROR", SERVO_COMPUERTA_PIN, SERVO_CANAL_COMPUERTA);

  servoElevacion.attach(SERVO_ELEVACION_PIN, SERVO_CANAL_ELEVACION);
  servoElevacionOk = servoElevacion.attached();
  Serial.printf("Servo de elevación: %s en GPIO%d (canal LEDC %d).\n",
                servoElevacionOk ? "OK" : "ERROR", SERVO_ELEVACION_PIN, SERVO_CANAL_ELEVACION);

  cerrarRampa(); // siempre arranca en posición de viaje
}

// ======================================================================
// Encoders de cuadratura (uno por motor)
// ======================================================================
volatile long encoderCountLeft  = 0;
volatile long encoderCountRight = 0;

// Los encoders se cuentan con A en RISING y B para determinar dirección.
// El QTR-8A no usa interrupciones: se lee por ADC.

void IRAM_ATTR isrEncoderLeft() {
  bool b = digitalRead(L_ENC_B);
  bool adelante = L_ENC_INVERT ? !b : b;
  if (adelante) encoderCountLeft++; else encoderCountLeft--;
}

void IRAM_ATTR isrEncoderRight() {
  bool b = digitalRead(R_ENC_B);
  bool adelante = R_ENC_INVERT ? !b : b;
  if (adelante) encoderCountRight++; else encoderCountRight--;
}

void setupEncoders() {
  pinMode(L_ENC_A, INPUT_PULLUP);
  pinMode(L_ENC_B, INPUT_PULLUP);
  pinMode(R_ENC_A, INPUT_PULLUP);
  pinMode(R_ENC_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(L_ENC_A), isrEncoderLeft, RISING);
  attachInterrupt(digitalPinToInterrupt(R_ENC_A), isrEncoderRight, RISING);
}

// ======================================================================
// Verificación de pines (duplicados) al arrancar
// ======================================================================
bool verificarPines() {
  const int pines[] = {
    L_RPWM, L_LPWM, L_REN, L_LEN, R_RPWM, R_LPWM, R_REN, R_LEN,
    L_ENC_A, L_ENC_B, R_ENC_A, R_ENC_B,
    QTR8_PINS[0], QTR8_PINS[1], QTR8_PINS[2], QTR8_PINS[3],
    QTR8_PINS[4], QTR8_PINS[5], QTR8_PINS[6], QTR8_PINS[7],
    RGB_LED_PIN, SERVO_COMPUERTA_PIN, SERVO_ELEVACION_PIN,
    QR_RX_PIN, QR_TX_PIN, IR_PIN
  };
  const uint8_t n = sizeof(pines) / sizeof(pines[0]);
  bool ok = true;
  for (uint8_t i = 0; i < n; i++) {
    for (uint8_t j = i + 1; j < n; j++) {
      if (pines[i] == pines[j]) {
        Serial.printf("ERROR: el pin GPIO%d esta usado en dos funciones distintas.\n", pines[i]);
        ok = false;
      }
    }
  }
  if (ok) Serial.println(F("Verificacion de pines: sin duplicados. OK"));
  return ok;
}

// ======================================================================
// LED de estado (direccionable, 1 solo LED en RGB_LED_PIN)
// ======================================================================
Adafruit_NeoPixel led(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

LedState ledEstadoActual = LED_DETENIDO;
unsigned long ledUltimoCambio = 0;
bool ledEncendido = false;
const uint8_t LED_BRILLO = 60; // 0-255, bajo para no encandilar de cerca

void setColorLed(uint8_t r, uint8_t g, uint8_t b) {
  led.setPixelColor(0, led.Color(r, g, b));
  led.show();
}

// Actualiza el LED según el estado del robot. No bloqueante: se llama
// en cada vuelta del loop y decide con millis() cuándo parpadear.
void actualizarLed(LedState estado) {
  unsigned long ahora = millis();

  if (estado != ledEstadoActual) {
    ledEstadoActual = estado;
    ledUltimoCambio = ahora;
    ledEncendido = true; // fuerza que el primer cambio se vea de inmediato
  }

  switch (estado) {
    case LED_SIGUIENDO:
      setColorLed(0, LED_BRILLO, 0); // verde fijo
      break;

    case LED_DETENIDO:
      setColorLed(0, 0, LED_BRILLO); // azul fijo
      break;

    case LED_CALIBRANDO:
      if (ahora - ledUltimoCambio >= 400) {
        ledUltimoCambio = ahora;
        ledEncendido = !ledEncendido;
        setColorLed(ledEncendido ? LED_BRILLO : 0, ledEncendido ? LED_BRILLO : 0, 0); // amarillo parpadeante
      }
      break;

    case LED_LINEA_PERDIDA:
      if (ahora - ledUltimoCambio >= 120) {
        ledUltimoCambio = ahora;
        ledEncendido = !ledEncendido;
        setColorLed(ledEncendido ? LED_BRILLO : 0, 0, 0); // rojo parpadeante rápido
      }
      break;

    case LED_ERROR:
      if (ahora - ledUltimoCambio >= 150) {
        ledUltimoCambio = ahora;
        ledEncendido = !ledEncendido;
        setColorLed(ledEncendido ? LED_BRILLO : 0, 0, ledEncendido ? LED_BRILLO : 0); // magenta parpadeante
      }
      break;
  }
}

// ======================================================================
// Control de motores (BTS7960: RPWM = giro adelante, LPWM = giro atrás)
// ======================================================================
bool setupMotors() {
  pinMode(L_REN, OUTPUT);
  pinMode(L_LEN, OUTPUT);
  pinMode(R_REN, OUTPUT);
  pinMode(R_LEN, OUTPUT);
  digitalWrite(L_REN, HIGH);
  digitalWrite(L_LEN, HIGH);
  digitalWrite(R_REN, HIGH);
  digitalWrite(R_LEN, HIGH);

  bool ok = true;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  // API del core ESP32 v3.x. Se usa ledcAttachChannel() para fijar los
  // motores a los canales 0-3 y que los servos (timers 2 y 3, ver
  // setupRampa()) no puedan compartir timer con un motor.
  ok &= ledcAttachChannel(L_RPWM, PWM_FREQ, PWM_RES, RAMPA_CANAL_IZQ_RPWM);
  ok &= ledcAttachChannel(L_LPWM, PWM_FREQ, PWM_RES, RAMPA_CANAL_IZQ_LPWM);
  ok &= ledcAttachChannel(R_RPWM, PWM_FREQ, PWM_RES, RAMPA_CANAL_DER_RPWM);
  ok &= ledcAttachChannel(R_LPWM, PWM_FREQ, PWM_RES, RAMPA_CANAL_DER_LPWM);
  if (ok) Serial.println(F("PWM de motores (ledcAttachChannel): OK en los 4 pines, canales 0-3."));
  else    Serial.println(F("ERROR: no se pudo configurar el PWM (ledcAttachChannel) en uno o mas pines de motor."));
#else
  // API antigua por canales (core ESP32 v2.x). No reporta
  // éxito/fracaso en tiempo de ejecución.
  ledcSetup(0, PWM_FREQ, PWM_RES); ledcAttachPin(L_RPWM, 0);
  ledcSetup(1, PWM_FREQ, PWM_RES); ledcAttachPin(L_LPWM, 1);
  ledcSetup(2, PWM_FREQ, PWM_RES); ledcAttachPin(R_RPWM, 2);
  ledcSetup(3, PWM_FREQ, PWM_RES); ledcAttachPin(R_LPWM, 3);
  Serial.println(F("PWM de motores (ledcAttachPin, API v2.x): configurado."));
#endif
  return ok;
}

void writePwm(int pin, int channelLegacy, int value) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, value);
#else
  ledcWrite(channelLegacy, value);
#endif
}

// speed: -255..255  (positivo = adelante, negativo = reversa)
void setMotorLeft(int speed) {
  speed = constrain(speed, -PWM_MAX, PWM_MAX);
  if (speed >= 0) {
    writePwm(L_RPWM, 0, speed);
    writePwm(L_LPWM, 1, 0);
  } else {
    writePwm(L_RPWM, 0, 0);
    writePwm(L_LPWM, 1, -speed);
  }
}

void setMotorRight(int speed) {
  speed = constrain(speed, -PWM_MAX, PWM_MAX);
  if (speed >= 0) {
    writePwm(R_RPWM, 2, speed);
    writePwm(R_LPWM, 3, 0);
  } else {
    writePwm(R_RPWM, 2, 0);
    writePwm(R_LPWM, 3, -speed);
  }
}

void stopMotors() {
  setMotorLeft(0);
  setMotorRight(0);
}

// Prueba manual de motores: manda PWM directo (-255..255) a cada motor, SIN
// PID ni encoders, y se detiene sola a los 1.5 s. Sirve para saber si el
// problema está en el cableado/alimentación o en el control.
void iniciarPruebaMotores(int izq, int der) {
  if (running) { Serial.println(F("Detén el seguimiento antes de probar motores.")); return; }
  if (!pinesOk) { Serial.println(F("Motores bloqueados por error de pines/PWM.")); return; }
  izq = constrain(izq, -PWM_MAX, PWM_MAX);
  der = constrain(der, -PWM_MAX, PWM_MAX);
  encTotIzq = 0; encTotDer = 0;
  setMotorLeft(izq);
  setMotorRight(der);
  pwmIzqActual = izq; pwmDerActual = der;
  pruebaMotorHasta = millis() + 1500;
  if (pruebaMotorHasta == 0) pruebaMotorHasta = 1;
  Serial.printf("Prueba de motores: izq=%d der=%d durante 1.5 s\n", izq, der);
}

void detenerPruebaMotores() {
  if (pruebaMotorHasta == 0) return;
  pruebaMotorHasta = 0;
  stopMotors();
  pwmIzqActual = 0; pwmDerActual = 0;
  Serial.printf("Fin de la prueba. Pulsos acumulados: izq=%ld der=%ld\n", encTotIzq, encTotDer);
}

void actualizarPruebaMotores() {
  if (pruebaMotorHasta == 0) return;
  if (running) { pruebaMotorHasta = 0; return; }   // si arrancó el seguimiento, la prueba ya no aplica
  if ((long)(millis() - pruebaMotorHasta) >= 0) detenerPruebaMotores();
}

// ======================================================================
// Sensor infrarrojo de marca negra -> zona de estación
// ======================================================================
const bool          IR_NEGRO_ES_ALTO  = true;   // true: el módulo da HIGH sobre negro (si es al revés, false)
const uint8_t       IR_CONFIRMACIONES = 2;      // lecturas seguidas (cada CONTROL_INTERVAL_MS) para validar la marca
const unsigned long IR_REARME_MS      = 300;    // tiempo sin negro para volver a armar la detección
const unsigned long ZONA_MANTENER_MS  = 1500;   // si pasa la marca sin leer QR, velocidad normal tras este tiempo
const float         ZONA_LENTA_FACTOR = 0.4f;   // fracción de baseSpeed/maxSpeed dentro de la zona
const bool          QR_PARAR_CON_CUALQUIER_CODIGO = true; // true: CUALQUIER QR (aunque no sea HOME/E1..E4) detiene al robot y manda la señal; false: los códigos desconocidos se ignoran
const unsigned long QR_PAUSA_PASO_MS  = 1500;  // QR que NO es HOME ni el destino: se detiene este tiempo, manda la señal y sigue sola (0 = queda detenido/pausado)
const unsigned long QR_BLOQUEO_MS     = 3000;  // tras arrancar o pausar en un QR, ignora ESE mismo QR este tiempo (el lector aún lo ve)
const bool          QR_REQUIERE_ZONA  = false;  // false: el QR detiene al robot EN CUANTO se lee (la marca negra solo baja la velocidad); true: solo cuenta si el IR marcó la zona

bool          irNegro         = false;  // marca negra confirmada bajo el sensor
bool          zonaLenta       = false;  // dentro de la zona de estación (va lento)
bool          zonaArmada      = false;  // listo para detectar una marca nueva
uint8_t       irContNegro     = 0;
unsigned long irUltimoNegroMs = 0;

void setupIR() {
  pinMode(IR_PIN, INPUT);
}

// Se llama en cada paso de control (también con el robot detenido, para
// poder ver el estado en vivo).
void actualizarIR() {
  unsigned long ahora = millis();
  bool crudoNegro = (digitalRead(IR_PIN) == (IR_NEGRO_ES_ALTO ? HIGH : LOW));

  if (crudoNegro) {
    irUltimoNegroMs = ahora;
    if (irContNegro < 255) irContNegro++;
  } else {
    irContNegro = 0;
  }
  irNegro = (irContNegro >= IR_CONFIRMACIONES);

  if (irNegro && zonaArmada) zonaLenta = true;

  if (zonaLenta && (ahora - irUltimoNegroMs) > ZONA_MANTENER_MS) {
    zonaLenta = false;   // pasó la marca sin leer QR
    Serial.println(F("Zona de estacion: salio de la marca sin leer QR."));
  }

  // Se vuelve a armar cuando el IR lleva IR_REARME_MS sin ver negro
  if (!zonaLenta && !crudoNegro && (ahora - irUltimoNegroMs) >= IR_REARME_MS) {
    zonaArmada = true;
  }
}

// ======================================================================
// Lector QR GM65 (UART1)
// ======================================================================
// El GM65 manda cada lectura como texto terminado en CR (a veces CR+LF).
// La lectura NO bloquea: leerQR() se llama en cada vuelta del loop y solo
// consume los bytes que ya llegaron.
#define QR_SERIAL Serial1
unsigned long       qrBaudActual      = 9600;  // valor de fábrica típico (ver manual); cambia con el comando z
const unsigned long QR_REPEAT_MS      = 1500;  // ignora la misma lectura repetida
const unsigned long QR_IDLE_FLUSH_MS  = 60;    // si no llega CR, cierra la lectura tras este silencio
const bool          QR_DESCARGA_AUTO  = true;  // al leer QR-E# siguiendo línea: parar y descargar
const bool          QR_REANUDAR_TRAS_DESCARGA = false; // false: tras el QR el seguimiento queda DETENIDO (hay que pulsar Iniciar); true: retoma la línea solo tras descargar
const size_t        QR_MAX_LEN        = 48;

char          qrBuf[QR_MAX_LEN + 1];
size_t        qrLen           = 0;
bool          qrDescartando   = false;  // true si la lectura actual excedió QR_MAX_LEN
unsigned long qrUltimoByteMs  = 0;
char          qrUltimo[QR_MAX_LEN + 1] = "";
unsigned long qrUltimoMs      = 0;      // millis() de la última lectura
bool          qrHayLectura    = false;

// --- Diagnóstico del lector ---
unsigned long qrBytesRx       = 0;      // bytes recibidos del GM65 desde el arranque (0 = no manda nada)
unsigned long qrBytesIgnorados = 0;     // bytes no imprimibles descartados (muchos = baudrate equivocado)
bool          qrVolcado       = false;  // true: imprime en HEX todo lo que llega (comando w)

void setupQR() {
  QR_SERIAL.setRxBufferSize(256);       // debe ir ANTES de begin()
  QR_SERIAL.begin(qrBaudActual, SERIAL_8N1, QR_RX_PIN, QR_TX_PIN);
  Serial.printf("Lector QR GM65: UART1 a %lu baudios (RX=GPIO%d, TX=GPIO%d).\n",
                qrBaudActual, QR_RX_PIN, QR_TX_PIN);
}

// Cambia el baudrate del lector sin reiniciar (comando z).
void cambiarBaudQR(unsigned long baud) {
  if (baud < 1200 || baud > 921600) {
    Serial.println(F("Baudrate invalido (usa p.ej. z9600 o z115200)."));
    return;
  }
  QR_SERIAL.end();
  qrBaudActual = baud;
  QR_SERIAL.setRxBufferSize(256);
  QR_SERIAL.begin(qrBaudActual, SERIAL_8N1, QR_RX_PIN, QR_TX_PIN);
  qrLen = 0; qrDescartando = false;
  Serial.printf("Lector QR: baudrate cambiado a %lu.\n", qrBaudActual);
}

unsigned long pausaPasoHasta  = 0;      // != 0: pausa breve en un QR de paso (running sigue en true)
unsigned long qrBloqueoHasta  = 0;
char          qrBloqueoCodigo[QR_MAX_LEN + 1] = "";

// Qué hace el AGV al leer un código (solo con el robot siguiendo la línea):
//  - Con QR_REQUIERE_ZONA, el código solo cuenta si el sensor IR marcó la
//    zona de estación (un QR visto fuera de la base no lo detiene).
//  - QR-E1..QR-E4: se detiene; con QR_DESCARGA_AUTO corre la secuencia de
//    la rampa y actualizarControl() retoma la línea al terminar.
//  - QR-HOME: se detiene en la base y se queda detenido.
// Estación elegida desde el HMI (o con el comando k#):
//   -1 = sin destino: se detiene en el primer QR que lea
//    0 = HOME, 1..4 = E1..E4: solo se detiene en ESE QR; los demás los pasa de largo
int destinoEstacion = -1;
const char* NOMBRES_EST[5] = {"HOME", "E1", "E2", "E3", "E4"};

// QR-HOME -> 0, QR-E1..QR-E4 -> 1..4, cualquier otro -> -1
int estacionDeCodigo(const char* c) {
  if (strcasecmp(c, "QR-HOME") == 0) return 0;
  if (strncasecmp(c, "QR-E", 4) == 0 && c[4] >= '1' && c[4] <= '4' && c[5] == '\0') return c[4] - '0';
  return -1;
}

void alLeerQR(const char* codigo) {
  if (!running || !pinesOk) return;
  if (rampaFase != RAMPA_IDLE) return;

  if (millis() < qrBloqueoHasta && strcmp(codigo, qrBloqueoCodigo) == 0) return;   // el mismo QR que el lector aún ve
  if (pausaPasoHasta != 0) return;                                                // ya está en una pausa de paso

  int  idx        = estacionDeCodigo(codigo);   // 0 = HOME, 1..4 = E1..E4, -1 = QR desconocido
  bool esEstacion = (idx >= 1);

  if (idx < 0 && !QR_PARAR_CON_CUALQUIER_CODIGO) return;   // código ajeno al circuito: se ignora

  if (QR_REQUIERE_ZONA && !zonaLenta) {
    Serial.printf("QR %s ignorado: el sensor IR no marca zona de estacion.\n", codigo);
    return;
  }

  // 1) PARA EN CUANTO LEE CUALQUIER QR (sea el destino o no)
  zonaLenta  = false;
  zonaArmada = false;       // no vuelve a disparar hasta salir de la marca
  stopMotors();
  pwmIzqActual = 0; pwmDerActual = 0;
  resetPID();

  // 2) Manda la señal al panel y al servidor según cuál QR leyó
  qrSenal(codigo, idx);

  // 3) ¿Era el destino? Sin destino elegido, cualquier QR cuenta como "llegada".
  bool esDestino = (destinoEstacion < 0) || (idx == destinoEstacion);
  if (!esDestino) {
    if (idx != 0 && QR_PAUSA_PASO_MS > 0) {
      // QR de paso (ni HOME ni el destino): pausa breve, ya mandó la señal, y sigue sola
      pausaPasoHasta = millis() + QR_PAUSA_PASO_MS;
      qrBloqueoHasta = pausaPasoHasta + QR_BLOQUEO_MS;
      strncpy(qrBloqueoCodigo, codigo, QR_MAX_LEN);
      qrBloqueoCodigo[QR_MAX_LEN] = '\0';
      Serial.printf("QR %s: de paso (destino %s). Pausa %lu ms y sigue.\n",
                    codigo, NOMBRES_EST[destinoEstacion], QR_PAUSA_PASO_MS);
      enviarTelemetria();
      return;
    }
    // HOME (SIEMPRE se detiene) u otro QR con QR_PAUSA_PASO_MS = 0: queda detenido.
    // Con lote en curso queda PAUSADO conservando el destino: "Continuar" retoma la ruta.
    running    = false;
    pausaPorQr = false;
    if (carroEnViaje()) carroPausado = true;
    Serial.printf("QR %s: no es el destino (%s). Robot DETENIDO; pulsa Continuar/Iniciar para seguir.\n",
                  codigo, NOMBRES_EST[destinoEstacion]);
    enviarTelemetria();
    return;
  }

  if (carroEnViaje()) { carroAlLlegar(idx); return; }   // carro: llegó a su almacén o a HOME
  destinoEstacion = -1;     // llegó: se borra el destino

  if (esEstacion && QR_DESCARGA_AUTO && QR_REANUDAR_TRAS_DESCARGA) {
    // Variante opcional: descarga y retoma la línea sola (el modo seguimiento sigue activo)
    pausaPorQr = true;
    iniciarSecuenciaRampa();
    Serial.printf("QR %s: robot detenido, descargando (reanudara solo).\n", codigo);
    return;
  }

  // Por defecto: al leer el QR SE DETIENE el modo seguimiento de linea.
  running    = false;
  pausaPorQr = false;
  if (esEstacion && QR_DESCARGA_AUTO) {
    iniciarSecuenciaRampa();   // la rampa corre aunque el seguimiento ya este detenido
    Serial.printf("QR %s: seguimiento DETENIDO, descargando. Pulsa Iniciar para continuar.\n", codigo);
  } else {
    Serial.printf("QR %s: seguimiento DETENIDO. Pulsa Iniciar para continuar.\n", codigo);
  }
}

// Manda al AGV a una estación (0 = HOME, 1..4 = E1..E4): arranca el seguimiento
// de línea y se detiene al leer el QR de ese destino. Si ya va en marcha solo
// cambia el destino. No arranca a media descarga de la rampa.
void irAEstacion(int est) {
  if (est < 0 || est > 4) return;
  if (carroLoteActivo()) { Serial.println(F("Hay un lote en curso: pausa o espera.")); return; }
  if (!pinesOk) { Serial.println(F("Motores bloqueados por error de pines/PWM.")); return; }
  if (rampaFase != RAMPA_IDLE) { Serial.println(F("Espera a que termine la descarga de la rampa.")); return; }
  destinoEstacion = est;
  Serial.printf("Destino: %s\n", NOMBRES_EST[est]);
  if (!running) {
    detenerPruebaMotores();
    cerrarRampa();                       // siempre viaja con la rampa cerrada
    integral = 0; lastError = 0;
    integralVelL = 0; lastErrorVelL = 0;
    integralVelR = 0; lastErrorVelR = 0;
    running = true;
  }
}

void qrAceptar(const char* codigo) {
  unsigned long ahora = millis();
  bool repetido = qrHayLectura && strcmp(codigo, qrUltimo) == 0 &&
                  (ahora - qrUltimoMs) < QR_REPEAT_MS;
  qrUltimoMs = ahora;       // mientras el lector siga viendo el mismo QR, no se repite
  if (repetido) return;

  strncpy(qrUltimo, codigo, QR_MAX_LEN);
  qrUltimo[QR_MAX_LEN] = '\0';
  qrHayLectura = true;
  Serial.printf("QR leido: %s\n", qrUltimo);
  alLeerQR(qrUltimo);
}

void qrCerrarLectura() {
  if (qrDescartando) { qrLen = 0; qrDescartando = false; return; }
  qrBuf[qrLen] = '\0';
  // quita espacios al inicio y al final
  char* ini = qrBuf;
  while (*ini == ' ') ini++;
  size_t n = strlen(ini);
  while (n > 0 && ini[n - 1] == ' ') ini[--n] = '\0';
  if (n > 0) qrAceptar(ini);
  qrLen = 0;
}

void leerQR() {
  while (QR_SERIAL.available()) {
    int c = QR_SERIAL.read();
    qrUltimoByteMs = millis();
    qrBytesRx++;                                     // diagnóstico: llegó un byte del lector
    if (qrVolcado) Serial.printf("%02X ", c);        // diagnóstico: volcado hex (comando w)
    if (c == '\r' || c == '\n') {
      if (qrLen > 0 || qrDescartando) qrCerrarLectura();
      continue;
    }
    if (c < 32 || c > 126) { qrBytesIgnorados++; continue; }   // ignora bytes no imprimibles
    if (qrDescartando) continue;
    if (qrLen < QR_MAX_LEN) qrBuf[qrLen++] = (char)c;
    else { qrDescartando = true; qrLen = 0; }        // demasiado largo: se descarta
  }
  // Si el lector no manda CR, cierra la lectura tras un breve silencio
  if ((qrLen > 0 || qrDescartando) && (millis() - qrUltimoByteMs) > QR_IDLE_FLUSH_MS) {
    qrCerrarLectura();
  }
}

// ======================================================================
// PID de velocidad (lazo interno): recibe la velocidad objetivo del
// PID de línea y los pulsos reales que contó el encoder en el último
// CONTROL_INTERVAL_MS, y devuelve el PWM que hay que aplicar al motor.
// ======================================================================
int pidVelocidad(float objetivo, float medida, float &integralVel, float &lastErrorVel) {
  float error = objetivo - medida;

  integralVel += error;
  // Anti-windup: la parte integral puede aportar hasta el PWM máximo. Con un
  // tope fijo bajo (±500 y Ki=0.05 solo daba ±25 de PWM) el control se
  // quedaba por debajo del PWM mínimo que necesita el motor para arrancar y
  // nunca lo movía.
  float limInt = (KiVel > 0.0001f) ? ((float)PWM_MAX / KiVel) : 500.0f;
  integralVel = constrain(integralVel, -limInt, limInt);

  float derivada = error - lastErrorVel;
  lastErrorVel = error;

  float salida = KpVel * error + KiVel * integralVel + KdVel * derivada;

  // En el seguidor automático solo se necesita avance: la rueda puede
  // quedar en 0 PWM, pero no se manda reversa.
  return (int)constrain(salida, 0, PWM_MAX);
}

// ======================================================================
// Posición de la línea por PICO (ignora manchas de la pista)
// ======================================================================
// readLineBlack() promedia TODOS los sensores, así que una mancha oscura
// al lado de la línea jala la posición hacia ella. Aquí solo cuenta la
// zona más sensible: el sensor con la lectura más alta (calibrada 0-1000)
// y, como mucho, sus dos vecinos para dar resolución entre sensores.
const uint16_t LINEA_PICO_MIN   = 450;   // el pico debe pasar de aquí para ser línea (una mancha suele quedar abajo; bájalo si la línea real lee poco)
const float    LINEA_TOL_PICO   = 0.90f; // picos >= 90 % del máximo = "igual de fuertes": gana el más cercano a la posición anterior
const float    LINEA_FRAC_PISO  = 0.35f; // a cada lectura se le resta esta fracción del pico: lo débil casi no pesa
uint16_t       posicionPrevia   = (QTR8_COUNT - 1) * 1000 / 2;

// Lee el QTR-8A y deja sensorValues y posicionActual (0..7000) listos.
// Devuelve true si hay línea (pico >= LINEA_PICO_MIN).
bool leerLineaPico() {
  qtr.readCalibrated(sensorValues);

  uint16_t maximo = 0;
  for (uint8_t i = 0; i < QTR8_COUNT; i++) {
    if (sensorValues[i] > maximo) maximo = sensorValues[i];
  }

  if (maximo < LINEA_PICO_MIN) {                       // nada convincente: línea perdida
    posicionActual = (lastSideSeen > 0) ? (QTR8_COUNT - 1) * 1000 : 0;
    return false;
  }

  // Pico: máximo local casi tan fuerte como el máximo global; si hay varios,
  // el más cercano a donde estaba la línea (evita saltar a una mancha igual de oscura)
  uint16_t umbralPico = (uint16_t)(maximo * LINEA_TOL_PICO);
  int  pico = 0;
  long mejorDist = 0x7fffffff;
  for (int i = 0; i < QTR8_COUNT; i++) {
    uint16_t v = sensorValues[i];
    if (v < umbralPico) continue;
    if (i > 0 && sensorValues[i - 1] > v) continue;
    if (i < QTR8_COUNT - 1 && sensorValues[i + 1] > v) continue;
    long dist = labs((long)i * 1000 - (long)posicionPrevia);
    if (dist < mejorDist) { mejorDist = dist; pico = i; }
  }

  // Promedio ponderado SOLO con el pico y sus vecinos inmediatos
  float piso = sensorValues[pico] * LINEA_FRAC_PISO;
  float suma = 0, pesos = 0;
  for (int i = max(0, pico - 1); i <= min(QTR8_COUNT - 1, pico + 1); i++) {
    float w = sensorValues[i] - piso;
    if (w <= 0) continue;
    suma  += w * i * 1000.0f;
    pesos += w;
  }
  posicionActual = (uint16_t)(suma / pesos);
  posicionPrevia = posicionActual;
  return true;
}

// ======================================================================
// Calibración y diagnóstico/mantenimiento del QTR-8A analógico
// ======================================================================
void diagnosticoSensores() {
  Serial.println(F("--- Diagnostico QTR-8A analogico (mantenimiento) ---"));
  for (uint8_t i = 0; i < QTR8_COUNT; i++) {
    uint16_t mn = qtr.calibrationOn.minimum[i];
    uint16_t mx = qtr.calibrationOn.maximum[i];
    calibRango[i] = (mx > mn) ? (mx - mn) : 0;
    sensorOk[i] = calibRango[i] >= SENSOR_RANGO_MIN;
    Serial.printf("  S%u: min=%u max=%u rango=%u -> %s\n",
                  i, mn, mx, calibRango[i],
                  sensorOk[i] ? "OK" : "REVISAR (limpiar/alinear/revisar cable)");
  }
}

void calibrarSensores() {
  Serial.println(F("Calibrando... mueve el sensor sobre la linea"));
  running = false;
  stopMotors();
  unsigned long t0 = millis();
  while (millis() - t0 < CALIBRATION_TIME_MS) {
    qtr.calibrate();
    actualizarLed(LED_CALIBRANDO);
    leerQR();      // sigue vaciando el buffer del lector durante la calibración
    delay(2);
  }
  Serial.println(F("Calibracion terminada."));
  diagnosticoSensores();
}

// ======================================================================
// Lectura de comandos de ajuste por Serial
// ======================================================================
void procesarComandoSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;

  char tipo = cmd.charAt(0);
  float valor = cmd.substring(1).toFloat();

  switch (tipo) {
    case 'p': Kp = valor; Serial.printf("Kp = %.3f\n", Kp); break;
    case 'i': Ki = valor; Serial.printf("Ki = %.4f\n", Ki); break;
    case 'd': Kd = valor; Serial.printf("Kd = %.3f\n", Kd); break;
    case 'b': baseSpeed = (int)valor; Serial.printf("baseSpeed = %d\n", baseSpeed); break;
    case 'm': maxSpeed = (int)valor; Serial.printf("maxSpeed = %d\n", maxSpeed); break;
    case 'P': KpVel = valor; Serial.printf("KpVel = %.3f\n", KpVel); break;
    case 'I': KiVel = valor; Serial.printf("KiVel = %.4f\n", KiVel); break;
    case 'D': KdVel = valor; Serial.printf("KdVel = %.3f\n", KdVel); break;
    case 'v':   // valores crudos del QTR-8A (0-4095) y rango de calibracion
      for (uint8_t i = 0; i < QTR8_COUNT; i++) {
        Serial.printf("S%u: crudo=%d  calMin=%u calMax=%u\n", i + 1,
                      analogRead(QTR8_PINS[i]),
                      qtr.calibrationOn.minimum[i], qtr.calibrationOn.maximum[i]);
      }
      break;
    case 'e':
      Serial.printf("encoderLeft(ult. intervalo)=%ld  encoderRight(ult. intervalo)=%ld\n",
                    pulsosIzqActual, pulsosDerActual);
      break;
    case 'c':
      pedirCalibracion = true; // se ejecuta en loop()
      break;
    case 's':
      if (!running && rampaFase != RAMPA_IDLE) {   // no arrancar a media descarga (movería la elevación)
        Serial.println(F("Espera a que termine la descarga de la rampa."));
        break;
      }
      detenerPruebaMotores();
      running = !running;
      destinoEstacion = -1;   // inicio/paro manual: sin destino (se detiene en el primer QR)
      integral = 0; lastError = 0;
      integralVelL = 0; lastErrorVelL = 0;
      integralVelR = 0; lastErrorVelR = 0;
      stopMotors();
      cerrarRampa(); // seguridad: nunca viajar con la rampa abierta
      Serial.println(running ? F("RUNNING") : F("STOPPED"));
      break;
    case 'a':
      if (running) Serial.println(F("Detén el seguimiento (s) antes de mover la rampa a mano."));
      else abrirCompuertaManual();
      break;
    case 'x':
      cerrarCompuertaManual();
      break;
    case 'u':
      if (running) Serial.println(F("Detén el seguimiento (s) antes de mover la rampa a mano."));
      else subirElevacionManual();
      break;
    case 'j':
      bajarElevacionManual();
      break;
    case 'g':
      iniciarDescargaManual();
      break;
    case 'k': irAEstacion((int)valor); break;                // k0 = HOME, k1..k4 = estacion 1..4
    case 'l': iniciarPruebaMotores((int)valor, 0); break;   // l120 / l-120: motor izquierdo
    case 'r': iniciarPruebaMotores(0, (int)valor); break;   // r120 / r-120: motor derecho
    case 'o': detenerPruebaMotores(); break;                 // o: parar la prueba
    case 'q':
      if (qrHayLectura)
        Serial.printf("Ultimo QR: %s (hace %lu ms)\n", qrUltimo, millis() - qrUltimoMs);
      else
        Serial.println(F("Aun no se ha leido ningun QR."));
      Serial.printf("Lector QR: %lu bytes recibidos (%lu no imprimibles) a %lu baudios%s\n",
                    qrBytesRx, qrBytesIgnorados, qrBaudActual,
                    qrBytesRx == 0 ? " -> NO LLEGA NADA por UART (modo USB-HID? TX/RX? 5V?)" : "");
      Serial.printf("Sensor IR: %s | zona lenta: %s | armada: %s\n",
                    irNegro ? "NEGRO" : "blanco", zonaLenta ? "SI" : "no", zonaArmada ? "si" : "no");
      break;
    case 'w':   // volcado hexadecimal de lo que manda el lector
      qrVolcado = !qrVolcado;
      Serial.println(qrVolcado ? F("Volcado HEX del lector: ON (escanea un QR)") : F("\nVolcado HEX del lector: OFF"));
      break;
    case 't':   // prueba con puente GPIO39-GPIO40 (lector desconectado)
      QR_SERIAL.print("QR-E1\r");
      Serial.println(F("Enviado 'QR-E1' por UART1 (si hay puente GPIO39-GPIO40 debe aparecer 'QR leido: QR-E1')"));
      break;
    case 'z':   // z9600 / z115200: baudrate del lector
      cambiarBaudQR((unsigned long)valor);
      break;
    default:
      Serial.println(F("Comando no reconocido (usa p,i,d,b,m,P,I,D,c,s,e,v,a,x,u,j,g,q,w,t,z,k,l,r,o)"));
  }
}

// ======================================================================
// HMI web: página embebida (servida en memoria, sin LittleFS)
// ======================================================================
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dnsServer;
const byte DNS_PORT = 53;

#include "agv_ui.h"   // página del carro + mantenimiento (ver agv_ui.h)

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("HMI: cliente %u conectado\n", client->id());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("HMI: cliente %u desconectado\n", client->id());
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    // solo mensajes de texto completos (los comandos del HMI caben en una trama)
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      onWsMessage(data, len);
    }
  }
}

void setupWebServer() {
  WiFi.mode(WIFI_AP_STA);   // AP para el panel del carro + STA para el servidor
  bool apOk = WiFi.softAP(AP_SSID, AP_PASS);
  conectarRed();
  WiFi.setSleep(false);   // sin ahorro de energía: el AP responde mejor
  if (!apOk) Serial.println(F("ERROR: no se pudo crear la red WiFi (softAP)."));
  Serial.print(F("HMI web listo. Conectate a la red '"));
  Serial.print(AP_SSID);
  Serial.print(F("' y abre http://"));
  Serial.println(WiFi.softAPIP());

  // Portal cautivo: cualquier dominio que el teléfono/PC consulte se
  // resuelve a la IP del ESP32, así el sistema operativo detecta "hay
  // una red con portal" y abre el navegador solo.
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", INDEX_HTML);
  });

  // Cualquier otra URL (las que usan Android/iOS/Windows para detectar
  // el portal cautivo) recibe el mismo panel en vez de un 404.
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", INDEX_HTML);
  });

  server.begin();
}

// ======================================================================
// CARRO DE ENTREGA: lote, estados, eventos y puente con el servidor
// ======================================================================
// Estados (mismos nombres que usan la UI y el HMI):
//   CARGA        en la posición de carga (HOME = QR-HOME), esperando lote
//   SALIENDO     lote completo: sigue la línea hacia su almacén
//   DESCARGANDO  en el almacén: corre la rampa (sube, abre, descarga, cierra, baja)
//   REGRESANDO   vuelve a HOME siguiendo la línea
// Almacén n = estación QR-E n (n = 1..3). E4 queda para pruebas.

// Mismo trazo que el dibujo de la UI (medido sobre la ruta SVG del carro)
const float CARRO_L = 1754.5f;                          // largo del circuito (px del dibujo)
const float CARRO_S_ALM[3] = {483.6f, 837.3f, 1280.9f}; // posición de cada almacén en el dibujo

const char* nombreCarro() {
  switch (carroEstado) {
    case CARRO_SALIENDO:    return "SALIENDO";
    case CARRO_DESCARGANDO: return "DESCARGANDO";
    case CARRO_REGRESANDO:  return "REGRESANDO";
    default:                return carroEsperaDest ? "ESPERA_DESTINO" : "CARGA";
  }
}

bool carroLoteActivo() { return carroEstado != CARRO_CARGA; }
bool carroEnViaje()    { return carroEstado == CARRO_SALIENDO || carroEstado == CARRO_REGRESANDO; }

void resetPID() {
  integral = 0; lastError = 0;
  integralVelL = 0; lastErrorVelL = 0;
  integralVelR = 0; lastErrorVelR = 0;
}

// Durante un lote el carro usa la velocidad elegida en la UI (20..100 %)
int velFactor() { return carroLoteActivo() ? carroVelPct : 100; }

// "abc" = sensor izquierdo, centro y derecho ven línea (1) o no (0)
String lineaTexto() {
  bool i = sensorValues[0] > 450 || sensorValues[1] > 450 || sensorValues[2] > 450;
  bool c = sensorValues[3] > 450 || sensorValues[4] > 450;
  bool d = sensorValues[5] > 450 || sensorValues[6] > 450 || sensorValues[7] > 450;
  String s = "";
  s += i ? '1' : '0';
  s += c ? '1' : '0';
  s += d ? '1' : '0';
  return s;
}

// ---------- eventos hacia la UI y hacia el servidor (HMI y tienda) ----------
void carroEvento(const char* ev, int almacen) {
  StaticJsonDocument<320> d;
  d["tipo"] = "agv_evento";
  d["ev"] = ev;
  if (carroLoteActivo()) {
    JsonObject lo = d.createNestedObject("lote");
    lo["n"] = carroLoteN;
    lo["id"] = serialized(carroLoteIdJson.c_str());   // id tal cual lo mandó la tienda
    lo["destino"] = carroDestino;
  } else {
    d["lote"] = nullptr;
  }
  if (almacen > 0) d["almacen"] = almacen;
  else             d["almacen"] = nullptr;
  String out;
  serializeJson(d, out);
  ws.textAll(out);        // panel del carro (si está abierto)
  servidorEnviar(out);    // servidor -> HMI y tienda
  Serial.printf("AGV evento: %s (almacen %d)\n", ev, almacen);
}

// ---------- señal de QR leído (se manda con CUALQUIER código) ----------
// ev "qr" + "codigo" (texto leído) + "estacion": 0 = HOME, 1..4 = E1..E4,
// -1 = QR desconocido. "almacen" solo va con E1..E4 (igual que antes).
void qrSenal(const char* codigo, int idx) {
  StaticJsonDocument<384> d;
  d["tipo"]     = "agv_evento";
  d["ev"]       = "qr";
  d["codigo"]   = codigo;
  d["estacion"] = idx;
  if (carroLoteActivo()) {
    JsonObject lo = d.createNestedObject("lote");
    lo["n"] = carroLoteN;
    lo["id"] = serialized(carroLoteIdJson.c_str());
    lo["destino"] = carroDestino;
  } else {
    d["lote"] = nullptr;
  }
  if (idx >= 1) d["almacen"] = idx;
  else          d["almacen"] = nullptr;
  String out;
  serializeJson(d, out);
  ws.textAll(out);        // panel del carro
  servidorEnviar(out);    // servidor -> HMI y tienda
  Serial.printf("AGV senal QR: %s (estacion %d)\n", codigo, idx);
}

// ---------- vasos (la rueda los manda: "vaso") ----------
void carroVaso(int g) {
  if (g < 1 || g > 3) return;
  if (carroEstado == CARRO_CARGA) {
    if (carroVasosN < 6) carroVasos[carroVasosN++] = g;
    Serial.printf("Vaso (color %d) cae al carro.\n", g);
    enviarTelemetria();
  } else {
    carroPiso++;
    Serial.println(F("Vaso aprobado al piso: el carro no estaba en carga."));
    carroEvento("piso", 0);
  }
}

// ---------- paro / continuar (no cancela el lote: lo deja pausado) ----------
void carroParo(bool on) {
  if (on) {
    running = false;
    stopMotors();
    pwmIzqActual = 0; pwmDerActual = 0;
    carroPausado = carroLoteActivo();          // con lote: queda pausado y conserva el destino
    if (!carroLoteActivo()) destinoEstacion = -1;
  } else {
    carroPausado = false;
    if (carroEnViaje() && rampaFase == RAMPA_IDLE && destinoEstacion >= 0) {
      resetPID();
      if (!carroSim) running = true;
    }
  }
  Serial.println(on ? F("Carro: detenido.") : F("Carro: continúa."));
  enviarTelemetria();
}

// ---------- ir hacia un destino (0 = HOME, 1..3 = almacenes) ----------
void carroIrA(int est) {
  destinoEstacion = est;
  zonaLenta = false; zonaArmada = false;
  resetPID();
  if (carroPausado) return;     // queda listo: al continuar retoma hacia este destino
  cerrarRampa();                // siempre viaja con la rampa cerrada
  if (!carroSim) running = true; // en simulador el viaje lo hace carroAvanzar()
}

// ---------- salir de carga con un lote ----------
bool carroSalir(const String& loteIdJson) {
  if (carroEstado != CARRO_CARGA) {
    Serial.println(F("Carro: no está en la posición de carga."));
    return false;
  }
  if (!pinesOk && !carroSim) { carroEvento("no_disponible", 0); return false; }
  if (rampaFase != RAMPA_IDLE) { Serial.println(F("Carro: la rampa está ocupada.")); return false; }

  int dest = carroModoDest;
  if (dest < 1 || dest > 3) {             // v17: sin almacén elegido NO sale; espera a que lo elijan
    carroEsperaDest = true;
    carroIdPend = loteIdJson;
    carroEvento("sin_destino", 0);
    Serial.println(F("Carro: lote listo, esperando que elijan el almacén."));
    enviarTelemetria();
    return false;
  }
  carroEsperaDest = false;
  carroLoteN++;
  carroLoteIdJson = loteIdJson;
  carroDestino = dest;
  carroPausado = false;
  carroS = 0;
  running = false;
  carroEstado = CARRO_SALIENDO;
  carroEvento("salio", 0);
  carroIrA(dest);
  Serial.printf("Carro: lote %ld sale hacia el almacen %d\n", carroLoteN, dest);
  return true;
}

// v17: eligen el almacén; si un lote estaba esperando destino, sale en ese momento
void carroElegirDest(int v) {
  carroModoDest = constrain(v, 0, 3);
  if (carroEsperaDest && carroModoDest >= 1 && carroEstado == CARRO_CARGA) {
    carroEsperaDest = false;
    carroSalir(carroIdPend);
  }
  enviarTelemetria();
}

// ---------- llegó a una estación marcada por el QR (lo llama alLeerQR) ----------
void carroAlLlegar(int idx) {
  destinoEstacion = -1;
  zonaLenta = false; zonaArmada = false;
  stopMotors();
  pwmIzqActual = 0; pwmDerActual = 0;
  resetPID();
  running = false;

  if (idx == 0) {                          // HOME: fin del regreso
    carroS = 0;
    carroEvento("en_carga", 0);
    carroEstado = CARRO_CARGA;
    carroModoDest = 0;                     // v17: el siguiente lote necesita que elijan su almacén
    carroEsperaDest = false;
    carroDestino = 0;
    carroLoteIdJson = "null";
    carroPausado = false;
    Serial.println(F("Carro: en la posición de carga."));
    return;
  }
  // almacén: se detiene y descarga con la rampa
  carroS = CARRO_S_ALM[idx - 1];
  carroEstado = CARRO_DESCARGANDO;
  carroEvento("llegada", idx);        // el evento "qr" ya lo mandó qrSenal()
  iniciarSecuenciaRampa();
}

// ---------- ⌂ HOME: regresar a la posición de carga (botón de la página del carro) ----------
bool carroHome() {
  if (carroEstado == CARRO_CARGA) { Serial.println(F("Carro: ya está en la posición de carga (HOME).")); return false; }
  if (carroEstado == CARRO_DESCARGANDO) { Serial.println(F("Carro: está descargando; al terminar regresa solo.")); return false; }
  carroPausado = false;
  carroEstado = CARRO_REGRESANDO;          // sigue la línea hasta QR-HOME (los QR de los almacenes son de paso)
  carroIrA(0);
  Serial.println(F("Carro: regresando a la posición de carga (HOME)."));
  enviarTelemetria();
  return true;
}

// ---------- la rampa terminó de descargar (lo llama actualizarRampa) ----------
void carroTrasDescarga() {
  if (carroDestino >= 1 && carroDestino <= 3) carroEntregas[carroDestino - 1]++;
  carroVasosN = 0;
  carroEvento("entregado", carroDestino);
  carroEstado = CARRO_REGRESANDO;
  carroIrA(0);                              // regresa a HOME por la línea
}

// ---------- posición estimada en el dibujo (la usan la UI y el HMI) ----------
void carroAvanzar(float dt) {
  if (!carroEnViaje() || carroPausado) return;
  if (carroSim && simPausaHasta != 0) {                 // simulador: detenido un momento en un QR de paso
    if ((long)(millis() - simPausaHasta) < 0) return;
    simPausaHasta = 0;
  }
  float antes = carroS;
  carroS += (40.0f + carroVelPct * 2.2f) * dt;   // misma velocidad que el dibujo anterior
  if (carroSim) { simRevisarQR(antes); return; }
  if (carroS >= CARRO_L) carroS -= CARRO_L;
}

// ---------- SIMULADOR: el "lector QR" lee la estación cuando el carro pasa por ella ----------
void simLeerQR(const char* codigo, int idx, float pos) {
  carroS = pos;
  strncpy(qrUltimo, codigo, QR_MAX_LEN);
  qrUltimo[QR_MAX_LEN] = '\0';
  qrUltimoMs = millis();
  qrHayLectura = true;
  qrSenal(codigo, idx);                                  // misma señal que un QR real
  bool esDestino = (carroEstado == CARRO_SALIENDO) ? (idx == carroDestino) : (idx == 0);
  if (esDestino) { carroAlLlegar(idx); return; }
  simPausaHasta = millis() + QR_PAUSA_PASO_MS;          // QR de paso: pausa breve y sigue
  Serial.printf("Simulador: QR %s de paso.\n", codigo);
}

void simRevisarQR(float antes) {
  for (int i = 0; i < 3; i++) {
    float p = CARRO_S_ALM[i];
    if (antes < p && carroS >= p) {
      char cod[8];
      snprintf(cod, sizeof(cod), "QR-E%d", i + 1);
      simLeerQR(cod, i + 1, p);
      return;
    }
  }
  if (carroS >= CARRO_L) simLeerQR("QR-HOME", 0, CARRO_L);  // dio la vuelta: llegó a la base
}

// ---------- cambiar entre FÍSICO y SIMULADOR (solo en carga y detenido) ----------
void cambiarModoSim(bool on) {
  if (carroLoteActivo() || running || rampaFase != RAMPA_IDLE) {
    Serial.println(F("Para cambiar de modo el carro debe estar en carga, detenido y sin descargar."));
    return;
  }
  carroSim = on;
  simPausaHasta = 0;
  prefs.begin("agv", false);
  prefs.putBool("sim", carroSim);
  prefs.end();
  Serial.println(carroSim ? F("Modo SIMULADOR: el viaje se simula, motores apagados.")
                          : F("Modo FISICO: el carro sigue la linea real."));
  enviarTelemetria();
}

// ======================================================================
// Red y servidor (el ESP32 se conecta como cliente WebSocket, rol "agv")
// ======================================================================
void cargarRed() {
  prefs.begin("agv", true);
  redSsid = prefs.getString("ssid", "");
  redPass = prefs.getString("pass", "");
  srvHost = prefs.getString("srv", "");
  srvPort = prefs.getUShort("port", 3000);
  if (srvHost.length() == 0 && strlen(SERVIDOR_NUBE) > 0) { srvHost = SERVIDOR_NUBE; srvPort = 443; }
  carroSim = prefs.getBool("sim", false);
  prefs.end();
  if (redSsid.length() == 0 && strlen(WIFI_RED) > 0) {   // nada guardado: usa la red escrita en el programa
    redSsid = WIFI_RED;
    redPass = WIFI_CLAVE;
  }
}

void guardarRed(const String& ssid, const String& passNueva, const String& srv, uint16_t port) {
  // contraseña vacía en el panel = conservar la guardada (el panel no la muestra)
  String pass = (passNueva.length() == 0 && ssid == redSsid) ? redPass : passNueva;
  srvAuto = false;
  prefs.begin("agv", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.putString("srv", srv);
  prefs.putUShort("port", port);
  prefs.end();
  redSsid = ssid; redPass = pass; srvHost = srv; srvPort = port;
  limpiarServidor();
  if (srvHost.length() == 0 && strlen(SERVIDOR_NUBE) > 0) { srvHost = SERVIDOR_NUBE; srvPort = 443; }
}

// Acepta "https://algo.onrender.com/" o "wss://..." y lo deja como "algo.onrender.com" (puerto 443)
void limpiarServidor() {
  srvHost.trim();
  bool seguro = srvHost.startsWith("https://") || srvHost.startsWith("wss://");
  int p = srvHost.indexOf("://"); if (p >= 0) srvHost = srvHost.substring(p + 3);
  while (srvHost.endsWith("/")) srvHost.remove(srvHost.length() - 1);
  if (seguro) srvPort = 443;
}

// Servidor en la nube = un nombre con letras (no una IP de la red) o el puerto 443 → conexión segura (wss)
bool servidorEnNube() {
  if (srvPort == 443) return true;
  for (size_t i = 0; i < srvHost.length(); i++) if (isalpha((unsigned char)srvHost[i])) return true;
  return false;
}

void conectarRed() {
  if (redSsid.length()) WiFi.begin(redSsid.c_str(), redPass.c_str());
}

void reconectarRed() {
  wsServidor.disconnect();
  srvIniciado = false; srvOk = false; hmiEnLinea = false;
  WiFi.disconnect(false, false);        // corta solo la STA; el AP del panel sigue
  conectarRed();
}

void servidorEnviar(String s) {
  if (srvOk) wsServidor.sendTXT(s);
}

void procesarServidor(uint8_t* payload, size_t length) {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, payload, length)) return;
  const char* tipo = doc["tipo"] | "";
  if (strcmp(tipo, "hola_ok") == 0) {
    hmiEnLinea = doc["hmi"] | false;
    Serial.printf("Servidor: listo (HMI %s)\n", hmiEnLinea ? "en linea" : "sin conexion");
  } else if (strcmp(tipo, "hmi") == 0) {
    hmiEnLinea = doc["en_linea"] | false;
  } else if (strcmp(tipo, "error") == 0) {
    Serial.printf("Servidor: %s\n", (const char*)(doc["msg"] | ""));
  } else if (strcmp(tipo, "agv_permiso") == 0) {        // permiso para entrar a la sala
    permisoCarro = (const char*)(doc["estado"] | "");
    const char* sl = doc["sala"] | "";
    if (permisoCarro == "esperando")      Serial.printf("Servidor: esperando permiso para entrar a la sala '%s'. Apruebalo en la pagina del carro de esa sala.\n", sl);
    else if (permisoCarro == "aprobado")  Serial.printf("Servidor: permiso aprobado, el carro ya esta en la sala '%s'.\n", sl);
    else if (permisoCarro == "rechazado") Serial.printf("Servidor: la sala '%s' rechazo al carro.\n", sl);
    else if (permisoCarro == "fuera")     Serial.printf("Servidor: el carro salio de la sala '%s'.\n", sl);
    enviarTelemetria();
  } else if (strcmp(tipo, "agv_ctl") == 0) {             // desde la página AGV_Carro (modo Carro ESP32)
    const char* c = doc["cmd"] | "";
    if (strcmp(c, "paro") == 0)          carroParo(doc["on"] | false);
    else if (strcmp(c, "salir") == 0)    carroSalir(String("null"));
    else if (strcmp(c, "modoDest") == 0) carroElegirDest((int)(doc["v"] | 0));
    else if (strcmp(c, "vel") == 0)      carroVelPct = constrain((int)(doc["v"] | 60), 20, 100);
    else if (strcmp(c, "modoSim") == 0)  cambiarModoSim(doc["on"] | false);
    else if (strcmp(c, "home") == 0)     carroHome();
    enviarTelemetria();
  } else if (strcmp(tipo, "agv_cmd") == 0) {
    const char* cmd = doc["cmd"] | "";
    if (strcmp(cmd, "despachar") == 0) {                 // la rueda completó el lote
      String idj = "null";
      JsonVariant idv = doc["lote"]["id"];
      if (!idv.isNull()) serializeJson(idv, idj);
      int dTienda = doc["lote"]["destino"] | 0;           // la tienda eligió el almacén
      if (dTienda >= 1 && dTienda <= 3) {
        carroModoDest = dTienda;
        Serial.printf("La tienda eligio el Almacen %d.\n", dTienda);
      }
      carroSalir(idj);
    } else if (strcmp(cmd, "vaso") == 0) {
      carroVaso(doc["g"] | 0);
    } else if (strcmp(cmd, "paro") == 0) {
      carroParo(doc["on"] | false);
    }
  }
}

void onServidorEvento(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      srvOk = true;
      StaticJsonDocument<256> h;
      h["tipo"] = "hola";
      h["rol"] = "agv";
      h["sala"] = SALA;
      h["nombre"] = NOMBRE_CARRO;
      {
        String mac = WiFi.macAddress(); mac.replace(":", "");
        h["id"] = String("esp-") + mac;
      }
      permisoCarro = "";
      String s;
      serializeJson(h, s);
      wsServidor.sendTXT(s);
      Serial.println(F("Servidor: conectado (rol agv)."));
      break;
    }
    case WStype_DISCONNECTED:
      srvOk = false; hmiEnLinea = false; permisoCarro = "";
      Serial.println(F("Servidor: desconectado, reintentando..."));
      break;
    case WStype_TEXT:
      procesarServidor(payload, length);
      break;
    default:
      break;
  }
}

// Escucha el aviso "CELDA_LLENADO,<puerto>" que la laptop del servidor manda cada 2 s.
// Se usa esa IP si no hay ninguna escrita, o si la escrita no responde desde hace 15 s.
void buscarServidor() {
  if (WiFi.status() != WL_CONNECTED) { udpIniciado = false; return; }
  if (srvHost.length() && servidorEnNube()) return;     // con la nube no se busca la laptop
  if (!udpIniciado) { udpBusca.begin(PUERTO_BUSQUEDA); udpIniciado = true; }
  int n = udpBusca.parsePacket();
  if (n <= 0) return;
  char b[40];
  int k = udpBusca.read(b, sizeof(b) - 1);
  b[k > 0 ? k : 0] = '\0';
  if (strncmp(b, "CELDA_LLENADO,", 14) != 0) return;
  String ip = udpBusca.remoteIP().toString();
  uint16_t pt = (uint16_t)atoi(b + 14);
  if (pt == 0) return;
  bool cambiar = srvHost.length() == 0 ||
                 (!srvOk && millis() - srvDesdeMs > 15000 && (ip != srvHost || pt != srvPort));
  if (!cambiar) return;
  srvHost = ip; srvPort = pt; srvAuto = true;
  if (srvIniciado) { wsServidor.disconnect(); srvIniciado = false; }
  Serial.printf("Servidor encontrado en la red: %s:%u\n", srvHost.c_str(), (unsigned)srvPort);
}

void servidorLoop() {
  wsServidor.loop();
  buscarServidor();
  if (!srvIniciado && srvHost.length() && WiFi.status() == WL_CONNECTED) {
    if (servidorEnNube()) {
      if (srvPort == 3000) srvPort = 443;
      wsServidor.beginSSL(srvHost.c_str(), srvPort, "/");   // wss:// (la nube usa conexión segura)
    } else {
      wsServidor.begin(srvHost.c_str(), srvPort, "/");
    }
    wsServidor.onEvent(onServidorEvento);
    wsServidor.setReconnectInterval(3000);
    wsServidor.enableHeartbeat(20000, 5000, 2);         // mantiene viva la conexión (la nube corta las calladas)
    srvIniciado = true;
    srvDesdeMs = millis();
    Serial.printf("Servidor: conectando a %s:%u\n", srvHost.c_str(), (unsigned)srvPort);
  }
}

// ======================================================================
// Telemetría: UNA sola salida con todo, para la UI y para el servidor
// ======================================================================
void enviarTelemetria() {
  StaticJsonDocument<2048> doc;
  doc["tipo"] = "agv_estado";

  // --- carro (lo que lee el HMI y la UI del carro) ---
  doc["estado"]  = nombreCarro();
  doc["pausado"] = carroPausado;
  doc["s"]       = roundf(carroS * 10.0f) / 10.0f;
  doc["lado"]    = roundf((running ? constrain(errorActual / 350.0f, -10.0f, 10.0f) : 0.0f) * 10.0f) / 10.0f;
  doc["linea"]   = lineaTexto();
  doc["modo"]    = carroSim ? "sim" : "fisico";
  doc["sim"]     = carroSim;
  doc["puerto"]  = true;
  JsonArray va = doc.createNestedArray("vasos");
  for (int i = 0; i < carroVasosN; i++) va.add(carroVasos[i]);
  JsonArray en = doc.createNestedArray("entregas");
  for (int i = 0; i < 3; i++) en.add(carroEntregas[i]);
  doc["piso"] = carroPiso;
  if (carroLoteActivo()) {
    JsonObject lo = doc.createNestedObject("lote");
    lo["n"] = carroLoteN;
    lo["id"] = serialized(carroLoteIdJson.c_str());
    lo["destino"] = carroDestino;
  } else {
    doc["lote"] = nullptr;
  }
  doc["modoDest"] = carroModoDest;
  doc["sigRot"]   = carroSigRot;      // próximo almacén en modo rotativo
  doc["vel"]      = carroVelPct;

  // --- red ---
  doc["srv"]     = srvOk;
  doc["hmi"]     = hmiEnLinea;
  doc["sta"]     = WiFi.status() == WL_CONNECTED;
  doc["ip"]      = WiFi.localIP().toString();
  doc["ssid"]    = redSsid;
  doc["srvHost"] = srvHost;
  doc["permiso"] = permisoCarro;
  doc["sala"]    = (strlen(SALA) ? SALA : "principal");
  doc["srvPort"] = srvPort;
  doc["srvAuto"] = srvAuto;

  // --- mantenimiento: sensores, rampa, QR, motores y PID ---
  JsonArray val   = doc.createNestedArray("val");
  JsonArray ok    = doc.createNestedArray("ok");
  for (uint8_t i = 0; i < QTR8_COUNT; i++) {
    val.add(sensorValues[i]);
    ok.add(sensorOk[i]);
  }
  doc["running"]     = running;
  doc["pinesOk"]     = pinesOk;
  doc["lineaVisible"]= lineaVisibleActual;
  doc["posicion"]    = posicionActual;
  doc["error"]       = (int)errorActual;
  doc["pwmL"]        = pwmIzqActual;
  doc["pwmR"]        = pwmDerActual;
  doc["encL"]        = pulsosIzqActual;
  doc["encR"]        = pulsosDerActual;
  doc["encTotL"]     = encTotIzq;
  doc["encTotR"]     = encTotDer;
  doc["prueba"]      = (pruebaMotorHasta != 0);
  doc["kp"] = Kp; doc["ki"] = Ki; doc["kd"] = Kd;
  doc["base"] = baseSpeed; doc["max"] = maxSpeed;
  doc["kpv"] = KpVel; doc["kiv"] = KiVel; doc["kdv"] = KdVel;
  doc["rampaFase"]   = nombreFaseRampa(rampaFase);
  doc["rampaActiva"] = (rampaFase != RAMPA_IDLE);
  doc["compuertaOk"] = servoCompuertaOk;
  doc["elevacionOk"] = servoElevacionOk;
  doc["qr"]          = qrUltimo;
  doc["qrHaceMs"]    = qrHayLectura ? (long)(millis() - qrUltimoMs) : -1L;
  doc["qrBytes"]     = qrBytesRx;
  doc["irNegro"]     = irNegro;
  doc["zonaLenta"]   = zonaLenta;
  doc["destino"]     = destinoEstacion;
  doc["pausaPaso"]   = (pausaPasoHasta != 0);

  String out;
  serializeJson(doc, out);
  if (ws.count() > 0) ws.textAll(out);     // panel del carro
  if (srvOk) servidorEnviar(out);          // servidor -> HMI y tienda
}

// ======================================================================
// Comandos del panel (ESP32 directo) — mismos nombres de siempre + los del carro
// ======================================================================
void onWsMessage(uint8_t *data, size_t len) {
  StaticJsonDocument<384> doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) { Serial.printf("Panel: JSON invalido (%s)\n", err.c_str()); return; }
  const char* cmd = doc["cmd"] | "";
  Serial.printf("Panel cmd: %s\n", cmd);

  if (strcmp(cmd, "setPID") == 0) {
    if (doc.containsKey("kp"))   Kp = doc["kp"];
    if (doc.containsKey("ki"))   Ki = doc["ki"];
    if (doc.containsKey("kd"))   Kd = doc["kd"];
    if (doc.containsKey("base")) baseSpeed = doc["base"];
    if (doc.containsKey("max"))  maxSpeed = doc["max"];
  } else if (strcmp(cmd, "setVelPID") == 0) {
    if (doc.containsKey("kp")) KpVel = doc["kp"];
    if (doc.containsKey("ki")) KiVel = doc["ki"];
    if (doc.containsKey("kd")) KdVel = doc["kd"];
  } else if (strcmp(cmd, "calibrate") == 0) {
    pedirCalibracion = true;                       // se ejecuta en loop()
  } else if (strcmp(cmd, "start") == 0) {
    if (carroLoteActivo()) { if (carroPausado) carroParo(false); return; }
    if (rampaFase != RAMPA_IDLE) { Serial.println(F("Espera a que termine la rampa.")); return; }
    detenerPruebaMotores();
    cerrarRampa();
    destinoEstacion = -1;                          // sin destino: se detiene en el primer QR
    resetPID();
    running = true;
    Serial.println(F("Panel: iniciar seguimiento"));
  } else if (strcmp(cmd, "stop") == 0 || strcmp(cmd, "paro") == 0) {
    carroParo(strcmp(cmd, "stop") == 0 ? true : (doc["on"] | false));
  } else if (strcmp(cmd, "salir") == 0) {
    carroSalir(String("null"));                    // "Enviar carro ahora" sin id de lote
  } else if (strcmp(cmd, "modoDest") == 0) {
    carroElegirDest((int)(doc["v"] | 0));
  } else if (strcmp(cmd, "vel") == 0) {
    carroVelPct = constrain((int)(doc["v"] | 60), 20, 100);
  } else if (strcmp(cmd, "vaso") == 0) {
    carroVaso(doc["g"] | 0);
  } else if (strcmp(cmd, "goto") == 0) {
    irAEstacion(doc["est"] | -1);                  // 0 = HOME, 1..4 = estaciones de prueba
  } else if (strcmp(cmd, "compuertaOpen") == 0) {
    if (!running && !carroLoteActivo()) abrirCompuertaManual();
  } else if (strcmp(cmd, "compuertaClose") == 0) {
    cerrarCompuertaManual();
  } else if (strcmp(cmd, "elevacionUp") == 0) {
    if (!running && !carroLoteActivo()) subirElevacionManual();
  } else if (strcmp(cmd, "elevacionDown") == 0) {
    bajarElevacionManual();
  } else if (strcmp(cmd, "servo") == 0) {          // slider de mantenimiento (0..180)
    if (running || carroLoteActivo() || rampaFase != RAMPA_IDLE) return;
    int g = constrain((int)(doc["grados"] | 0), 0, 180);
    if (strcmp(doc["c"] | "", "c") == 0) { if (servoCompuertaOk) servoCompuerta.write(g); }
    else                                  { if (servoElevacionOk) servoElevacion.write(g); }
  } else if (strcmp(cmd, "descargar") == 0) {
    iniciarDescargaManual();
  } else if (strcmp(cmd, "testMotor") == 0) {
    iniciarPruebaMotores(doc["l"] | 0, doc["r"] | 0);
  } else if (strcmp(cmd, "testStop") == 0) {
    detenerPruebaMotores();
  } else if (strcmp(cmd, "qrTest") == 0) {
    QR_SERIAL.print("QR-E1\r");
  } else if (strcmp(cmd, "qrBaud") == 0) {
    cambiarBaudQR((unsigned long)(doc["v"] | 9600));
  } else if (strcmp(cmd, "modoSim") == 0) {
    cambiarModoSim(doc["on"] | false);
  } else if (strcmp(cmd, "red") == 0) {
    guardarRed(String(doc["ssid"] | ""), String(doc["pass"] | ""),
               String(doc["srv"] | ""), (uint16_t)(doc["port"] | 3000));
    reconectarRed();
  }
}

// ======================================================================
// Setup
// ======================================================================
// Explica por qué arrancó la placa. Si ves BROWNOUT o CRASH repetido en
// cada arranque, la red WiFi no llega a mantenerse: revisa alimentación
// (servos, lector QR) o el cableado antes de seguir.
void imprimirMotivoReinicio() {
  esp_reset_reason_t r = esp_reset_reason();
  const char* txt = "desconocido";
  switch (r) {
    case ESP_RST_POWERON:  txt = "encendido normal"; break;
    case ESP_RST_SW:       txt = "reinicio por software"; break;
    case ESP_RST_PANIC:    txt = "CRASH del programa (excepcion)"; break;
    case ESP_RST_INT_WDT:  txt = "watchdog de interrupciones"; break;
    case ESP_RST_TASK_WDT: txt = "watchdog de tarea (el programa se bloqueo)"; break;
    case ESP_RST_WDT:      txt = "watchdog"; break;
    case ESP_RST_BROWNOUT: txt = "BROWNOUT: voltaje de alimentacion muy bajo"; break;
    default: break;
  }
  Serial.printf("Motivo del ultimo reinicio: %s (codigo %d)\n", txt, (int)r);
}

void setup() {
  Serial.begin(115200);
  delay(1000);   // da tiempo a que el monitor serial se conecte y no se pierdan los primeros mensajes
  imprimirMotivoReinicio();

  led.begin();
  led.setBrightness(255); // el brillo real ya se controla con LED_BRILLO
  actualizarLed(LED_DETENIDO);

  Serial.println(F("=== MAPA DE PINES AGV ==="));
  Serial.println(F("QTR-8A analogico : GPIO1..GPIO8"));
  Serial.println(F("Encoder izquierdo: GPIO9(A), GPIO10(B)"));
  Serial.println(F("BTS izquierdo    : GPIO11 RPWM, 12 LPWM, 13 R_EN, 14 L_EN"));
  Serial.println(F("BTS derecho      : GPIO15 RPWM, 16 LPWM, 17 R_EN, 18 L_EN"));
  Serial.println(F("Encoder derecho  : GPIO21(A), GPIO38(B)"));
  Serial.println(F("Servos           : GPIO41 compuerta, GPIO42 elevacion"));
  Serial.println(F("NeoPixel         : GPIO48"));
  Serial.println(F("Lector QR GM65   : GPIO39 (RX) <- TX GM65, GPIO40 (TX) -> RX GM65"));
  Serial.println(F("Sensor IR marca  : GPIO47 (DO)"));
  Serial.println(F("========================"));

  bool dupOk = verificarPines();
  bool pwmOk = setupMotors();
  pinesOk = dupOk && pwmOk;
  stopMotors();
  setupEncoders();
  setupQR();
  setupIR();

  if (!pinesOk) {
    Serial.println(F("=========================================="));
    Serial.println(F("ERROR CRITICO de pines/PWM. El robot NO se"));
    Serial.println(F("movera hasta que corrijas el cableado y"));
    Serial.println(F("reinicies. Revisa el detalle arriba."));
    Serial.println(F("=========================================="));
  }

  // El sensor es QTR-8A: las salidas S1-S8 son ANALOGICAS.
  // NO usar setTypeRC() aquí.
  qtr.setTypeAnalog();
  qtr.setSensorPins(QTR8_PINS, QTR8_COUNT);
  // Los emisores del módulo los alimenta su propio circuito; no se usa
  // pin de control de emisor del ESP32.

  Serial.println(F("QTR-8A configurado en modo ANALOGICO: GPIO1..GPIO8."));

  // La red WiFi y el HMI se levantan PRIMERO: así, aunque la calibración
  // (5 s) o el movimiento de los servos provoquen un problema después,
  // la red ya aparece y se puede ver el estado desde el celular.
  cargarRed();
  setupWebServer();

  calibrarSensores(); // también corre el diagnóstico de mantenimiento

  setupRampa(); // después del PWM de motores; siempre arranca cerrada

  Serial.println(F("Listo. Comandos serial: p# i# d# b# m# P# I# D# c s e a x u j g q w t z# l# r# o"));
  Serial.println(F("Lector QR: 'q' = estado y bytes recibidos, 'w' = volcado HEX, 't' = prueba con puente GPIO39-40, 'z9600' = baudrate."));
}

// ======================================================================
// Un paso de control: lee QTR-8A y encoders SIEMPRE (para poder ver
// datos en vivo en el HMI aunque el robot esté detenido), y solo
// mueve motores si pinesOk && running.
// ======================================================================
void actualizarControl() {
  lineaVisibleActual = leerLineaPico();   // sigue solo la zona más sensible (ignora manchas)
  carroAvanzar(CONTROL_INTERVAL_MS / 1000.0f);
  actualizarIR();

  noInterrupts();
  pulsosIzqActual = encoderCountLeft;
  pulsosDerActual = encoderCountRight;
  encoderCountLeft = 0;
  encoderCountRight = 0;
  interrupts();
  encTotIzq += pulsosIzqActual;
  encTotDer += pulsosDerActual;

  // Al arrancar/reanudar: ignora un rato el último QR leído (el lector aún lo ve)
  static bool runningPrev = false;
  if (running && !runningPrev) {
    qrBloqueoHasta = millis() + QR_BLOQUEO_MS;
    strncpy(qrBloqueoCodigo, qrUltimo, QR_MAX_LEN);
    qrBloqueoCodigo[QR_MAX_LEN] = '\0';
    pausaPasoHasta = 0;
  }
  runningPrev = running;

  if (!pinesOk) {
    actualizarLed(LED_ERROR);
    stopMotors();
    pwmIzqActual = 0; pwmDerActual = 0;
    return;
  }

  if (!running) {
    actualizarLed(LED_DETENIDO);
    if (pruebaMotorHasta != 0) return;   // prueba manual en curso: no tocar los motores
    stopMotors();
    pwmIzqActual = 0; pwmDerActual = 0;
    return;
  }

  // Detenido en una estación descargando (lo pidió un QR-E#): sin mover
  // motores hasta que termine la secuencia de la rampa; luego retoma.
  if (pausaPorQr) {
    if (rampaFase != RAMPA_IDLE) {
      actualizarLed(LED_DETENIDO);
      stopMotors();
      pwmIzqActual = 0; pwmDerActual = 0;
      return;
    }
    pausaPorQr = false;
    integral = 0; lastError = 0;
    integralVelL = 0; lastErrorVelL = 0;
    integralVelR = 0; lastErrorVelR = 0;
    Serial.println(F("Descarga terminada: reanudando seguimiento de linea."));
  }

  // Pausa breve en un QR de paso: motores en 0 y luego sigue hacia el destino
  if (pausaPasoHasta != 0) {
    if ((long)(millis() - pausaPasoHasta) < 0) {
      actualizarLed(LED_DETENIDO);
      stopMotors();
      pwmIzqActual = 0; pwmDerActual = 0;
      return;
    }
    pausaPasoHasta = 0;
    resetPID();
  }

  actualizarLed(lineaVisibleActual ? LED_SIGUIENDO : LED_LINEA_PERDIDA);

  if (lineaVisibleActual) {
    errorActual = (float)posicionActual - (float)LINE_CENTER;
    lastSideSeen = (errorActual >= 0) ? 1 : -1;
  } else {
    // Línea perdida: gira con fuerza hacia el último lado conocido.
    errorActual = lastSideSeen * (float)LINE_CENTER;
  }

  integral += errorActual;
  integral = constrain(integral, -5000, 5000); // anti-windup simple
  float derivada = errorActual - lastError;
  lastError = errorActual;

  float correccion = Kp * errorActual + Ki * integral + Kd * derivada;

  // El seguidor NO necesita reversa para corregir: una rueda puede
  // llegar a velocidad 0, pero no a velocidad negativa.
  // Dentro de la zona de estación (marca negra bajo el IR) va más lento
  int baseEf = max(1, baseSpeed * velFactor() / 100);
  int maxEf  = max(1, maxSpeed  * velFactor() / 100);
  if (zonaLenta) {
    baseEf = max(1, (int)(baseSpeed * ZONA_LENTA_FACTOR * velFactor() / 100));
    maxEf  = max(1, (int)(maxSpeed  * ZONA_LENTA_FACTOR * velFactor() / 100));
  }

  int objetivoIzq = baseEf - (int)correccion;
  int objetivoDer = baseEf + (int)correccion;

  objetivoIzq = constrain(objetivoIzq, 0, maxEf);
  objetivoDer = constrain(objetivoDer, 0, maxEf);

  pwmIzqActual = pidVelocidad(objetivoIzq, pulsosIzqActual, integralVelL, lastErrorVelL);
  pwmDerActual = pidVelocidad(objetivoDer, pulsosDerActual, integralVelR, lastErrorVelR);

  // Seguridad adicional: en seguimiento automático no hay PWM negativo.
  pwmIzqActual = constrain(pwmIzqActual, 0, PWM_MAX);
  pwmDerActual = constrain(pwmDerActual, 0, PWM_MAX);

  setMotorLeft(pwmIzqActual);
  setMotorRight(pwmDerActual);
}

void loop() {
  procesarComandoSerial();
  ws.cleanupClients();
  dnsServer.processNextRequest();
  actualizarRampa(); // avanza la secuencia de descarga
  servoCompuerta.actualizar();   // movimiento suave de los servos
  servoElevacion.actualizar();
  leerQR();          // lector QR GM65 (no bloqueante)
  servidorLoop();    // puente con el servidor (HMI y tienda)
  actualizarPruebaMotores();

  // Recalibración pedida desde el HMI o por Serial: se hace aquí, en la
  // tarea principal, para no bloquear la red durante los 5 s.
  if (pedirCalibracion) {
    pedirCalibracion = false;
    calibrarSensores();
  }

  unsigned long ahora = millis();

  if (ahora - ultimoControl >= CONTROL_INTERVAL_MS) {
    ultimoControl = ahora;
    actualizarControl();
  }

  if (ahora - ultimoBroadcast >= BROADCAST_INTERVAL_MS) {
    ultimoBroadcast = ahora;
    enviarTelemetria();
  }
}
