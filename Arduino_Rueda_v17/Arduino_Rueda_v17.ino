/* ============================================================================
   RUEDA DE GINEBRA · 2 ORIFICIOS (A y B a 180°) · 4 ESTACIONES (cada 90°)
   Arduino UNO / Nano / Mega · driver CL57T (PUL, DIR, ENA) · HMI por Web Serial

   CAMBIOS RESPECTO A LA VERSIÓN ANTERIOR
   - Trama GIN cada 100 ms, enviada sin bloquear (no distorsiona los pulsos del motor).
   - Sensores F0, F1, F2, S3 leídos de los pines reales (polaridad configurable)
     y enviados en la subcadena `sens` ("1100": F0 y F1 activos).
   - `cups` refleja los 4 puestos: '-' vacío/sin orificio, 'E' vaso vacío,
     'F' llenado, 'X' llenado y tapado (ej. "E---").
   - PAUSA POR ESTACIÓN: al terminar cada indexado de 90° el motor se detiene y
     el estado pasa a 9 (ESPERANDO_OK). Solo vuelve a girar con
     OK_LLENADO / OK_TAPADO / OK_VERIF:
        · estación con vaso que requiere OK -> necesita SU OK (Llenado=1, Tapado=2, Verif=3)
        · si ninguna estación requiere OK (rueda vacía, vaso rechazado)
          -> cualquiera de los tres OK libera la parada.

   Estados: 0 INACTIVO · 1 PROCESANDO · 2 INDEXANDO · 3 PAUSA · 4 PARO · 5 SIN VASOS
            6 BUSCANDO HOME · 7 POSICIONANDO · 8 ESPERANDO SENSOR/VASO
            9 ESPERANDO OK · 10 ESPERANDO PASO (manual)

   Archivo: Arduino_Rueda_v17.ino (va en la carpeta "Arduino_Rueda_v17").

   UNIDO CON el antiguo dispensador_e2.ino (mismo Arduino, mismo pin 8):
   - El sensor F1 del pin 8 (estación Llenado) es también el "Sensor E2" del dispensador.
   - Al encender manda READY y el estado del sensor: E2,1 (hay recipiente) / E2,0 (no hay).
   - Cada cambio estable (antirrebote de 50 ms) se reporta con E2,1 / E2,0.
   - Responde a "?" (READY + estado) y a "E2?" (solo estado), igual que dispensador_e2.ino.
   - Las líneas E2 salen por el mismo búfer sin bloqueo, así nunca se meten en medio de una trama GIN.
   El HMI recibe E2 por el puerto de la rueda y lo reenvía al Arduino del dispensador como V,1 / V,0.

   CAMBIOS v13:
   - TAPAS: el inductivo I2 (pin 9) se lee en CADA vuelta del loop (no cada 20 ms). En cuanto la tapa
     pasa frente al sensor, el servo de tapas se detiene AHÍ MISMO (antes un paso rápido de la tapa
     no se alcanzaba a ver y el servo seguía girando). Prueba con TAPA? (pin, estado y si ya pasó).
   - MOTOR VIBRADOR (pin 4): ya NO se enciende al llegar el vaso. Se enciende solo cuando el
     dispensador empieza a servir (el HMI manda VIB:1) y al terminar (DISP_FIN) sigue vibrando
     T_VIB_EXTRA_MS para que caiga el producto atorado. El OK de Llenado se da al terminar esa vibración.
   - CARRO AGV: el HMI avisa AGV:1 (el carro está en carga y puede recibir vasos) o AGV:0 (no).
     v14: con AGV:0 la rueda NO GIRA NADA (termina lo que esté haciendo, p. ej. el tapado, y se queda
     en su estación). Cuando el carro vuelve a carga (AGV:1) sigue girando desde donde se quedó.
   - Avisos nuevos: CAIDA,<tolva> (un vaso bueno salió de Verificación y cayó al carro) y TAPA_FIN
     (terminó el ciclo de tapado). Con ellos el HMI decide cuándo sale el carro.

   MOTOR DC DEL DISPENSADO (pin 4) Y OK AUTOMÁTICO (versión anterior, ver CAMBIOS v13):
   - Cuando el pin 8 detecta un vaso (cambio estable a "hay recipiente") se ENCIENDE el motor DC (pin 4).
   - Cuando el dispensador termina, el HMI manda DISP_FIN: se APAGA el motor y se da el OK de Llenado
     automáticamente (igual que OK_LLENADO), así la rueda gira sola a la siguiente estación.
   - Por seguridad el motor también se apaga si el vaso se retira, con PARO, con OK_LLENADO manual
     o si pasan T_MOTOR_DC_MAX_MS sin que llegue DISP_FIN.
   - El motor no vuelve a encender con el mismo vaso: espera a que el pin 8 quede libre y llegue otro.
   - El DESVIADOR de rechazo se movió del pin 4 al pin 13.

   REPOSICIÓN DE RECHAZADOS (lote completo):
   - El HMI manda DISP_FIN:<tolva> (1-3). El vaso de Llenado guarda qué grano lleva y lo conserva
     al pasar por Tapado y Verificación.
   - Al salir de Verificación se reporta VERIF,<tolva>,1 (bueno, con tapa) o VERIF,<tolva>,0 (rechazo).
     El HMI pide al dispensador volver a servir ese grano hasta completar el lote.

   Trama: GIN,estado,n,cups,sens,peso10,valv,servo,desv,pedidos,buenos,rech,f1,f2,f3,home,homed
   Comandos: INICIAR PAUSA PARO RESET HOME PASO ESTADO? MODO:AUTO MODO:MANUAL
             OK_LLENADO OK_TAPADO OK_VERIF SPEED:<10-100> SET_ST<0-3>:<deg> GRADOS?
             JOG:±<deg> GOTO_ST:<0-3> GOTO_DEG:<idx>:<deg> FORZ:<tag>:<0|1> POS? TARA CAL:<g>
             TAPA_V? / TAPA_V:<µs 1000-2000> (velocidad del servo de tapas, 1500 = parado; se guarda)
             TAPA_PRUEBA (gira hasta que I2 ve la tapa o 5 s; también en automático) · TAPA_PRUEBA:0 (detener)
             ACT_EXT ACT_RET ACT_GIRO:<ms> ACT_STOP (actuador E3 de 360°)
             ? E2?  (sensor E2 del dispensador) · DISP_FIN[:tolva] (fin de dispensado: vibra T_VIB_EXTRA_MS y da el OK de Llenado)
             VIB:1 / VIB:0 (vibrador automático al dispensar) · VIB_MAN:1|0 (BOTÓN del HMI: solo lo apaga el botón)
             VIB? · VIB_POL:1|0 (enciende con HIGH|LOW, se guarda) -> responde VIB,on,alto,paro,causa,manual · BOOT al arrancar
             AGV:1 / AGV:0 (el carro puede o no recibir vasos)
   Respuestas extra: READY · E2,0|1 · VERIF,<tolva>,<1 bueno|0 rechazo> · CAIDA,<tolva> · TAPA_FIN
   ============================================================================ */

#include <EEPROM.h>
#include <string.h>
#include <Servo.h>      // servo de rotación continua (360°) de la Estación 2

/* ---------------------------- Pines ---------------------------- */
const uint8_t PIN_PUL = 5;
const uint8_t PIN_DIR = 2;
const uint8_t PIN_ENA = 3;
const bool    ENA_ACTIVA_BAJA = true;
const bool    DIR_INVERTIDA   = true;

const uint8_t PIN_HOME = 12;
const uint8_t PIN_F0   = 7;      // E0 Carga: recipiente en la rampa
const uint8_t PIN_F1   = 8;      // E1 Llenado: recipiente presente
const uint8_t PIN_F2   = 11;     // E2 Tapado: recipiente presente
const uint8_t PIN_S3   = 10;     // E3 Verificación: inductivo, detecta la TAPA
const uint8_t PIN_TAPA_IND = 9;  // E2 Tapado: inductivo I2, detecta la tapa colocada

/* Polaridad de cada sensor con INPUT_PULLUP:
   true  = activo en LOW  (salida NPN / contacto a GND)
   false = activo en HIGH (salida PNP; usa resistencia de 10k a GND y quita el pull-up si hace falta)
   Si en la HMI los sensores aparecen siempre encendidos o siempre apagados, cambia aquí. */
const bool HOME_ACT_BAJO = true;
const bool F0_ACT_BAJO   = true;
const bool F1_ACT_BAJO   = true;
const bool F2_ACT_BAJO   = true;
const bool S3_ACT_BAJO   = true;
const bool TAPA_IND_ACT_BAJO = true;   // inductivo I2

const uint8_t PIN_V1 = A4, PIN_V2 = A1, PIN_V3 = A2;   // V1 movida a A4
const uint8_t PIN_ACT_SERVO = A0;                          // servo del ACTUADOR LINEAL E3 (A0)
const uint8_t PIN_SERVO_TAPA = 6;
const uint8_t PIN_DESVIADOR  = 13;   // antes pin 4 (el pin 4 ahora es el motor DC del dispensado)
const uint8_t PIN_MOTOR_DC   = 4;    // motor DC del dispensado: ON con vaso en el pin 8, OFF al terminar de dispensar
const bool    MOTOR_DC_ACT_ALTO = true;   // true = HIGH enciende el motor (cámbialo si tu relé/driver es activo en LOW)
const uint8_t PIN_POT        = A3;
const bool    USAR_POT_FISICO = false;

/* ---------------------------- Motor ---------------------------- */
const long     PASOS_REV    = 3200;
const uint16_t STEP_US_HOME = 900;

uint8_t  velocidad      = 50;
uint16_t stepUsIndex    = 889;
long     posPasos       = 0;
long     pasosRestantes = 0;
int8_t   dirActual      = 1;
uint16_t stepUsActual   = 889;
long     movTotal       = 0;
const uint16_t STEP_US_ARRANQUE = 1600;
const long     RAMPA_PASOS      = 300;
unsigned long lastPulseMicros = 0;
bool     pulseLevel     = false;
bool     motorEnMovimiento = false;

uint16_t stepUsDesdeVel(uint8_t v){
  if(v < 10)  v = 10;
  if(v > 100) v = 100;
  return (uint16_t)map(v, 10, 100, 1400, 250);
}
long gradosAPasos(float g){ return (long)round((double)g / 360.0 * PASOS_REV); }

/* ---------------------------- Calibración (EEPROM) ---------------------------- */
float stGrados[4] = {0, 90, 180, 270};
const int      EEPROM_BASE  = 0;
const uint16_t EEPROM_MAGIC = 0xE518;

/* Posición absoluta (en pasos de motor, 0..PASOS_REV-1) de la rueda cuando el orificio A está en cada estación.
   Se recalcula SIEMPRE que cambian los grados calibrados. */
long pasosEstacion[4] = {0, 800, 1600, 2400};
void recalcularPasos(){
  for(uint8_t i = 0; i < 4; i++){
    float g = fmod(stGrados[i], 360.0f); if(g < 0) g += 360.0f;
    pasosEstacion[i] = gradosAPasos(g) % PASOS_REV;
  }
}

void guardarCalibracion(){
  EEPROM.put(EEPROM_BASE, EEPROM_MAGIC);
  EEPROM.put(EEPROM_BASE + sizeof(uint16_t), stGrados);
}
void grados_por_defecto(){
  stGrados[0] = 0; stGrados[1] = 90; stGrados[2] = 180; stGrados[3] = 270;
}
void cargarCalibracion(){
  uint16_t magic;
  EEPROM.get(EEPROM_BASE, magic);
  if(magic == EEPROM_MAGIC){
    EEPROM.get(EEPROM_BASE + sizeof(uint16_t), stGrados);
    for(uint8_t i = 0; i < 4; i++)                       // EEPROM corrupta o de otro firmware: valores por defecto
      if(isnan(stGrados[i]) || stGrados[i] < 0 || stGrados[i] >= 360){ grados_por_defecto(); guardarCalibracion(); break; }
  } else { grados_por_defecto(); guardarCalibracion(); }
  recalcularPasos();
}

/* ---------------------------- Estado ---------------------------- */
enum Estado : uint8_t { ST_IDLE=0, ST_RUN=1, ST_INDEX=2, ST_PAUSE=3, ST_ESTOP=4, ST_VACIO=5,
                        ST_HOME=6, ST_POS=7, ST_ESPERA_VASO=8, ST_ESPERA_OK=9, ST_ESPERA_PASO=10 };
Estado estado = ST_IDLE;
Estado estadoAntesPos = ST_IDLE;
bool   pausaPendiente = false;
int8_t posDestinoEst  = -1;
bool   indexManual    = false;
bool   homed          = false;
bool   pulsoHome      = false;
uint8_t estacionA     = 0;
bool    modoAuto      = true;
bool    inicioPendiente = false;

struct Vaso { bool presente; bool lleno; bool tapa; bool verificado; bool rech; bool real; bool dispensada; bool cuenta; uint8_t grano; };   // grano: tolva 1-3 del dispensador (0 = sin dato)   // real = un sensor SÍ vio el recipiente
Vaso vaso[4];
bool okLatch[4] = {false, false, false, false};
bool okLiberado = false;     // llegó algún OK_* durante esta parada (libera paradas sin OK propio)

uint8_t valvulaActiva    = 0;
uint8_t siguienteValvula = 1;
bool    servoActivo      = false;
bool    desviadorActivo  = false;

long tCarga = -1, tLlenado = -1, tTapado = -1, tDesv = -1;

/* ---- Estación 2 (Tapado): servo 360° + inductivo I2 ---- */
Servo servoTapa;
const unsigned long T_TAPADO_TIMEOUT_MS = 5000;   // si I2 no detecta la tapa en este tiempo, el servo se detiene
/* VELOCIDAD del servo de tapas (360°): pulso en µs. 1500 = parado; entre más lejos de 1500, más rápido.
   Menos de 1500 gira para un lado y más de 1500 para el otro. Se calibra en el HMI y se guarda en la EEPROM. */
uint16_t servoTapaUs = 1100;                       // de fábrica (como antes)
const int      EE_TAPA_BASE  = 120;               // después del tiempo retraído (110)
const uint16_t EE_TAPA_MAGIC = 0x7A91;
struct CfgTapa { uint16_t magic; uint16_t us; };
bool tapaUsValido(long us){ return us >= 1000 && us <= 2000 && (us <= 1460 || us >= 1540); }
void cargarServoTapa(){ CfgTapa c; EEPROM.get(EE_TAPA_BASE, c); if(c.magic == EE_TAPA_MAGIC && tapaUsValido(c.us)) servoTapaUs = c.us; }
void guardarServoTapa(){ CfgTapa c = { EE_TAPA_MAGIC, servoTapaUs }; EEPROM.put(EE_TAPA_BASE, c); }
void enviarServoTapa(){ Serial.print(F("TAPA_V,")); Serial.println(servoTapaUs); }
const uint8_t  TAPA_IND_DEBOUNCE_TICKS = 2;       // lecturas seguidas (20 ms c/u) para validar I2
uint8_t  faseTapa    = 0;      // 0 espera · 1 servo girando hasta I2 · 2 sellando
bool     tapaE2Hecha = false;  // el tapado de ESTA parada ya terminó
bool     tapaForzada = false;  // OK_TAPADO como respaldo manual
uint8_t  tapaIndCont = 0;
bool     tapaIndLibre = false;   // I2 ya se vio libre desde que arrancó el servo
bool     tapaPaso     = false;   // I2 vio pasar la tapa (lectura rápida en el loop): el servo ya se detuvo
uint8_t  tapaRapCont  = 0;       // lecturas seguidas de I2 activo (lectura rápida)
const uint8_t TAPA_IND_LECTURAS = 3;   // lecturas seguidas (una por vuelta del loop) para aceptar la tapa: filtra ruido
unsigned long forzServoFin = 0;
const long T_CARGA_MS   = 400;
const long T_LLENADO_MS = 1200;
const long T_TAPADO_MS  = 2000;           // tiempo de SELLADO después de que I2 detecta la tapa
const long T_DESV_MS    = 600;
const long T_SENSOR_MS  = 1500;
const long T_ESPERA_CARGA_MS = 1500;
long tEsperaF0 = T_ESPERA_CARGA_MS;
const long T_PARADA_MS = 1000;           // la rueda SIEMPRE se queda quieta al menos 1 s en cada estación
long tParada = T_PARADA_MS;
bool sensorVisto[4]   = {false, false, false, false};
bool esperaVencida[4] = {false, false, false, false};
long tSens[4]         = {1500, 1500, 1500, 1500};

unsigned long pedidosCiclo = 0, buenos = 0, rech = 0;
bool contando = false;   // false durante la primera vuelta tras el home (no existe como pedido)
unsigned long fValv[3] = {0, 0, 0};
float pesoActual = 0;

/* serie */
const uint8_t BUF_LEN = 40;
char rxBuf[BUF_LEN];
uint8_t rxLen = 0;
unsigned long lastEmit  = 0;
unsigned long lastLogic = 0;
const unsigned long EMIT_MS = 100;       // periodo de la trama GIN

/* búfer de salida: la trama se arma una vez y se vacía sin bloquear el loop */
char    txBuf[128];
uint8_t txLen = 0, txPos = 0;

/* ---- Sensor E2 del dispensador (antes dispensador_e2.ino): mismo pin 8 que F1 ---- */
const unsigned long E2_ANTIRREBOTE_MS = 50;   // LLEGA un vaso: la lectura debe mantenerse este tiempo
/* SE VA el vaso CON EL VIBRADOR PRENDIDO: tiene que dejar de verse este tiempo seguido. Con la vibración el sensor parpadea y,
   con solo 50 ms, el dispensador creía que pusieron otro vaso y servía la siguiente porción en el MISMO vaso. */
const unsigned long E2_SALIDA_MS = 500;
bool e2Estado    = false;     // lo último que se reportó (true = hay recipiente)
bool e2Candidato = false;     // lectura que se está confirmando
unsigned long e2TCambio = 0;
bool e2Pendiente = false;     // hay que mandar E2,x en cuanto el búfer esté libre

/* ---- Motor DC del dispensado (pin 4) ---- */
const unsigned long T_MOTOR_DC_MAX_MS = 60000;   // seguridad: apaga solo si en 60 s no llega DISP_FIN (ajústalo)
bool motorDcOn = false;
/* v13: el motor del pin 4 es el VIBRADOR: solo vibra mientras se dispensa y unos segundos más */
const unsigned long T_VIB_EXTRA_MS = 3000;   // AJUSTAR: tiempo que sigue vibrando al terminar de dispensar
unsigned long vibFinHasta = 0;               // != 0: vibrando el tiempo extra; al terminar se da el OK de Llenado
bool okLlenadoPend = false;                  // el OK de Llenado espera a que termine la vibración extra
bool agvListo = true;                        // AGV:1 / AGV:0 desde el HMI: ¿el carro puede recibir vasos?
bool motorDcHecho = false;    // este vaso ya terminó de dispensar: no volver a encender hasta que llegue otro
unsigned long motorDcT0 = 0;
/* VIBRADOR: con qué señal enciende el relé/driver del pin 4 (se cambia desde el HMI y se guarda).
   true = HIGH enciende (driver/transistor) · false = LOW enciende (módulo de relé activo en bajo) */
bool vibAlto = MOTOR_DC_ACT_ALTO;
/* BOTÓN del HMI (VIB_MAN): encendido a mano. Así NO lo apaga el sensor del pin 8, ni el fin del
   dispensado: solo el botón, el paro o la seguridad de 10 min. */
bool vibManual = false;
const unsigned long T_VIB_MANUAL_MAX_MS = 600000UL;   // seguridad del encendido a mano: 10 minutos
/* por qué se apagó la última vez (se manda al HMI): 0 botón/orden · 1 sensor E1 (pin 8) dejó de ver el vaso
   2 seguridad por tiempo · 3 fin del dispensado · 4 paro de emergencia */
uint8_t vibCausa = 0;
const int      EE_VIB_BASE  = 100;               // después del actuador (64)
const uint16_t EE_VIB_MAGIC = 0xB1B0;
struct CfgVib { uint16_t magic; uint8_t alto; };
void cargarVib(){ CfgVib c; EEPROM.get(EE_VIB_BASE, c); if(c.magic == EE_VIB_MAGIC && c.alto <= 1) vibAlto = (c.alto == 1); }
void guardarVib(){ CfgVib c = { EE_VIB_MAGIC, (uint8_t)(vibAlto ? 1 : 0) }; EEPROM.put(EE_VIB_BASE, c); }

/* ---- Resultados de verificación pendientes de enviar (VERIF,tolva,ok) ---- */
uint8_t verifCola[8];           // VERIF: bit 7 = bueno, bits 0-3 = tolva · CAIDA: 0x40 | tolva · TAPA_FIN: 0x20
uint8_t nVerif = 0;
void reportarVerif(uint8_t grano, bool bueno){
  if(grano == 0 || nVerif >= sizeof(verifCola)) return;
  verifCola[nVerif++] = grano | (bueno ? 0x80 : 0);
}
void reportarEvento(uint8_t codigo){
  if(nVerif >= sizeof(verifCola)) return;
  verifCola[nVerif++] = codigo;
}

/* ============================================================================
   SETUP
   ============================================================================ */
void setup(){
  Serial.begin(115200);

  pinMode(PIN_PUL, OUTPUT); pinMode(PIN_DIR, OUTPUT); pinMode(PIN_ENA, OUTPUT);
  habilitarMotor(false);

  pinMode(PIN_HOME, INPUT_PULLUP);
  pinMode(PIN_F0, INPUT_PULLUP); pinMode(PIN_F1, INPUT_PULLUP);
  pinMode(PIN_F2, INPUT_PULLUP); pinMode(PIN_S3, INPUT_PULLUP);
  pinMode(PIN_TAPA_IND, TAPA_IND_ACT_BAJO ? INPUT_PULLUP : INPUT);

  pinMode(PIN_V1, OUTPUT); pinMode(PIN_V2, OUTPUT); pinMode(PIN_V3, OUTPUT);
  pinMode(PIN_SERVO_TAPA, OUTPUT); pinMode(PIN_DESVIADOR, OUTPUT);
  cargarVib();                                  // con qué señal enciende el vibrador (guardado)
  pinMode(PIN_MOTOR_DC, OUTPUT);
  digitalWrite(PIN_V1, LOW); digitalWrite(PIN_V2, LOW); digitalWrite(PIN_V3, LOW);
  digitalWrite(PIN_SERVO_TAPA, LOW); digitalWrite(PIN_DESVIADOR, LOW);
  motorDc(false);

  cargarCalibracion();
  stepUsIndex  = stepUsDesdeVel(velocidad);
  stepUsActual = stepUsIndex;
  limpiarVasos();
  cargarActuador();
  cargarServoTapa();
  iniciarActuador();

  delay(200);                                   // igual que dispensador_e2.ino
  e2Estado = e2Candidato = sensorF1();
  e2TCambio = millis();
  Serial.println(F("BOOT"));                    // el Arduino arrancó (si llega a media prueba = se reinició)
  Serial.println(F("READY"));                   // el loop todavía no manda tramas: aquí no se mezcla nada
  e2Pendiente = true;                           // manda E2,1 / E2,0 en el primer ciclo
}

void motorDc(bool on){
  digitalWrite(PIN_MOTOR_DC, (on == vibAlto) ? HIGH : LOW);
  if(on && !motorDcOn) motorDcT0 = millis();
  motorDcOn = on;
}
/* VIB,<encendido>,<enciende con HIGH>,<paro>,<causa del último apagado>,<a mano> -> el HMI muestra el estado real */
void enviarVib(){
  while(txLen) drenarTx();                      // no se mezcla con una trama GIN a medias
  Serial.print(F("VIB,")); Serial.print(motorDcOn ? 1 : 0);
  Serial.print(','); Serial.print(vibAlto ? 1 : 0);
  Serial.print(','); Serial.print(estado == ST_ESTOP ? 1 : 0);
  Serial.print(','); Serial.print(vibCausa);
  Serial.print(','); Serial.println(vibManual ? 1 : 0);
}

void habilitarMotor(bool on){
  digitalWrite(PIN_ENA, ENA_ACTIVA_BAJA ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
}

/* ============================================================================
   SENSORES (lectura real de pines, con polaridad configurable)
   ============================================================================ */
bool leer(uint8_t pin, bool activoBajo){ return (digitalRead(pin) == LOW) == activoBajo; }
bool sensorHome(){ return leer(PIN_HOME, HOME_ACT_BAJO); }
bool sensorF0(){ return leer(PIN_F0, F0_ACT_BAJO); }
bool sensorF1(){ return leer(PIN_F1, F1_ACT_BAJO); }
bool sensorF2(){ return leer(PIN_F2, F2_ACT_BAJO); }
bool sensorS3(){ return leer(PIN_S3, S3_ACT_BAJO); }
bool sensorTapaIndRaw(){ return leer(PIN_TAPA_IND, TAPA_IND_ACT_BAJO); }
bool sensorTapaInd(){ return tapaIndCont >= TAPA_IND_DEBOUNCE_TICKS; }

/* ================= ACTUADOR E3 · SERVO DE ROTACIÓN CONTINUA (360°) =================
   Servo de 360° en PIN_ACT_SERVO (A0). No tiene HOME: se mueve POR TIEMPO.
     write(0)   = EXTENDER  (T_ACT_IDA_MS, de fábrica 1100 ms)
     write(90)  = PARAR
     write(180) = RETRAER   (T_ACT_REGRESO_MS, de fábrica 1136 ms)
   ANTES DE PRENDER: deja el actuador a mano en RETRAÍDO (paso libre de los recipientes al carro).
   AL PRENDER: NO se mueve, se queda retraído. (Retraído = al carro · Extendido = a la MERMA)
   LÓGICA SIN TIEMPO, solo con el sensor de tapa de Verificación (E3):
     vaso CON tapa: si está extendido se RETRAE y se queda así; si ya está retraído, no hace nada.
     vaso SIN tapa: si está retraído se EXTIENDE y se queda así; si ya está extendido, no hace nada.

   CÓMO CALIBRARLO (HMI > Calibración > Actuador lineal, o la consola de la rueda):
     ACT_EXT        extiende T_ACT_IDA_MS y se para (solo si está retraído)
     ACT_RET        retrae T_ACT_REGRESO_MS y se para (solo si está extendido)
     ACT_GIRO:<ms>  gira <ms> milisegundos (+ = extender, - = retraer), para medir cuánto tarda
     ACT_STOP       lo detiene al instante
     ACT_T?                      lee los tiempos guardados -> ACT_T,ida,regreso,velocidad,sentido
     ACT_T:ida,regreso,vel,sent  los cambia y los GUARDA en la EEPROM (ya no hay que reprogramar)
     ACT_EXT / ACT_RET           probar extender (si está retraído) / retraer (si está extendido)
     ACT_POS_RET                 avisar que lo pusiste a mano RETRAÍDO
   Todo esto se hace desde el HMI: pestaña Rueda > Actuador lineal.
   - Si no extiende completo: sube el tiempo de extender.   Si se pasa o forza: bájalo.
   - Si no retrae completo: sube el tiempo de retraer.
   - Si gira al revés: cambia el sentido.
   - Velocidad = cuántos grados se aleja de 90 (90 = máxima, como write(0)/write(180); 10 = muy lento). */
int8_t   ACT_SENTIDO        = 1;             // 1 o -1: invierte cuál giro es "extender"
const uint8_t  ACT_PARO_GRADOS = 90;         // write(90) = parado (como en el código de prueba)
uint16_t ACT_VELOCIDAD_US   = 90;            // grados que se aleja de 90: 90 = write(0) / write(180) (máxima)
unsigned long T_ACT_IDA_MS     = 1100;       // EXTENDER: ms con write(0)   — se calibra en el HMI
unsigned long T_ACT_REGRESO_MS = 1136;       // RETRAER:  ms con write(180) — se calibra en el HMI
/* tiempos del actuador guardados en la EEPROM (después de la calibración de la rueda) */
const int      EE_ACT_BASE  = 64;
const uint16_t EE_ACT_MAGIC = 0xAC37;          // nuevo: los tiempos viejos (otra lógica) no se cargan
struct CfgAct { uint16_t magic; uint16_t ida; uint16_t reg; uint16_t vel; int8_t sent; };
void cargarActuador(){
  CfgAct c; EEPROM.get(EE_ACT_BASE, c);
  if(c.magic != EE_ACT_MAGIC) return;                       // nunca se guardó: valores de arriba
  if(c.ida >= 50 && c.ida <= 20000) T_ACT_IDA_MS = c.ida;
  if(c.reg >= 50 && c.reg <= 20000) T_ACT_REGRESO_MS = c.reg;
  if(c.vel >= 10 && c.vel <= 90) ACT_VELOCIDAD_US = c.vel;
  if(c.sent == 1 || c.sent == -1) ACT_SENTIDO = c.sent;
}
void guardarActuador(){
  CfgAct c = { EE_ACT_MAGIC, (uint16_t)T_ACT_IDA_MS, (uint16_t)T_ACT_REGRESO_MS, ACT_VELOCIDAD_US, ACT_SENTIDO };
  EEPROM.put(EE_ACT_BASE, c);
}
void enviarActuador(){
  Serial.print(F("ACT_T,")); Serial.print(T_ACT_IDA_MS); Serial.print(','); Serial.print(T_ACT_REGRESO_MS);
  Serial.print(','); Serial.print(ACT_VELOCIDAD_US); Serial.print(','); Serial.println((int)ACT_SENTIDO);
}
/* Para detenerlo se manda write(90), igual que en el código de prueba que sí funcionó. */

Servo srvAct;

/* REPOSO = extendido (merma) · HACIA_ACTIVO = retrayendo · SOSTENIDO = retraído (paso al carro)
   REGRESANDO = extendiendo de nuevo · INICIO = extendiendo al prender */
enum ActEstado : uint8_t { ACT_REPOSO, ACT_HACIA_ACTIVO, ACT_SOSTENIDO, ACT_REGRESANDO, ACT_INICIO };
ActEstado actEstado = ACT_REPOSO;
unsigned long actT0 = 0;
bool actF2Libre = false;                    // E2 ya quedó libre desde que se activó
unsigned long actTActivo = 0;               // momento en que se activó
unsigned long actGiroFin = 0;               // giro manual (ACT_EXT / ACT_RET / ACT_GIRO): hasta cuándo
bool actGiroManual = false;
unsigned long actRegresoMs = 1100;          // cuánto debe volver a extender (menos si un paro cortó la retracción a la mitad)

/* dir: 1 adelante, -1 atrás, 0 parado */
/* dir: 1 = EXTENDER (write(0)), -1 = RETRAER (write(180)), 0 = PARAR (write(90)) */
void actGirar(int8_t dir){
  if(!srvAct.attached()) srvAct.attach(PIN_ACT_SERVO);        // mismo attach que el código de prueba
  if(dir == 0){ srvAct.write(ACT_PARO_GRADOS); return; }
  int g = (int)ACT_PARO_GRADOS - dir * ACT_SENTIDO * (int)ACT_VELOCIDAD_US;   // extender = 90-90 = 0 · retraer = 90+90 = 180
  srvAct.write(constrain(g, 0, 180));
}

// Llamar desde setup(): el actuador se dejó a mano RETRAÍDO y ASÍ SE QUEDA (no se mueve al prender)
void iniciarActuador() {
  actGirar(0);
  actRegresoMs = T_ACT_IDA_MS;               // para extenderse completo desde retraído
  actEstado = ACT_SOSTENIDO;                 // retraído: paso libre al carro
}

bool actuadorOcupado() { return actEstado != ACT_REPOSO && actEstado != ACT_SOSTENIDO; }   // se está moviendo

/* LÓGICA DEL ACTUADOR (sin tiempo):
     vaso CON tapa  -> si está extendido se RETRAE (paso al carro) y se queda retraído;
                       si ya está retraído, no hace nada.
     vaso SIN tapa  -> si está retraído se EXTIENDE (paso a la merma) y se queda extendido;
                       si ya está extendido, no hace nada.  */
bool actPendiente = false;                  // llegó un vaso bueno mientras se extendía: al terminar se retrae
bool actPendMerma = false;                  // llegó un vaso sin tapa mientras se retraía: al terminar se extiende
void dispararActuador() {
  actPendMerma = false;
  if (actEstado == ACT_SOSTENIDO || actEstado == ACT_HACIA_ACTIVO) return;   // ya está (o se está) retrayendo: no hace nada
  if (actEstado == ACT_REGRESANDO || actEstado == ACT_INICIO) { actPendiente = true; return; }
  if (actEstado != ACT_REPOSO) return;
  actGiroManual = false;
  actGirar(-1);                              // empieza a retraerse
  actRegresoMs = T_ACT_IDA_MS;               // luego vuelve a extender todo
  actT0 = millis();
  actTActivo = actT0;
  actF2Libre = false;
  actEstado = ACT_HACIA_ACTIVO;
}

/* vaso SIN TAPA en Calidad: si el actuador está retraído, se EXTIENDE para que el vaso vaya a la merma
   (y se queda extendido hasta que llegue un vaso con tapa) */
void actuadorAMerma() {
  actPendiente = false;
  if (actEstado == ACT_HACIA_ACTIVO) { actPendMerma = true; return; }      // se está retrayendo: al terminar se extiende
  if (actEstado != ACT_SOSTENIDO) return;                                   // ya está extendido: no hace nada
  actGirar(1); actT0 = millis(); actEstado = ACT_REGRESANDO;   // (actRegresoMs: todo, o lo que avanzó si un paro lo cortó)
}

/* giro manual para calibrar (solo con el actuador quieto; no cambia la posición registrada) */
void actGiroPrueba(long ms){
  if (actuadorOcupado() || ms == 0) return;
  actGirar(ms > 0 ? 1 : -1);
  actGiroFin = millis() + (unsigned long)labs(ms);
  actGiroManual = true;
}

// Llamar continuamente desde loop(), sin delay()
void actualizarActuador() {
  unsigned long ahora = millis();
  if (actGiroManual && (long)(ahora - actGiroFin) >= 0) { actGirar(0); actGiroManual = false; }
  switch (actEstado) {
    case ACT_INICIO:                         // al prender: termina de extenderse
      if (ahora - actT0 >= T_ACT_IDA_MS) { actGirar(0); actEstado = ACT_REPOSO; if (actPendiente) { actPendiente = false; dispararActuador(); } }
      break;
    case ACT_HACIA_ACTIVO:                   // retrayendo
      if (ahora - actT0 >= T_ACT_REGRESO_MS) {
        actGirar(0); actEstado = ACT_SOSTENIDO;          // retraído: se para y SE QUEDA así
        if (actPendMerma) { actPendMerma = false; actuadorAMerma(); }     // mientras se retraía llegó uno sin tapa
      }
      break;
    case ACT_SOSTENIDO:                      // RETRAÍDO: se queda así hasta que llegue un vaso SIN tapa
      break;
    case ACT_REGRESANDO:
      if (ahora - actT0 >= actRegresoMs) { actGirar(0); actEstado = ACT_REPOSO; if (actPendiente) { actPendiente = false; dispararActuador(); } }
      break;
    default: break;
  }
}

/* Servo 360° de la Estación 2: se mueve con PWM (librería Servo), no con digitalWrite */
void iniciarServoTapa(){
  if(!servoTapa.attached()) servoTapa.attach(PIN_SERVO_TAPA, 1000, 2000);
  servoTapa.writeMicroseconds(servoTapaUs);
  servoActivo = true;
  tapaIndLibre = false;            // exige ver I2 libre antes de aceptar la detección
  tapaPaso = false; tapaRapCont = 0;
}
/* v13: se llama en CADA vuelta del loop. En cuanto I2 detecta la tapa (después de haberse visto libre),
   el servo se detiene de inmediato, sin esperar al ciclo de 20 ms. */
void vigilarTapaRapido(){
  if(!servoActivo) return;
  if(!sensorTapaIndRaw()){ tapaIndLibre = true; tapaRapCont = 0; return; }
  if(!tapaIndLibre) return;                         // la tapa anterior seguía frente al sensor: espera a que se libere
  if(++tapaRapCont >= TAPA_IND_LECTURAS){ detenerServoTapa(); tapaPaso = true; }
}
void detenerServoTapa(){
  if(servoTapa.attached()) servoTapa.detach();
  pinMode(PIN_SERVO_TAPA, OUTPUT);
  digitalWrite(PIN_SERVO_TAPA, LOW);
  servoActivo = false;
}
void vigilarServoForzado(){          // prueba manual FORZ:SV1:1 / TAPA_PRUEBA (se detiene con I2 o por tiempo)
  if(forzServoFin == 0) return;
  if(!servoActivo || tapaPaso || (long)(millis() - forzServoFin) >= 0){
    bool porTapa = tapaPaso;
    detenerServoTapa(); forzServoFin = 0;
    Serial.println(porTapa ? F("TAPA_PRUEBA,0,tapa") : F("TAPA_PRUEBA,0,tiempo"));   // el HMI dice por qué se paró
  }
}

float leerCelda(){ return pesoActual; }
void  tarar(){ pesoActual = 0; }
void  calibrar(float /*gramosPatron*/){ }

/* ============================================================================
   ORIFICIOS Y ESTACIONES
   ============================================================================ */
bool hayOrificio(uint8_t est){ return (est & 1) == (estacionA & 1); }

uint8_t pinValvula(uint8_t v){ return v == 1 ? PIN_V1 : (v == 2 ? PIN_V2 : PIN_V3); }

void limpiarVasos(){
  for(uint8_t s = 0; s < 4; s++){ vaso[s] = Vaso(); okLatch[s] = false; resetSensor(s); }
  okLiberado = false;
  tEsperaF0 = T_ESPERA_CARGA_MS;
  tParada = T_PARADA_MS;
  tCarga = tLlenado = tTapado = -1;
  valvulaActiva = 0;
  digitalWrite(PIN_V1, LOW); digitalWrite(PIN_V2, LOW); digitalWrite(PIN_V3, LOW);
  detenerServoTapa(); forzServoFin = 0;
  faseTapa = 0; tapaE2Hecha = false; tapaForzada = false;
}

/* ============================================================================
   MOVIMIENTO DEL MOTOR
   ============================================================================ */
void iniciarMovimientoPasos(long pasos, int8_t dir, uint16_t stepUs){
  if(pasos <= 0){ motorEnMovimiento = false; return; }
  dirActual = dir;
  bool dirFisica = (dir > 0);
  if(DIR_INVERTIDA) dirFisica = !dirFisica;
  digitalWrite(PIN_DIR, dirFisica ? HIGH : LOW);
  habilitarMotor(true);
  pasosRestantes    = pasos;
  stepUsActual      = stepUs;
  movTotal          = pasos;
  motorEnMovimiento = true;
}

uint16_t stepUsEnRampa(){
  long hechos = movTotal - pasosRestantes;
  long k = min(hechos, pasosRestantes);
  if(k >= RAMPA_PASOS || stepUsActual >= STEP_US_ARRANQUE) return stepUsActual;
  return stepUsActual + (uint16_t)(((long)(STEP_US_ARRANQUE - stepUsActual) * (RAMPA_PASOS - k)) / RAMPA_PASOS);
}

void actualizarPulsos(){
  if(!motorEnMovimiento) return;
  unsigned long ahora = micros();
  if(ahora - lastPulseMicros < stepUsEnRampa()) return;
  lastPulseMicros = ahora;
  pulseLevel = !pulseLevel;
  digitalWrite(PIN_PUL, pulseLevel);
  if(!pulseLevel) return;
  posPasos = ((posPasos + dirActual) % PASOS_REV + PASOS_REV) % PASOS_REV;
  if(--pasosRestantes <= 0) motorEnMovimiento = false;
  if(estado == ST_HOME && sensorHome()) finalizarHome();
}

long pasosHastaGrado(float grados){
  long destino = gradosAPasos(grados) % PASOS_REV;
  return ((destino - posPasos) % PASOS_REV + PASOS_REV) % PASOS_REV;
}
/* pasos hacia adelante desde la posición actual hasta la posición calibrada de la estación s */
long pasosHastaEstacion(uint8_t s){
  return ((pasosEstacion[s] - posPasos) % PASOS_REV + PASOS_REV) % PASOS_REV;
}

/* ============================================================================
   HOMING
   ============================================================================ */
void iniciarHome(){
  contando = false;
  estado = ST_HOME;
  homed = false; pausaPendiente = false; indexManual = false;
  limpiarVasos();
  iniciarMovimientoPasos(PASOS_REV * 2, 1, STEP_US_HOME);
}

void finalizarHome(){
  motorEnMovimiento = false; pasosRestantes = 0;
  posPasos  = gradosAPasos(stGrados[0]) % PASOS_REV;
  estacionA = 0;
  homed     = true;
  pulsoHome = true;
  estado    = inicioPendiente ? ST_RUN : ST_IDLE;
  inicioPendiente = false;
}

/* ============================================================================
   TAREAS POR ESTACIÓN Y CONFIRMACIÓN
   ============================================================================ */
void resetSensor(uint8_t s){ sensorVisto[s] = false; esperaVencida[s] = false; tSens[s] = T_SENSOR_MS; }

void detectarEstaciones(long dt){
  for(uint8_t s = 1; s < 4; s++){
    if(!hayOrificio(s) || sensorVisto[s]) continue;
    /* Lectura directa del pin de la estación: E1 = Pin 8 (F1), E2 = Pin 11 (F2), E3 = Pin 10 (S3).
       Si hay recipiente frente al sensor se memoriza (sensorVisto) hasta el próximo indexado. */
    bool det = (s == 1) ? sensorF1() : (s == 2) ? sensorF2() : sensorS3();
    if(det){ sensorVisto[s] = true; if(s == 2 && vaso[s].presente) vaso[s].real = true; continue; }   // pin 11 = memoria del vaso
    /* solo para un vaso que la máquina cargó: plazo para que el sensor lo confirme */
    if(vaso[s].presente && !esperaVencida[s]){
      tSens[s] -= dt;
      if(tSens[s] <= 0) esperaVencida[s] = true;
    }
  }
}

void tareasEstaciones(long dt){
  detectarEstaciones(dt);

  if(hayOrificio(0) && !vaso[0].presente && tEsperaF0 > 0) tEsperaF0 -= dt;
  if(hayOrificio(0) && !vaso[0].presente){
    if(sensorF0()){
      if(tCarga < 0) tCarga = T_CARGA_MS;
      tCarga -= dt;
      if(tCarga <= 0){ vaso[0] = Vaso(); vaso[0].presente = true; vaso[0].cuenta = contando; tCarga = -1; }
    } else {
      tCarga = -1;
    }
  }

  if(hayOrificio(1) && vaso[1].presente && !vaso[1].lleno){
    if(tLlenado < 0){
      valvulaActiva = siguienteValvula;
      digitalWrite(pinValvula(valvulaActiva), HIGH);
      tLlenado = T_LLENADO_MS;
    }
    tLlenado -= dt;
    if(tLlenado <= 0){
      digitalWrite(PIN_V1, LOW); digitalWrite(PIN_V2, LOW); digitalWrite(PIN_V3, LOW);
      vaso[1].lleno = true;
      fValv[valvulaActiva - 1]++;
      valvulaActiva = 0;
      tLlenado = -1;
      siguienteValvula = siguienteValvula % 3 + 1;
    }
  }

  /* TAPADO (E2) AUTOMÁTICO: F2 detecta el envase -> servo gira -> I2 detecta la tapa -> servo se
     detiene -> sellado (T_TAPADO_MS) -> la rueda continúa sola. OK_TAPADO solo es respaldo manual. */
  if(hayOrificio(2) && !tapaE2Hecha && vaso[2].presente && esperaVencida[2] && !sensorVisto[2]) tapaE2Hecha = true;
  if(hayOrificio(2) && !tapaE2Hecha && sensorVisto[2] && (!vaso[2].presente || vaso[2].lleno)){
    if(tapaForzada){                                   // respaldo manual
      detenerServoTapa();
      if(vaso[2].presente) vaso[2].tapa = true;
      tapaE2Hecha = true; faseTapa = 0; tTapado = -1; tapaForzada = false; reportarEvento(0x20);
    } else if(faseTapa == 0){                          // llegada: arranca el servo
      forzServoFin = 0;                                // si había una prueba del HMI, el tapado real manda
      faseTapa = 1; tTapado = (long)T_TAPADO_TIMEOUT_MS;
      if(vaso[2].presente) vaso[2].dispensada = true;   // el dispensador SÍ giró para este vaso
      iniciarServoTapa();
    } else if(faseTapa == 1){
      if(tapaPaso){                                    // I2 vio pasar la tapa: el servo YA se detuvo (vigilarTapaRapido)
        detenerServoTapa();
        faseTapa = 2; tTapado = T_TAPADO_MS;
      } else {
        tTapado -= dt;                                 // TIMEOUT: si I2 nunca detecta la tapa, el servo se para
        if(tTapado <= 0){
          detenerServoTapa();
          tapaE2Hecha = true; faseTapa = 0; tTapado = -1;   // vaso sin tapa -> en E3 sale como rechazo
          reportarEvento(0x20);                             // TAPA_FIN (sin tapa)
          Serial.println("ALERTA,TAPA_TIMEOUT");
        }
      }
    } else if(faseTapa == 2){
      tTapado -= dt;
      if(tTapado <= 0){ if(vaso[2].presente) vaso[2].tapa = true; tapaE2Hecha = true; faseTapa = 0; tTapado = -1; reportarEvento(0x20); }   // TAPA_FIN
    }
  }

  if(!vaso[3].verificado && (!vaso[3].presente || sensorVisto[3] || esperaVencida[3])){
    vaso[3].verificado = true;
    if(vaso[3].presente && sensorVisto[3]){ if(vaso[3].cuenta){ buenos++; Serial.println("CUENTA,BUENO"); } dispararActuador(); reportarVerif(vaso[3].grano, true); }   // dispensado + tapa (pin 10): bueno -> se retrae
    else if(vaso[3].presente){                                   // RECHAZO (sin tapa): se avisa YA, no hasta que la rueda gire
      vaso[3].rech = true;                                       // así el HMI manda al dispensador la reposición enseguida y el vaso
      if(vaso[3].cuenta){ rech++; Serial.println("CUENTA,RECHAZO"); }   // vacío que está en Llenado se sirve (antes se quedaba esperando
      reportarVerif(vaso[3].grano, false);                       // el OK de Llenado y la rueda se paraba)
      actuadorAMerma();                                          // extendido hacia la merma
    }
  }
}

/* Pide OK SOLO si el sensor de esa estación detecta un recipiente:
   E1 (Pin 8) -> OK_LLENADO · E2 (Pin 11) -> OK_TAPADO · E3 (Pin 10) -> OK_VERIF.
   Sensor libre = no pide confirmación y la rueda puede avanzar. */
bool requiereOk(uint8_t s){
  if(s == 0 || !hayOrificio(s)) return false;
  if(s == 2 && modoAuto) return false;     // E2 (Tapado) en automático: sin OK_TAPADO
  if(s == 3 && modoAuto) return false;     // E3 (Verificación) en automático: sin OK_VERIF
  return sensorVisto[s];
}
bool tareaHecha(uint8_t s){
  if(s == 1) return vaso[1].lleno;
  if(s == 2) return vaso[2].tapa || tapaE2Hecha;
  return vaso[3].verificado;
}
/* si la máquina rastrea un vaso ahí, su tarea (llenar/tapar/verificar) debe haber terminado;
   si el recipiente lo puso el operador sin rastreo, decide el operador con el OK */
bool tareaParaOk(uint8_t s){ return !vaso[s].presente || tareaHecha(s); }
bool estacionLista(uint8_t s){
  if(!hayOrificio(s)) return true;
  if(s == 0) return vaso[0].presente || (!sensorF0() && tEsperaF0 <= 0);
  if(vaso[s].presente && !sensorVisto[s] && !esperaVencida[s]) return false;
  if(vaso[s].presente && !tareaHecha(s)) return false;     // no avanza a mitad de llenado/tapado
  if(s == 2 && sensorVisto[2] && !tapaE2Hecha) return false; // F2 ve un envase: espera a terminar el tapado
  if(s == 3 && sensorVisto[3] && (actEstado == ACT_HACIA_ACTIVO || actEstado == ACT_REGRESANDO || actEstado == ACT_INICIO)) return false; // E3: no suelta el vaso mientras el actuador se mueve
  if(!requiereOk(s)) return true;
  return tareaParaOk(s) && okLatch[s];
}
bool todasListas(){
  if(!agvListo) return false;            // v14: el carro AGV no está en carga: la rueda NO gira (sigue cuando vuelva)
  for(uint8_t s = 0; s < 4; s++) if(!estacionLista(s)) return false;
  return true;
}
bool algunaRequiereOk(){
  for(uint8_t s = 1; s < 4; s++) if(requiereOk(s)) return true;
  return false;
}
bool pipelineVacio(){
  for(uint8_t s = 0; s < 4; s++) if(vaso[s].presente) return false;
  return !sensorF0();
}
bool esperaSensor(){
  if(hayOrificio(0) && !vaso[0].presente && (sensorF0() || tEsperaF0 > 0)) return true;
  for(uint8_t s = 1; s < 4; s++)
    if(hayOrificio(s) && vaso[s].presente && !sensorVisto[s] && !esperaVencida[s]) return true;
  return false;
}

/* Parada en una estación. NUNCA indexa sin permiso:
   - con estaciones que piden OK: cuando todas tienen su tarea hecha y su OK;
   - sin ninguna que pida OK: cuando llega cualquier OK_*.
   Mientras tanto el estado es 9 (o 8 si aún espera a que un sensor confirme). */
void evaluarDwell(long dt){
  tareasEstaciones(dt);
  if(tParada > 0) tParada -= dt;
  /* Indexa cuando: pasó la parada mínima, todas las estaciones terminaron su tarea y
     las que tienen vaso detectado (Llenado, Tapado, Verificación con tapa) ya recibieron su OK.
     La entrada (E0) y las estaciones vacías NO piden OK. */
  if(todasListas() && tParada <= 0){
    if(!modoAuto){
      if(pipelineVacio()){ estado = ST_VACIO; return; }
      estado = ST_ESPERA_PASO; return;
    }
    iniciarIndexado();
    return;
  }
  bool okPendiente = false;
  for(uint8_t s = 1; s < 4; s++) if(requiereOk(s) && tareaParaOk(s) && !okLatch[s]) okPendiente = true;
  if(esperaSensor())      estado = ST_ESPERA_VASO;   // 8: esperando envase o confirmación del sensor
  else if(okPendiente)    estado = ST_ESPERA_OK;     // 9: esperando OK del operador
  else                    estado = ST_RUN;           // 1: parada mínima / tarea en curso
}

/* ============================================================================
   INDEXADO
   ============================================================================ */
void iniciarIndexado(){
  /* pasos exactos desde donde está la rueda hasta la posición CALIBRADA de la estación (N+1) mod 4 */
  uint8_t destino = (estacionA + 1) % 4;
  long pasos = pasosHastaEstacion(destino);
  estado = ST_INDEX;
  iniciarMovimientoPasos(pasos, 1, stepUsIndex);
}

void completarIndexado(){
  if(vaso[3].presente && !vaso[3].rech && !vaso[3].verificado && !sensorVisto[3]){   // sin tapa en E3 (S3) = RECHAZO (si no se decidió antes)
    vaso[3].rech = true;
    if(vaso[3].cuenta){ rech++; Serial.println("CUENTA,RECHAZO"); }
    reportarVerif(vaso[3].grano, false);
  }
  if(vaso[3].presente && vaso[3].rech){
    desviadorActivo = true; tDesv = T_DESV_MS;
    digitalWrite(PIN_DESVIADOR, HIGH);
  }
  if(vaso[3].presente && !vaso[3].rech && vaso[3].grano) reportarEvento(0x40 | vaso[3].grano);   // CAIDA: el vaso bueno cae al carro
  for(int8_t s = 3; s > 0; s--) vaso[s] = vaso[s - 1];
  vaso[0] = Vaso();
  for(uint8_t s = 0; s < 4; s++){ okLatch[s] = false; resetSensor(s); }
  faseTapa = 0; tapaE2Hecha = false; tapaForzada = false;   // nueva parada: el tapado se repite
  okLiberado = false;                     // cada parada exige su propio OK
  tEsperaF0 = T_ESPERA_CARGA_MS;
  tParada = T_PARADA_MS;                  // detenida al menos 1 s en la nueva estación

  estacionA = (estacionA + 1) % 4;
  if(estacionA == 0){ if(contando) pedidosCiclo++; contando = true; }

  if(indexManual){ indexManual = false; estado = ST_IDLE; return; }
  /* el motor queda detenido en la estación esperando OK (estado 9) */
  estado = pausaPendiente ? ST_PAUSE : ST_ESPERA_OK;
  pausaPendiente = false;
}

/* ============================================================================
   COMANDOS
   ============================================================================ */
bool detenidaParaAjustes(){ return estado == ST_IDLE || estado == ST_PAUSE || estado == ST_VACIO || estado == ST_ESPERA_PASO; }

void cmdIniciar(){
  if(estado == ST_HOME){ inicioPendiente = true; return; }
  if(estado == ST_PAUSE){ estado = ST_RUN; return; }
  if(estado == ST_IDLE || estado == ST_VACIO){
    if(!homed){ inicioPendiente = true; iniciarHome(); return; }
    estado = ST_RUN;
  }
}
void cmdPausa(){
  if(estado == ST_RUN || estado == ST_ESPERA_OK || estado == ST_ESPERA_VASO){
    estado = ST_PAUSE;
    detenerServoTapa(); if(faseTapa == 1) faseTapa = 0;   // el servo no gira en pausa
  }
  else if(estado == ST_INDEX) pausaPendiente = true;
}
void cmdParo(){
  inicioPendiente = false;
  estado = ST_ESTOP;
  motorEnMovimiento = false; pasosRestantes = 0;
  habilitarMotor(false);
  pausaPendiente = false; indexManual = false;
  limpiarVasos();
  digitalWrite(PIN_DESVIADOR, LOW); desviadorActivo = false; tDesv = -1;
  valvulaActiva = 0;
  { bool estaba = motorDcOn; motorDc(false); vibFinHasta = 0; okLlenadoPend = false; vibManual = false; if(estaba){ vibCausa = 4; enviarVib(); } }
  actGirar(0); actGiroManual = false;            // el actuador se detiene donde esté
  if(actEstado == ACT_HACIA_ACTIVO){                              // quedó a medio camino: regresará solo lo que avanzó
    actRegresoMs = (unsigned long)((millis() - actT0) * (float)T_ACT_IDA_MS / T_ACT_REGRESO_MS);
    actEstado = ACT_SOSTENIDO;
  }
  else if(actEstado == ACT_REGRESANDO || actEstado == ACT_INICIO) actEstado = ACT_REPOSO;
}
void cmdReset(){
  if(estado != ST_ESTOP) return;
  homed = false;
  estado = ST_IDLE;
  limpiarVasos();
}
void cmdPaso(){
  if(estado == ST_ESPERA_PASO){ iniciarIndexado(); return; }
  if(detenidaParaAjustes() && homed){ indexManual = true; iniciarIndexado(); }
}
void cmdModo(bool automatico){
  modoAuto = automatico;
  if(automatico && estado == ST_ESPERA_PASO) estado = ST_RUN;
}
/* OK_LLENADO (1) · OK_TAPADO (2) · OK_VERIF (3): confirma esa estación y libera la parada */
void cmdOk(uint8_t s){
  if(estado == ST_RUN || estado == ST_ESPERA_OK || estado == ST_ESPERA_VASO || estado == ST_PAUSE){
    if(s == 2 && hayOrificio(2) && sensorVisto[2] && !tapaE2Hecha && modoAuto) tapaForzada = true;   // respaldo manual
    if(requiereOk(s)) okLatch[s] = true;   // solo vale el OK de una estación que de verdad lo está pidiendo
  }
}

void cmdVelocidad(int v){
  if(USAR_POT_FISICO) return;
  velocidad   = (uint8_t)constrain(v, 10, 100);
  stepUsIndex = stepUsDesdeVel(velocidad);
}

float cuantizaGrado(float g){
  g = fmod(g, 360.0f); if(g < 0) g += 360.0f;
  return (float)round(g * PASOS_REV / 360.0) * 360.0f / PASOS_REV;
}
void enviarGrados(){
  Serial.print("GRADOS,");
  for(uint8_t i = 0; i < 4; i++){ Serial.print((long)round(stGrados[i] * 100.0)); Serial.print(","); }
  Serial.println(PASOS_REV);
}
/* Cada estación guarda SU PROPIO ángulo calibrado (sin tocar las demás) y actualiza pasosEstacion[]. */
void fijaEstacion(uint8_t idx, float grados){
  stGrados[idx] = cuantizaGrado(grados);
  recalcularPasos();
}
void cmdSetSt(uint8_t idx, float grados){
  if(idx > 3) return;
  fijaEstacion(idx, grados);
  guardarCalibracion();
  enviarGrados();
}

void iniciarPosicionado(long pasos, int8_t dir, int8_t destEst){
  if(pasos <= 0){
    if(destEst >= 0) estacionA = destEst;
    else homed = false;
    return;
  }
  estadoAntesPos = estado;
  estado = ST_POS;
  posDestinoEst = destEst;
  if(destEst < 0) homed = false;
  iniciarMovimientoPasos(pasos, dir, stepUsIndex);
}
void cmdJog(float grados){
  if(!detenidaParaAjustes() || grados == 0) return;
  iniciarPosicionado(labs(gradosAPasos(grados)), grados > 0 ? 1 : -1, -1);
}
void cmdGotoSt(uint8_t idx){
  if(idx > 3 || !detenidaParaAjustes()) return;
  iniciarPosicionado(pasosHastaEstacion(idx), 1, (int8_t)idx);
}
void cmdGotoDeg(uint8_t idx, float grados){
  if(idx > 3 || !detenidaParaAjustes()) return;
  fijaEstacion(idx, grados);
  guardarCalibracion();
  enviarGrados();
  iniciarPosicionado(pasosHastaEstacion(idx), 1, (int8_t)idx);
}

void cmdForzar(const char* tag, bool on){
  if(!detenidaParaAjustes()) return;
  if(!strcmp(tag,"V1"))       digitalWrite(PIN_V1, on ? HIGH : LOW);
  else if(!strcmp(tag,"V2"))  digitalWrite(PIN_V2, on ? HIGH : LOW);
  else if(!strcmp(tag,"V3"))  digitalWrite(PIN_V3, on ? HIGH : LOW);
  else if(!strcmp(tag,"SV1")){
    if(on){ iniciarServoTapa(); forzServoFin = millis() + T_TAPADO_TIMEOUT_MS; }
    else  { detenerServoTapa(); forzServoFin = 0; }
  }
  else if(!strcmp(tag,"DV1")) digitalWrite(PIN_DESVIADOR, on ? HIGH : LOW);
}

/* ---------------------------- Parser de línea ---------------------------- */
void procesarLinea(char* l){
  int n = strlen(l);
  while(n > 0 && (l[n-1] == '\r' || l[n-1] == ' ')) l[--n] = 0;
  if(n == 0) return;

  if(!strcmp(l,"INICIAR"))         cmdIniciar();
  else if(!strcmp(l,"PAUSA"))      { if(estado == ST_PAUSE) estado = ST_RUN; else cmdPausa(); }
  else if(!strcmp(l,"PARO"))       cmdParo();
  else if(!strcmp(l,"RESET"))      cmdReset();
  else if(!strcmp(l,"HOME"))       { inicioPendiente = false; if(detenidaParaAjustes()) iniciarHome(); }
  else if(!strcmp(l,"PASO"))       cmdPaso();
  else if(!strcmp(l,"MODO:AUTO"))   cmdModo(true);
  else if(!strcmp(l,"MODO:MANUAL")) cmdModo(false);
  else if(!strcmp(l,"OK_LLENADO")) { if(!vibManual) motorDc(false); motorDcHecho = true; vibFinHasta = 0; okLlenadoPend = false; cmdOk(1); }
  else if(!strcmp(l,"VIB:1"))      { if(estado != ST_ESTOP && !vibManual){ motorDc(true); vibFinHasta = 0; } enviarVib(); }   // empieza a dispensar
  else if(!strcmp(l,"VIB:0"))      { if(!vibManual){ motorDc(false); vibFinHasta = 0; vibCausa = 0; } enviarVib(); }
  else if(!strcmp(l,"VIB_MAN:1"))  { if(estado != ST_ESTOP){ vibManual = true; vibFinHasta = 0; motorDcT0 = millis(); motorDc(true); vibCausa = 0; } enviarVib(); }   // BOTÓN del HMI
  else if(!strcmp(l,"VIB_MAN:0"))  { vibManual = false; vibFinHasta = 0; motorDc(false); vibCausa = 0; enviarVib(); }
  else if(!strcmp(l,"VIB?"))       enviarVib();
  else if(!strcmp(l,"VIB_POL:1") || !strcmp(l,"VIB_POL:0")){       // con qué señal enciende el relé/driver
    vibAlto = (l[8] == '1'); guardarVib();
    motorDc(motorDcOn);                         // aplica la nueva lógica al estado actual
    enviarVib();
  }
  else if(!strcmp(l,"AGV:1"))      agvListo = true;
  else if(!strcmp(l,"AGV:0"))      agvListo = false;
  else if(!strcmp(l,"DISP_FIN"))   cmdDispFin(0);
  else if(!strncmp(l,"DISP_FIN:",9)) cmdDispFin((uint8_t)atoi(l + 9));
  else if(!strcmp(l,"OK_TAPADO"))  cmdOk(2);
  else if(!strcmp(l,"OK_VERIF"))   cmdOk(3);
  else if(!strcmp(l,"ESTADO?"))    enviarEstado();
  else if(!strcmp(l,"?"))          { while(txLen) drenarTx(); Serial.println(F("READY")); e2Pendiente = true; }
  else if(!strcmp(l,"E2?"))        e2Pendiente = true;
  else if(!strcmp(l,"RECHAZOS_RESET")) rech = 0;
  else if(!strcmp(l,"BUENOS_RESET")) buenos = 0;
  else if(!strcmp(l,"GRADOS?"))    enviarGrados();
  else if(!strcmp(l,"GRADOS_DEF")){ grados_por_defecto(); recalcularPasos(); guardarCalibracion(); enviarGrados(); }
  else if(!strcmp(l,"POS?")){
    Serial.print("POS,"); Serial.print((long)(posPasos * 36000L / PASOS_REV));
    Serial.print(","); Serial.print(estacionA); Serial.print(","); Serial.println(homed ? 1 : 0);
  }
  else if(!strcmp(l,"E3?")){ Serial.print("E3,visto="); Serial.print(sensorVisto[3]); Serial.print(",pres="); Serial.print(vaso[3].presente); Serial.print(",real="); Serial.print(vaso[3].real); Serial.print(",disp="); Serial.print(vaso[3].dispensada); Serial.print(",verif="); Serial.print(vaso[3].verificado); Serial.print(",S3="); Serial.print(sensorS3()); Serial.print(",buenos="); Serial.print(buenos); Serial.print(",rech="); Serial.print(rech); Serial.print(",act="); Serial.println((int)actEstado); }
  else if(!strcmp(l,"ACT?")){ Serial.print("ACT,"); Serial.print((int)actEstado); Serial.print(","); Serial.println(sensorS3() ? 1 : 0); }
  else if(!strcmp(l,"ACT_RET")){                       // probar RETRAER (solo si está extendido)
    if(actEstado == ACT_REPOSO){ dispararActuador(); Serial.println(F("ACT,regresar")); }
    else Serial.println(actEstado == ACT_SOSTENIDO ? F("ACT,ya_retraido") : F("ACT,ocupado"));
  }
  else if(!strcmp(l,"ACT_EXT")){                       // probar EXTENDER (solo si está retraído)
    if(actEstado == ACT_SOSTENIDO){ actuadorAMerma(); Serial.println(F("ACT,extender")); }
    else Serial.println(actEstado == ACT_REPOSO ? F("ACT,ya_extendido") : F("ACT,ocupado"));
  }
  else if(!strcmp(l,"ACT_POS_RET")){ actGirar(0); actGiroManual = false; actRegresoMs = T_ACT_IDA_MS; actEstado = ACT_SOSTENIDO; Serial.println(F("ACT,pos_retraido")); }   // "lo puse a mano retraído"
  else if(!strncmp(l,"ACT_GIRO:",9)){ actGiroPrueba(atol(l + 9)); Serial.println("ACT,giro"); }
  else if(!strcmp(l,"ACT_T?")) enviarActuador();
  else if(!strncmp(l,"ACT_T:",6)){
    long ida = 0, reg = 0, vel = 0, sen = 0;
    if(sscanf(l + 6, "%ld,%ld,%ld,%ld", &ida, &reg, &vel, &sen) == 4 && ida >= 50 && ida <= 20000 && reg >= 50 && reg <= 20000
       && vel >= 10 && vel <= 90 && (sen == 1 || sen == -1) && !actuadorOcupado()){
      T_ACT_IDA_MS = ida; T_ACT_REGRESO_MS = reg; ACT_VELOCIDAD_US = vel; ACT_SENTIDO = sen;
      guardarActuador(); enviarActuador();
    } else Serial.println(F("ERR,ACT_T"));
  }
  else if(!strcmp(l,"ACT_STOP")){ actGirar(0); actGiroManual = false;
    if(actEstado == ACT_HACIA_ACTIVO) actEstado = ACT_SOSTENIDO; else if(actEstado == ACT_REGRESANDO || actEstado == ACT_INICIO) actEstado = ACT_REPOSO;
    Serial.println("ACT,paro"); }
  else if(!strcmp(l,"MOTOR?")){ Serial.print(F("MOTOR_DC,")); Serial.println(motorDcOn ? 1 : 0); }
  else if(!strcmp(l,"TAPA_V?")) enviarServoTapa();
  else if(!strcmp(l,"TAPA_PRUEBA")){                 // botón "Probar giro" del HMI: funciona también en automático
    if(estado == ST_ESTOP) Serial.println(F("ERR,TAPA_PRUEBA,paro"));
    else if(faseTapa != 0) Serial.println(F("ERR,TAPA_PRUEBA,tapando"));   // está poniendo una tapa: no se interrumpe
    else { iniciarServoTapa(); forzServoFin = millis() + T_TAPADO_TIMEOUT_MS; Serial.println(F("TAPA_PRUEBA,1")); }
  }
  else if(!strcmp(l,"TAPA_PRUEBA:0")){ if(forzServoFin){ detenerServoTapa(); forzServoFin = 0; } Serial.println(F("TAPA_PRUEBA,0,boton")); }
  else if(!strncmp(l,"TAPA_V:",7)){                  // velocidad del servo de tapas (µs): se guarda en la EEPROM
    long v = atol(l + 7);
    if(tapaUsValido(v)){ servoTapaUs = (uint16_t)v; guardarServoTapa(); if(servoActivo && servoTapa.attached()) servoTapa.writeMicroseconds(servoTapaUs); enviarServoTapa(); }
    else Serial.println(F("ERR,TAPA_V"));
  }
  else if(!strcmp(l,"TAPA?")){ Serial.print("TAPA,pin="); Serial.print(digitalRead(PIN_TAPA_IND)); Serial.print(",detecta="); Serial.print(sensorTapaIndRaw() ? 1 : 0); Serial.print(",paso="); Serial.print(tapaPaso ? 1 : 0); Serial.print(",servo="); Serial.println(servoActivo ? 1 : 0); }
  else if(!strcmp(l,"AGV?")){ Serial.print("AGV,"); Serial.println(agvListo ? 1 : 0); }
  else if(!strcmp(l,"TARA"))       tarar();
  else if(!strncmp(l,"CAL:",4))    calibrar(atof(l + 4));
  else if(!strncmp(l,"SPEED:",6))  cmdVelocidad(atoi(l + 6));
  else if(!strncmp(l,"SET_ST",6) && strchr(l,':')){
    uint8_t idx = (uint8_t)(l[6] - '0');
    cmdSetSt(idx, atof(strchr(l,':') + 1));
  }
  else if(!strncmp(l,"JOG:",4))      cmdJog(atof(l + 4));
  else if(!strncmp(l,"GOTO_ST:",8))  cmdGotoSt((uint8_t)atoi(l + 8));
  else if(!strncmp(l,"GOTO_DEG:",9)){
    char* p2 = strchr(l + 9, ':');
    if(p2) cmdGotoDeg((uint8_t)atoi(l + 9), atof(p2 + 1));
  }
  else if(!strncmp(l,"FORZ:",5)){
    char tag[8]; char* p1 = l + 5; char* p2 = strchr(p1, ':');
    if(p2){
      uint8_t len = min((int)(p2 - p1), 7);
      memcpy(tag, p1, len); tag[len] = 0;
      cmdForzar(tag, atoi(p2 + 1) != 0);
    }
  }
}

void leerSerieNoBloqueante(){
  while(Serial.available()){
    char c = Serial.read();
    if(c == '\n'){
      rxBuf[rxLen] = 0;
      procesarLinea(rxBuf);
      rxLen = 0;
    } else if(rxLen < BUF_LEN - 1){
      rxBuf[rxLen++] = c;
    } else {
      rxLen = 0;
    }
  }
}

/* ============================================================================
   TELEMETRÍA
   ============================================================================ */
char cupChar(uint8_t s){
  if(!hayOrificio(s) || !vaso[s].presente) return '-';
  if(!vaso[s].lleno) return 'E';
  if(!vaso[s].tapa)  return 'F';
  return 'X';
}

/* Arma la trama GIN en txBuf (si el envío anterior ya terminó). loop() la vacía sin bloquear. */
void enviarEstado(){
  if(txLen != 0) return;                         // la trama anterior aún se está enviando
  char cups[5], sens[5];
  for(uint8_t s = 0; s < 4; s++) cups[s] = cupChar(s);
  cups[4] = 0;
  sens[0] = sensorF0() ? '1' : '0';
  sens[1] = sensorF1() ? '1' : '0';
  sens[2] = sensorF2() ? '1' : '0';
  sens[3] = sensorS3() ? '1' : '0';
  sens[4] = 0;

  pesoActual = leerCelda();

  int n = snprintf(txBuf, sizeof(txBuf),
      "GIN,%d,%u,%s,%s,%d,%u,%d,%d,%lu,%lu,%lu,%lu,%lu,%lu,%d,%d\n",
      (int)estado, (unsigned)estacionA, cups, sens, (int)round(pesoActual * 10),
      (unsigned)valvulaActiva, servoActivo ? 1 : 0, desviadorActivo ? 1 : 0,
      pedidosCiclo, buenos, rech, fValv[0], fValv[1], fValv[2],
      pulsoHome ? 1 : 0, homed ? 1 : 0);
  if(n <= 0 || n >= (int)sizeof(txBuf)) return;  // no cupo: se descarta (nunca ocurre con estos campos)
  txLen = (uint8_t)n; txPos = 0;
  if(pulsoHome) pulsoHome = false;               // el pulso HOME dura una sola trama
}

/* Sensor E2: antirrebote del pin 8 y aviso de cada cambio estable */
void vigilarE2(){
  bool lectura = sensorF1();
  if(lectura != e2Candidato){ e2Candidato = lectura; e2TCambio = millis(); }
  // con el vibrador prendido la rueda está quieta: un "ya no hay vaso" corto es parpadeo por la vibración
  if(e2Candidato != e2Estado && millis() - e2TCambio >= ((!e2Candidato && motorDcOn && !vibManual) ? E2_SALIDA_MS : E2_ANTIRREBOTE_MS)){
    e2Estado = e2Candidato;
    e2Pendiente = true;
    if(!e2Estado){                                  // el vaso se fue: vibrador apagado y listo para el siguiente
      if(vibFinHasta == 0 && motorDcOn && !vibManual){ motorDc(false); vibCausa = 1; enviarVib(); }   // (no si está en la vibración extra ni si lo prendió el botón)
      motorDcHecho = false;
    }                                               // v13: al LLEGAR el vaso ya no vibra; vibra con VIB:1 (al dispensar)
  }
  if(vibFinHasta != 0 && (long)(millis() - vibFinHasta) >= 0){   // terminó la vibración extra
    vibFinHasta = 0;
    if(!vibManual){ motorDc(false); vibCausa = 3; enviarVib(); }
    if(okLlenadoPend){ okLlenadoPend = false; cmdOk(1); }        // ahora sí: OK de Llenado, la rueda puede girar
  }
  if(motorDcOn && millis() - motorDcT0 >= (vibManual ? T_VIB_MANUAL_MAX_MS : T_MOTOR_DC_MAX_MS)){   // seguridad por tiempo
    motorDc(false); motorDcHecho = true; vibFinHasta = 0; vibManual = false; vibCausa = 2;
    if(okLlenadoPend){ okLlenadoPend = false; cmdOk(1); }
    Serial.println(F("ALERTA,MOTOR_DC_TIMEOUT"));
    enviarVib();
  }
}
/* DISP_FIN: el dispensador terminó con este vaso -> motor DC OFF y OK de Llenado automático */
void cmdDispFin(uint8_t grano){
  motorDcHecho = true;
  if(hayOrificio(1) && grano >= 1 && grano <= 3){
    if(!vaso[1].presente && sensorVisto[1]){          // vaso puesto a mano (sin rastreo): se empieza a rastrear aquí
      vaso[1] = Vaso(); vaso[1].presente = true; vaso[1].real = true; vaso[1].lleno = true; vaso[1].cuenta = contando;
    }
    if(vaso[1].presente) vaso[1].grano = grano;      // el vaso sabe qué grano lleva hasta Verificación
  }
  if(vibManual){ cmdOk(1); return; }               // lo prendió el botón: sigue vibrando, solo da el OK
  if(motorDcOn && T_VIB_EXTRA_MS > 0){               // v13: sigue vibrando para que caiga lo atorado
    vibFinHasta = millis() + T_VIB_EXTRA_MS;
    if(vibFinHasta == 0) vibFinHasta = 1;
    okLlenadoPend = true;                            // el OK de Llenado se da al terminar de vibrar
    return;
  }
  motorDc(false);
  cmdOk(1);
}
void emitirVerif(){
  if(nVerif == 0 || txLen != 0) return;
  uint8_t v = verifCola[0];
  for(uint8_t i = 1; i < nVerif; i++) verifCola[i - 1] = verifCola[i];
  nVerif--;
  int n;
  if(v == 0x20)        n = snprintf(txBuf, sizeof(txBuf), "TAPA_FIN\n");
  else if(v & 0x40)    n = snprintf(txBuf, sizeof(txBuf), "CAIDA,%d\n", v & 0x0F);
  else                 n = snprintf(txBuf, sizeof(txBuf), "VERIF,%d,%d\n", v & 0x0F, (v & 0x80) ? 1 : 0);
  if(n > 0){ txLen = (uint8_t)n; txPos = 0; }
}
/* Pone "E2,1" / "E2,0" en el búfer de salida solo cuando está libre (no corta tramas GIN) */
void emitirE2(){
  if(!e2Pendiente || txLen != 0) return;
  int n = snprintf(txBuf, sizeof(txBuf), "E2,%d\n", e2Estado ? 1 : 0);
  if(n <= 0) return;
  txLen = (uint8_t)n; txPos = 0;
  e2Pendiente = false;
}

void drenarTx(){
  while(txPos < txLen && Serial.availableForWrite() > 0) Serial.write(txBuf[txPos++]);
  if(txPos >= txLen){ txLen = 0; txPos = 0; }
}

/* ============================================================================
   LOOP PRINCIPAL
   ============================================================================ */
void loop(){
  leerSerieNoBloqueante();
  vigilarTapaRapido();          // v13: la tapa se detecta en cada vuelta (el servo para al instante)
  actualizarPulsos();
  drenarTx();
  vigilarE2();
  emitirE2();
  emitirVerif();
  drenarTx();

  unsigned long ahora = millis();
  if(ahora - lastLogic >= 20){
    long dt = (long)(ahora - lastLogic);
    lastLogic = ahora;

    tapaIndCont = sensorTapaIndRaw() ? (tapaIndCont < 255 ? tapaIndCont + 1 : 255) : 0;   // antirrebote de I2
    vigilarServoForzado();
    actualizarActuador();

    if(USAR_POT_FISICO){
      velocidad   = (uint8_t)map(analogRead(PIN_POT), 0, 1023, 10, 100);
      stepUsIndex = stepUsDesdeVel(velocidad);
    }

    if(tDesv > 0){
      tDesv -= dt;
      if(tDesv <= 0){ desviadorActivo = false; digitalWrite(PIN_DESVIADOR, LOW); }
    }

    switch(estado){
      case ST_HOME:
        if(!motorEnMovimiento){ estado = ST_ESTOP; habilitarMotor(false); }
        break;
      case ST_POS:
        if(!motorEnMovimiento){
          if(posDestinoEst >= 0){ estacionA = (uint8_t)posDestinoEst; posDestinoEst = -1; }
          estado = estadoAntesPos;
        }
        break;
      case ST_INDEX:
        if(!motorEnMovimiento) completarIndexado();
        break;
      case ST_RUN:
      case ST_ESPERA_OK:
      case ST_ESPERA_VASO:
        evaluarDwell(dt);
        break;
      case ST_VACIO:
        if(sensorF0()){
          if(hayOrificio(0)) estado = ST_RUN;
          else if(agvListo) iniciarIndexado();   // v14: no gira sin el carro en carga
        }
        break;
      default:
        break;
    }
  }

  if(ahora - lastEmit >= EMIT_MS){
    lastEmit = ahora;
    enviarEstado();
  }
}
