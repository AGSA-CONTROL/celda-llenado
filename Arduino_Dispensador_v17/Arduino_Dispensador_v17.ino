/* ===================================================================
   NUEVO: SERVOS DE 180° (COMPUERTAS) CALIBRADOS POR GRADOS (todo desde el HMI, Dispensador > Calibrar)
     Cada tolva tiene una compuerta movida por un servo de 180°:
       - ÁNGULO CERRADO: grados donde la compuerta tapa la tolva (no cae grano)
       - ÁNGULO ABIERTO: grados donde la compuerta deja caer el grano
       - TIEMPO ABIERTO: ms que se queda abierta en cada apertura
       - VECES POR PORCIÓN: cuántas veces abre y cierra para llenar un recipiente
     Entre una apertura y la siguiente se espera la PAUSA (igual para las 3).
     Los valores se GUARDAN en la EEPROM: no se pierden al apagarlo.
     Comandos de calibración:
       C?                     lee todo -> ANGULOS,c1,c2,c3,a1,a2,a3,t1,t2,t3,k1,k2,k3,pausa
       C,n,cerr,abie,ms,k     tolva n: cerrado y abierto (0-180), ms abierto (100-10000), veces (1-10). Se guarda.
       CP,ms                  pausa entre aperturas (0-10000 ms). Se guarda.
       M,n,g                  mueve el servo n a g grados YA (para buscar el ángulo)  -> MOV,n,g
       T,n,k                  PRUEBA: abre y cierra k veces con su tiempo           -> PRUEBA,n,k ... PRUEBA_OK,n
       G,n,1 / G,n,0          abre / cierra la compuerta n (botón del HMI)          -> GIRA,n / PARA,n

   DISPENSADOR: 3 SERVOS DE 180° CON COMPUERTA (PEDIDOS DE 3) + 3 SENSORES HC-SR04
   Para usar con HMI_Llenado_v17.html (pestaña Control del Dispensado).   Placa: Arduino UNO / Nano
   Librería: "Servo" (viene con el IDE). Ya NO se usa la celda de carga ni el HX711.
   Guarda este archivo dentro de una carpeta llamada "Arduino_Dispensador_v17".

   CONEXIONES (las mismas de antes, sin el HX711)
     Tolva 1 (frijol):   HC-SR04 TRIG -> D7    ECHO -> D8     Servo 1 señal -> D9
     Tolva 2 (lentejas): HC-SR04 TRIG -> D4    ECHO -> D5     Servo 2 señal -> D10
     Tolva 3 (maíz):     HC-SR04 TRIG -> D6    ECHO -> D12    Servo 3 señal -> D11
     HC-SR04: VCC -> 5V Arduino, GND -> GND.
     Servos: + y - a la fuente EXTERNA de 5 V (con fusible), GND de la fuente unido al GND del Arduino.

   CÓMO DOSIFICA
     Los servos son de 180°: van a un ángulo y se quedan ahí.
     Un pedido es un LOTE de 3 unidades repartidas entre las tolvas (p. ej. 2+1+0).
     Cada unidad = la compuerta de esa tolva se abre (ángulo abierto) su "tiempo abierto" y se
     cierra (ángulo cerrado), tantas veces como diga "veces por porción". Se calibra en el HMI.
     Se atienden en orden: primero las de la tolva 1, luego la 2 y luego la 3.
     Al encender, las 3 compuertas van a su ángulo CERRADO.

   ESTACIÓN 2 (SENSOR E2: hay recipiente)
     La rueda tiene un sensor en la estación 2 que detecta el recipiente. Como aún no está
     conectado, el HMI lo simula con un botón (comando V). Todo pasa por fijarVaso(): cuando
     conectes el sensor real, solo llama a fijarVaso(true/false) desde el loop.
       - El Arduino guarda UN lote "registrado" (S). Lo empieza a dispensar cuando E2 = hay recipiente.
       - Si E2 se suelta a media dispensada, se detiene (queda en ESPERA) y sigue donde se quedó
         cuando E2 vuelve a activarse.
       - Cada pedido (1 unidad) va en un recipiente distinto. Al terminar un pedido el dispensador
         se detiene y espera: hay que soltar E2 (retirar el recipiente que avanza) y volver a
         presionarlo cuando llegue el siguiente. Así se dispensan los 3 pedidos del lote, uno por recipiente.
     La cola de lotes vive en el HMI: en cuanto termina un lote le manda el siguiente.

   COMANDOS (el HMI los manda terminados en salto de línea)
     ?                    ping
     S,p1,p2,p3           registrar un lote: unidades de la tolva 1, 2 y 3 (suman 3; o de 1 a 3 en una REPOSICIÓN de rechazados)
     X                    cancelar el lote registrado (solo si aún no empieza)
     V,1 / V,0            SENSOR E2: 1 = hay recipiente, 0 = no hay (simulado desde el HMI)
     A                    reanudar después de un PARO
     P                    paro        E  emergencia        R  reset de emergencia
     G,n,1                abre la compuerta n (1-3) (solo en reposo; sirve para probar)
     G,n,0                cierra la compuerta n
     L                    cierra las 3 compuertas (solo en reposo)

   RESPUESTAS
     READY, INICIADO, CICLO_OK, DETENIDO, EMERGENCIA, RESET_OK, SOLTADOS
     ARMADO,a,b,c         lote registrado           CANCELADO     lote cancelado
     E2,1 | E2,0          estado del sensor E2      SIN_VASO      dispensado en pausa por falta de recipiente
     QUITAR_VASO          lote terminado: retira el recipiente (suelta E2) antes del siguiente
     SERVO,<n>,LISTO | MOVIENDO | GIRANDO | PARO | ESPERA | EMERGENCIA
                          (MOVIENDO = dosificando, GIRANDO = giro manual, ESPERA = sin recipiente)
     PORCION,<n>,<k>,<total>     empieza la unidad k de total en la tolva n
     CAYENDO,<ms>                cerró la compuerta: espera <ms> a que caiga el producto
     PEDIDO_OK,<k>               pedido k del lote terminado (ya cayó todo): espera el siguiente recipiente
   ESPERA_RECIPIENTE_MS: tiempo que se espera después de que llega el recipiente antes de
   dispensar, para que quede bien colocado y no haya derrame.
     GIRA,<n>   PARA,<n>
     SENSOR,<n>,<cm> | SENSOR,<n>,ERR
     ERR,<motivo>         (OCUPADO, FORMATO, PEDIDO_VACIO, LOTE)
   Sin HMI: Monitor Serie a 115200 baudios, con "Nueva línea" activado.
   =================================================================== */

#include <Servo.h>
#include <EEPROM.h>
#include <string.h>
#include <stdlib.h>

const uint8_t N_TOLVAS = 3;
const uint8_t SERVO_PIN[N_TOLVAS] = {9, 10, 11};
const uint8_t TRIG_PIN[N_TOLVAS]  = {7, 4, 6};
const uint8_t ECHO_PIN[N_TOLVAS]  = {8, 5, 12};
const uint8_t N_SENSORES_ACTIVOS = 3;           // 1 a 3: cuántos HC-SR04 se leen

// ---- Servos de 180° (compuertas) ----
// Valores de fábrica; los reales se calibran en el HMI y se guardan en la EEPROM.
uint8_t  angCerrado[N_TOLVAS]  = {0, 0, 0};          // grados: compuerta cerrada
uint8_t  angAbierto[N_TOLVAS]  = {90, 90, 90};       // grados: compuerta abierta
uint16_t msAbierto[N_TOLVAS]   = {1000, 1000, 1000}; // ms que se queda abierta en cada apertura
uint8_t  vecesPorcion[N_TOLVAS] = {1, 1, 1};         // aperturas por porción (un recipiente)
uint16_t pausaAperturasMs      = 800;                // compuerta cerrada entre una apertura y la siguiente
const uint16_t EE_MAGIC = 0xA180;                    // distinto al de las vueltas: no lee datos viejos
struct CfgGrados { uint16_t magic; uint8_t c[N_TOLVAS]; uint8_t a[N_TOLVAS]; uint16_t ms[N_TOLVAS]; uint8_t k[N_TOLVAS]; uint16_t pausa; };
const unsigned long T_MOVER_MS = 400;                // tiempo que le damos al servo para llegar al ángulo
const unsigned long ESPERA_CAIDA_MS = 3000;     // AJUSTAR: al terminar de girar, espera este tiempo a que termine de caer
                                                // el producto y SOLO ENTONCES avisa PEDIDO_OK / CICLO_OK (la rueda avanza)
const unsigned long ESPERA_RECIPIENTE_MS = 1500; // al llegar un recipiente, espera este tiempo antes de dispensar (evita derrame)
/* Un recipiente YA LLENO solo cuenta como retirado si el sensor deja de verlo este tiempo seguido.
   Si "regresa" antes (parpadeo por la vibración) es el MISMO recipiente: no se le sirve otra porción. */
const unsigned long VASO_RETIRADO_MS = 250;
unsigned long tSinVaso = 0;                     // desde cuándo E2 dice "no hay recipiente"
const uint8_t LOTE = 3;                         // unidades por pedido

// ---- Sensores ----
const unsigned long INTERVALO_PING_MS   = 40;   // un ping cada 40 ms, rotando entre los sensores
const unsigned long INTERVALO_SENSOR_MS = 300;
const unsigned long ECHO_TIMEOUT_US     = 8000UL;   // ~137 cm
const float DIST_MIN_CM = 2.0;
const float DIST_MAX_CM = 100.0;
const uint8_t N_MUESTRAS = 5;

Servo servos[N_TOLVAS];
enum Estado { IDLE, GIRANDO, ESPERA_GIRO, ENTRE, DETENIDO, SIN_VASO };   // GIRANDO = compuerta abierta · ESPERA_GIRO = cerrada entre aperturas
Estado estado = IDLE;
Estado estadoPrevio = IDLE;
enum EstServo { S_LISTO, S_MOVIENDO, S_GIRA, S_PARO, S_ESPERA, S_EMERG };
EstServo svEst[N_TOLVAS];
uint8_t activo = 0;                             // servo que está dosificando
uint8_t pend[N_TOLVAS]  = {0, 0, 0};            // unidades que faltan por tolva
uint8_t total[N_TOLVAS] = {0, 0, 0};            // unidades pedidas por tolva
unsigned long tIni = 0, tPausa = 0;             // inicio del giro / de la pausa
unsigned long restanteMs = 0;                   // lo que le falta a la apertura en curso
uint8_t giroN = 0;                              // número de apertura en curso dentro de la unidad
bool emergencia = false;
bool vaso = false;                              // SENSOR E2: hay recipiente en la estación 2
bool armado = false;                            // hay un lote registrado (esperando o en curso)
unsigned long tVaso = 0;                        // cuándo llegó el recipiente actual (SENSOR E2 pasó a 1)
bool vasoArmado = false;                        // E2 se presionó DESPUÉS de registrar el lote (arranque solo con pulsación nueva)
bool soltarVasoFlag = false;                    // lote terminado: falta retirar el recipiente

float muestras[N_TOLVAS][N_MUESTRAS];
uint8_t idxM[N_TOLVAS];
uint8_t turno = 0;
unsigned long tPing = 0, tSensor = 0;

char linea[32];

// ---- movimientos manuales (prueba de calibración) ----
int8_t   manS = -1;                         // servo en prueba, -1 = ninguno
uint8_t  manVeces = 0, manHechas = 0;       // aperturas de la prueba
bool     manAbierta = false;
unsigned long manT = 0;
uint8_t nlin = 0;


// ===================== SENSORES =====================
float medirUnaVez(uint8_t s) {                  // cm, o -1 si no hubo eco / fuera de rango
  digitalWrite(TRIG_PIN[s], LOW);
  delayMicroseconds(4);
  digitalWrite(TRIG_PIN[s], HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN[s], LOW);
  unsigned long us = pulseIn(ECHO_PIN[s], HIGH, ECHO_TIMEOUT_US);
  if (us == 0) return -1.0;
  float cm = us / 58.0;
  if (cm < DIST_MIN_CM || cm > DIST_MAX_CM) return -1.0;
  return cm;
}

void muestrear() {                              // un sensor por turno
  if (millis() - tPing < INTERVALO_PING_MS) return;
  tPing = millis();
  muestras[turno][idxM[turno]] = medirUnaVez(turno);
  idxM[turno] = (idxM[turno] + 1) % N_MUESTRAS;
  turno = (turno + 1) % N_SENSORES_ACTIVOS;
}

// Cada 300 ms manda, por cada tolva, la MEDIANA de sus últimas 5 lecturas.
void reportarSensores() {
  if (millis() - tSensor < INTERVALO_SENSOR_MS) return;
  tSensor = millis();
  for (uint8_t s = 0; s < N_SENSORES_ACTIVOS; s++) {
    float v[N_MUESTRAS];
    uint8_t n = 0;
    for (uint8_t i = 0; i < N_MUESTRAS; i++)
      if (muestras[s][i] > 0) v[n++] = muestras[s][i];

    Serial.print(F("SENSOR,"));
    Serial.print(s + 1);
    Serial.print(',');
    if (n < 3) { Serial.println(F("ERR")); continue; }
    for (uint8_t i = 1; i < n; i++) {
      float k = v[i];
      int j = i - 1;
      while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; }
      v[j + 1] = k;
    }
    Serial.println(v[n / 2], 1);
  }
}

// ===================== SERVOS DE 180° =====================
void moverServo(uint8_t s, uint8_t g) {         // va al ángulo g y se queda ahí (sostiene la posición)
  if (!servos[s].attached()) servos[s].attach(SERVO_PIN[s]);   // igual que el código de prueba: write(0) = cerrado
  servos[s].write(g);
}
void abrirServo(uint8_t s)  { moverServo(s, angAbierto[s]); }
void cerrarServo(uint8_t s) { moverServo(s, angCerrado[s]); }
void girarServo(uint8_t s)  { abrirServo(s); }  // (nombres de antes: "girar" = abrir, "soltar" = cerrar)
void soltarServo(uint8_t s) { cerrarServo(s); }

// ===================== CALIBRACIÓN POR GRADOS (EEPROM) =====================
void cargarGrados() {
  CfgGrados c;
  EEPROM.get(0, c);
  if (c.magic != EE_MAGIC) return;              // nunca se ha guardado: quedan los valores de fábrica
  for (uint8_t s = 0; s < N_TOLVAS; s++) {
    if (c.c[s] <= 180) angCerrado[s] = c.c[s];
    if (c.a[s] <= 180) angAbierto[s] = c.a[s];
    if (c.ms[s] >= 100 && c.ms[s] <= 10000) msAbierto[s] = c.ms[s];
    if (c.k[s] >= 1 && c.k[s] <= 10) vecesPorcion[s] = c.k[s];
  }
  if (c.pausa <= 10000) pausaAperturasMs = c.pausa;
}
void guardarGrados() {
  CfgGrados c;
  c.magic = EE_MAGIC;
  for (uint8_t s = 0; s < N_TOLVAS; s++) { c.c[s] = angCerrado[s]; c.a[s] = angAbierto[s]; c.ms[s] = msAbierto[s]; c.k[s] = vecesPorcion[s]; }
  c.pausa = pausaAperturasMs;
  EEPROM.put(0, c);                             // put solo escribe los bytes que cambiaron
}
void reportarGrados() {
  Serial.print(F("ANGULOS"));
  for (uint8_t s = 0; s < N_TOLVAS; s++) { Serial.print(','); Serial.print(angCerrado[s]); }
  for (uint8_t s = 0; s < N_TOLVAS; s++) { Serial.print(','); Serial.print(angAbierto[s]); }
  for (uint8_t s = 0; s < N_TOLVAS; s++) { Serial.print(','); Serial.print(msAbierto[s]); }
  for (uint8_t s = 0; s < N_TOLVAS; s++) { Serial.print(','); Serial.print(vecesPorcion[s]); }
  Serial.print(','); Serial.println(pausaAperturasMs);
}
bool libreManual() {                            // los movimientos manuales solo en reposo
  if (emergencia || estado != IDLE) { Serial.println(F("ERR,OCUPADO")); return false; }
  return true;
}
void cortarPrueba() {
  if (manS < 0) return;
  uint8_t s = manS; manS = -1;
  cerrarServo(s); svEst[s] = S_LISTO; avisoServo(s);
}
void iniciarPrueba(uint8_t s, uint8_t k) {      // abre y cierra k veces
  cortarPrueba();
  manS = s; manVeces = k; manHechas = 0; manAbierta = true; manT = millis();
  abrirServo(s);
  svEst[s] = S_GIRA; avisoServo(s);
}
void actualizarManual() {
  if (manS < 0) return;
  uint8_t s = manS;
  if (manAbierta) {
    if (millis() - manT < msAbierto[s]) return;
    cerrarServo(s); manAbierta = false; manHechas++; manT = millis();
    if (manHechas >= manVeces) {
      manS = -1; svEst[s] = S_LISTO; avisoServo(s);
      Serial.print(F("PRUEBA_OK,")); Serial.println(s + 1);
    }
  } else if (millis() - manT >= pausaAperturasMs) {
    abrirServo(s); manAbierta = true; manT = millis();
  }
}

void avisoServo(uint8_t s) {
  Serial.print(F("SERVO,"));
  Serial.print(s + 1);
  Serial.print(',');
  switch (svEst[s]) {
    case S_LISTO:    Serial.println(F("LISTO"));      break;
    case S_MOVIENDO: Serial.println(F("MOVIENDO"));   break;
    case S_GIRA:     Serial.println(F("GIRANDO"));    break;
    case S_PARO:     Serial.println(F("PARO"));       break;
    case S_ESPERA:   Serial.println(F("ESPERA"));     break;
    case S_EMERG:    Serial.println(F("EMERGENCIA")); break;
  }
}

void avisoTodos() {                             // reenvía el estado real de los 3 servos
  for (uint8_t s = 0; s < N_TOLVAS; s++) avisoServo(s);
}

int siguientePendiente(uint8_t desde) {         // primera tolva con unidades por dar, o -1
  for (uint8_t s = desde; s < N_TOLVAS; s++)
    if (pend[s] > 0) return s;
  return -1;
}

void empezarPorcion() {                         // abre la compuerta para una unidad
  estado = GIRANDO;
  giroN = 1;
  restanteMs = msAbierto[activo];               // tiempo abierto calibrado
  tIni = millis();
  girarServo(activo);
  Serial.print(F("PORCION,"));
  Serial.print(activo + 1);
  Serial.print(',');
  Serial.print((int)(total[activo] - pend[activo] + 1));
  Serial.print(',');
  Serial.println((int)total[activo]);
}

void actualizarServo() {
  switch (estado) {
    case GIRANDO:
      if (millis() - tIni >= restanteMs) {
        soltarServo(activo);
        tPausa = millis();
        estado = (giroN < vecesPorcion[activo]) ? ESPERA_GIRO : ENTRE;   // tras la última apertura pasa a cerrar la porción
        if (estado == ENTRE) { Serial.print(F("CAYENDO,")); Serial.println(ESPERA_CAIDA_MS); }
      }
      break;
    case ESPERA_GIRO:                                          // compuerta cerrada la pausa y luego la siguiente apertura
      if (millis() - tPausa >= pausaAperturasMs) {
        giroN++;
        estado = GIRANDO;
        restanteMs = msAbierto[activo];
        tIni = millis();
        girarServo(activo);
      }
      break;
    case ENTRE:                                                // servo parado: deja caer el producto
      if (millis() - tPausa >= ESPERA_CAIDA_MS) {
        pend[activo]--;                                        // se dispensó 1 pedido (1 recipiente)
        if (siguientePendiente(0) >= 0) {                      // aún quedan pedidos del lote
          svEst[activo] = S_LISTO;
          avisoServo(activo);
          estado = IDLE;                                       // espera el siguiente recipiente
          soltarVasoFlag = vaso;                               // hay que retirar el que ya se llenó
          vasoArmado = false;
          uint8_t hechos = 0;
          for (uint8_t s = 0; s < N_TOLVAS; s++) hechos += total[s] - pend[s];
          Serial.print(F("PEDIDO_OK,")); Serial.println((int)hechos);
        } else {                                               // lote completo (último pedido)
          svEst[activo] = S_LISTO;
          avisoServo(activo);
          estado = IDLE;
          armado = false;
          soltarVasoFlag = vaso;                               // este recipiente ya está lleno
          Serial.println(F("CICLO_OK"));
          if (soltarVasoFlag) Serial.println(F("QUITAR_VASO"));
        }
      }
      break;
    default: break;
  }
}

// ===================== COMANDOS =====================
int leerNumeros(const char *s, long *v, int maximo) {   // "…,12,-3,40" -> v[]; devuelve cuántos
  int n = 0;
  while (*s && n < maximo) {
    if (*s == ',') s++;
    char *fin;
    long x = strtol(s, &fin, 10);
    if (fin == s) break;
    v[n++] = x;
    s = fin;
  }
  return n;
}

void armarLote(const long *p) {                 // registra el lote; se dispensa cuando E2 = hay recipiente
  if (emergencia || armado || estado != IDLE) { Serial.println(F("ERR,OCUPADO")); return; }
  long suma = 0;
  for (uint8_t s = 0; s < N_TOLVAS; s++) {
    if (p[s] < 0) { Serial.println(F("ERR,FORMATO")); return; }
    suma += p[s];
  }
  if (suma == 0) { Serial.println(F("ERR,PEDIDO_VACIO")); return; }
  if (suma > LOTE) { Serial.println(F("ERR,LOTE")); return; }       // lote de 3; las reposiciones de rechazados pueden ser de 1 o 2
  for (uint8_t s = 0; s < N_TOLVAS; s++) pend[s] = total[s] = (uint8_t)p[s];
  armado = true;
  vasoArmado = vaso && !soltarVasoFlag;         // v13: si ya hay un recipiente VACÍO esperando en Llenado, el lote arranca solo
                                                // (uno que ya se llenó tiene soltarVasoFlag y no se vuelve a llenar)
  Serial.print(F("ARMADO,"));
  Serial.print((int)p[0]); Serial.print(',');
  Serial.print((int)p[1]); Serial.print(',');
  Serial.println((int)p[2]);
}

void comenzarLote() {
  cortarPrueba();                                                  // corta cualquier prueba manual
  activo = siguientePendiente(0);
  for (uint8_t s = 0; s < N_TOLVAS; s++) {                         // corta cualquier giro manual
    soltarServo(s);
    svEst[s] = (s == activo) ? S_MOVIENDO : S_LISTO;
  }
  Serial.println(F("INICIADO"));
  avisoTodos();
  empezarPorcion();
}

void revisarInicio() {                          // arranca el lote registrado en cuanto hay recipiente
  if (armado && estado == IDLE && !emergencia && vaso && vasoArmado && !soltarVasoFlag && manS < 0
      && millis() - tVaso >= ESPERA_RECIPIENTE_MS) comenzarLote();
}

void pausar(Estado nuevo) {                     // detiene el servo y recuerda lo que le faltaba
  estadoPrevio = estado;
  if (estado == GIRANDO) {
    unsigned long usado = millis() - tIni;
    restanteMs = (usado >= restanteMs) ? 0 : restanteMs - usado;
    soltarServo(activo);
  }
  estado = nuevo;
}

void continuar() {                              // sigue donde se había quedado
  estado = estadoPrevio;
  tIni = tPausa = millis();
  if (estado == GIRANDO) girarServo(activo);
  svEst[activo] = S_MOVIENDO;
  Serial.println(F("INICIADO"));
  avisoTodos();
}

void revisarRetiro() {                          // el recipiente lleno ya no está desde hace VASO_RETIRADO_MS: ahora sí se retiró
  if (soltarVasoFlag && !vaso && millis() - tSinVaso >= VASO_RETIRADO_MS) soltarVasoFlag = false;
}

void revisarReanudar() {                        // tras un corte por falta de recipiente: sigue cuando el nuevo ya está asentado
  if (!emergencia && estado == SIN_VASO && vaso && millis() - tVaso >= ESPERA_RECIPIENTE_MS) continuar();
}

void fijarVaso(bool on) {                       // ÚNICA entrada del sensor E2 (llega por el Arduino 2 a través del HMI)
  bool cambio = (on != vaso);
  vaso = on;
  if (cambio && on) tVaso = millis();           // arranca el tiempo de asentamiento
  if (on) Serial.println(F("E2,1")); else Serial.println(F("E2,0"));
  if (!cambio) return;
  if (on && soltarVasoFlag) { vasoArmado = false; return; }   // regresó el MISMO recipiente lleno (parpadeo): no se vuelve a llenar
  vasoArmado = on;                              // solo una pulsación (flanco de subida) habilita el arranque
  if (!on) {
    tSinVaso = millis();                        // el recipiente lleno se da por retirado en revisarRetiro()
    if (!emergencia && (estado == GIRANDO || estado == ESPERA_GIRO || estado == ENTRE)) {   // se quitó a media dispensada
      pausar(SIN_VASO);
      svEst[activo] = S_ESPERA;
      Serial.println(F("SIN_VASO"));
      avisoTodos();
    }
  }
}

void comandoSimple(char c) {
  switch (c) {
    case '?':
      Serial.println(F("READY"));
      avisoTodos();
      reportarGrados();
      if (vaso) Serial.println(F("E2,1")); else Serial.println(F("E2,0"));
      if (armado) {                             // por si el HMI se reconectó con un lote registrado
        Serial.print(F("ARMADO,"));
        Serial.print((int)total[0]); Serial.print(',');
        Serial.print((int)total[1]); Serial.print(',');
        Serial.println((int)total[2]);
      }
      break;

    case 'A':                                   // reanuda tras PARO
      if (!emergencia && estado == DETENIDO) {
        if (!vaso) {                            // sin recipiente: queda esperando
          estado = SIN_VASO;
          svEst[activo] = S_ESPERA;
          Serial.println(F("SIN_VASO"));
          avisoTodos();
        } else {
          continuar();
        }
      }
      break;

    case 'P':
      if (estado == GIRANDO || estado == ESPERA_GIRO || estado == ENTRE) {
        pausar(DETENIDO);
        svEst[activo] = S_PARO;
        Serial.println(F("DETENIDO"));
        avisoTodos();
      }
      break;

    case 'X':                                   // cancela el lote registrado si aún no empieza
      if (armado && estado == IDLE) {
        armado = false;
        for (uint8_t s = 0; s < N_TOLVAS; s++) pend[s] = 0;
        Serial.println(F("CANCELADO"));
      } else {
        Serial.println(F("ERR,OCUPADO"));
      }
      break;

    case 'E':                                   // emergencia: todos los servos se detienen
      emergencia = true;
      armado = false;
      cortarPrueba();
      for (uint8_t s = 0; s < N_TOLVAS; s++) {
        soltarServo(s);
        svEst[s] = S_EMERG;
        pend[s] = 0;
      }
      estado = DETENIDO;
      Serial.println(F("EMERGENCIA"));
      avisoTodos();
      break;

    case 'R':
      if (emergencia) {
        emergencia = false;
        estado = IDLE;
        activo = 0;
        armado = false;
        for (uint8_t s = 0; s < N_TOLVAS; s++) { svEst[s] = S_LISTO; pend[s] = 0; }
        Serial.println(F("RESET_OK"));
        avisoTodos();
      }
      break;

    case 'L':                                   // para todos los servos (solo en reposo)
      if (!emergencia && estado == IDLE) {
        cortarPrueba();
        for (uint8_t s = 0; s < N_TOLVAS; s++) { soltarServo(s); svEst[s] = S_LISTO; }
        Serial.println(F("SOLTADOS"));
        avisoTodos();
      }
      break;
  }
}

void procesarLinea(const char *l) {
  long v[3];
  switch (l[0]) {
    case 'C':                                   // C?  |  C,n,cerrado,abierto,ms,veces  |  CP,ms
      if (l[1] == '?') { reportarGrados(); break; }
      if (l[1] == 'P') {
        if (leerNumeros(l + 2, v, 1) == 1 && v[0] >= 0 && v[0] <= 10000) { pausaAperturasMs = v[0]; guardarGrados(); reportarGrados(); }
        else Serial.println(F("ERR,FORMATO"));
        break;
      }
      if (leerNumeros(l + 1, v, 5) == 5 && v[0] >= 1 && v[0] <= N_TOLVAS && v[1] >= 0 && v[1] <= 180 && v[2] >= 0 && v[2] <= 180
          && v[3] >= 100 && v[3] <= 10000 && v[4] >= 1 && v[4] <= 10) {
        uint8_t s = v[0] - 1;
        angCerrado[s] = v[1]; angAbierto[s] = v[2]; msAbierto[s] = v[3]; vecesPorcion[s] = v[4];
        guardarGrados(); reportarGrados();
        if (estado == IDLE && manS != (int8_t)s && svEst[s] != S_GIRA) cerrarServo(s);   // queda en su nuevo "cerrado"
      } else Serial.println(F("ERR,FORMATO"));
      break;

    case 'M':                                   // M,n,g  mueve el servo n a g grados (buscar el ángulo)
      if (leerNumeros(l + 1, v, 2) != 2 || v[0] < 1 || v[0] > N_TOLVAS || v[1] < 0 || v[1] > 180) { Serial.println(F("ERR,FORMATO")); break; }
      if (!libreManual()) break;
      cortarPrueba();
      moverServo(v[0] - 1, v[1]);
      Serial.print(F("MOV,")); Serial.print((int)v[0]); Serial.print(','); Serial.println((int)v[1]);
      break;

    case 'T':                                   // T,n,k  prueba: abre y cierra k veces
      if (leerNumeros(l + 1, v, 2) != 2 || v[0] < 1 || v[0] > N_TOLVAS || v[1] < 1 || v[1] > 10) { Serial.println(F("ERR,FORMATO")); break; }
      if (!libreManual()) break;
      iniciarPrueba(v[0] - 1, v[1]);
      Serial.print(F("PRUEBA,")); Serial.print((int)v[0]); Serial.print(','); Serial.println((int)v[1]);
      break;

    case 'S':                                   // S,p1,p2,p3
      if (leerNumeros(l + 1, v, 3) == 3) armarLote(v);
      else Serial.println(F("ERR,FORMATO"));
      break;

    case 'V':                                   // V,1 = hay recipiente en E2   V,0 = no hay
      if (leerNumeros(l + 1, v, 1) == 1 && (v[0] == 0 || v[0] == 1)) fijarVaso(v[0] == 1);
      else Serial.println(F("ERR,FORMATO"));
      break;

    case 'G':                                   // G,n,1 = gira   G,n,0 = para
      if (leerNumeros(l + 1, v, 2) != 2 || v[0] < 1 || v[0] > N_TOLVAS || (v[1] != 0 && v[1] != 1)) {
        Serial.println(F("ERR,FORMATO"));
      } else if (emergencia || estado != IDLE) {
        Serial.println(F("ERR,OCUPADO"));
      } else {
        uint8_t s = v[0] - 1;
        if (manS == (int8_t)s) manS = -1;
        if (v[1] == 1) { girarServo(s);  svEst[s] = S_GIRA;  Serial.print(F("GIRA,")); }
        else           { soltarServo(s); svEst[s] = S_LISTO; Serial.print(F("PARA,")); }
        Serial.println((int)v[0]);
        avisoServo(s);
      }
      break;
  }
}

void leerComandos() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'E') { comandoSimple('E'); nlin = 0; continue; }        // la emergencia nunca espera
    if (c == '\n' || c == '\r') {
      if (nlin > 0) { linea[nlin] = '\0'; procesarLinea(linea); nlin = 0; }
      continue;
    }
    if (nlin == 0 && c != 0 && strchr("?APRLX", c)) { comandoSimple(c); continue; }
    if (nlin < sizeof(linea) - 1) linea[nlin++] = c;
  }
}

void setup() {
  Serial.begin(115200);
  for (uint8_t s = 0; s < N_TOLVAS; s++) {
    pinMode(TRIG_PIN[s], OUTPUT);
    pinMode(ECHO_PIN[s], INPUT);
    digitalWrite(TRIG_PIN[s], LOW);
    for (uint8_t i = 0; i < N_MUESTRAS; i++) muestras[s][i] = -1.0;
    idxM[s] = 0;
    svEst[s] = S_LISTO;
  }
  cargarGrados();                               // calibración por grados guardada en la EEPROM
  for (uint8_t s = 0; s < N_TOLVAS; s++) cerrarServo(s);   // al encender las compuertas van a CERRADO
  delay(300);
  Serial.println(F("READY"));
  avisoTodos();
  reportarGrados();
}

void loop() {
  leerComandos();
  muestrear();
  reportarSensores();
  actualizarServo();
  actualizarManual();
  revisarRetiro();
  revisarInicio();
  revisarReanudar();
}
