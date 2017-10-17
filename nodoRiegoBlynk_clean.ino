/*
 * Tomás Vigouroux
 * 16Oct17
 * 
 * *******Sistema de Riego Automático*******
 * 
 * El sistema reemplaza una central Orbit usando cuatros zonas de riego abastecidas por 4 válvulas solenoides de 24vac
 * 
 * Hardware:
 * 
 * - 1 x nodeMCU v1.0
 * - 1 x modulo de 4 reles
 * - 1 fuente de poder de 24VAC (la original Orbit)
 * - 4 cuatro válvulas solenoides de 24VAC
 * 
 *  
 * Software:
 * 
 * - Blynk
 * - Capacidad OTA desde IDE
 * 
 * Pines:
 * 
 * D1: rele 1/solenoide 1
 * D2: rele 2/solenoide 2
 * D3: rele 3/solenoide 3
 * D4: rele 4/solenoide 4
 * 
 * V1: switch virtual zona 1
 * V2: switch virtual zona 2 
 * V3: switch virtual zona 3
 * V4: switch virtual zona 4
 * V5: valor en segundos encendido zona 1
 * V6: valor en segundos encendido zona 2 
 * V7: valor en segundos encendido zona 3
 * V8: valor en segundos encendido zona 4
 * V9: slider para fijar tiempo de riego zona 1
 * V10: slider para fijar tiempo de riego zona 2
 * V11: slider para fijar tiempo de riego zona 3
 * V12: slider para fijar tiempo de riego zona 4
 */




// descomentar "2" para tener salida básica en Serial, descomentar adicionalmente "1" para debugging junto con "2"
//#define BLYNK_DEBUG // 1
//#define BLYNK_PRINT Serial // 2

// Las definiciones de a continuación mapean los pines del ESP8266 a los de la placa nodeMCU
#define D0 16
#define D1 5 // I2C Bus SCL (clock)
#define D2 4 // I2C Bus SDA (data)
#define D3 0
#define D4 2 // Same as "LED_BUILTIN", but inverted logic
#define D5 14 // SPI Bus SCK (clock)
#define D6 12 // SPI Bus MISO 
#define D7 13 // SPI Bus MOSI
#define D8 15 // SPI Bus SS (CS)
#define D9 3 // RX0 (Serial console)
#define D10 1 // TX0 (Serial console)

// librerias requeridas
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <BlynkSimpleEsp8266.h>

// Obtener tu propio código bajando la aplicación Blynk para Android o iOS
char auth[] = "**********";

// Se define objeto que controlará temporizadores. Esta es una práctica necesaria para usar Blynk, en lugar de DELAY. Funciona igual que SimpleTimer.h
BlynkTimer timer;

// Credenciales WIFI
char ssid[] = "*******";
char pass[] = "********";

// Definición variables globales
const int relay1Pin = D1; // pines a los cuales estarán conectados los 4 reles
const int relay2Pin = D2;
const int relay3Pin = D3;
const int relay4Pin = D4;
const int limSeguridad = 420; // límite de seguridad en segundos en caso que se pierda comunicación con la central, en este caso ningún riego podrá por firmware funcionar mas de 7 minutos
bool flag1 = false; // almacena el estado de cada solenoide
bool flag2 = false;
bool flag3 = false;
bool flag4 = false;
unsigned long tiempo1 = 0; // registra el tiempo que lleva encendido en ms cada solenoide
unsigned long tiempo2 = 0;
unsigned long tiempo3 = 0;
unsigned long tiempo4 = 0;
unsigned long tiempo1On = 0; // registra el momento millis en cual un riego pasó a estado encendido
unsigned long tiempo2On = 0;
unsigned long tiempo3On = 0;
unsigned long tiempo4On = 0;
int tiempo1Prog = 30; // registran los tiempos programados para cada zona en segundos, se inicializan en 30 segundos
int tiempo2Prog = 30;
int tiempo3Prog = 30;
int tiempo4Prog = 30;

BLYNK_CONNECTED() {
    Blynk.syncVirtual(V1, V2, V3, V4, V9, V10, V11, V12); // sincroniza en placa valores registrados en servidor para botones virtuales y sliders
}


void setup()
{
  // Debug console
  Serial.begin(9600); //para debugging
  
  Blynk.begin(auth, ssid, pass); // Inicializa conexión con Blynk

  ArduinoOTA.setPassword((const char *)"*********"); // Define password de protección para OTA desde IDE
  ArduinoOTA.begin(); // Inicializa OTA
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP()); // Imprime valor básicos de red
  pinMode(relay1Pin, OUTPUT); // Inicializa pines físicos como salidas
  pinMode(relay2Pin, OUTPUT);
  pinMode(relay3Pin, OUTPUT);
  pinMode(relay4Pin, OUTPUT);
  digitalWrite(relay1Pin, HIGH); // Define los pines inicialmente como todos apagados (reles de lógica inversa, HIGH es off - LOW es on)
  digitalWrite(relay2Pin, HIGH);
  digitalWrite(relay3Pin, HIGH);
  digitalWrite(relay4Pin, HIGH);
  timer.setInterval(5000, tiemposAct); // define los intervalos en ms que se ejecutarán rutinas
  timer.setInterval(1000, actualizarTiempos);
  timer.setInterval(10000, revisarTiempos);
}
void revisarTiempos() { // esta rutina revisa si algún contador de tiempo del riego sobrepasó el tiempo definido ya sea como programación o como límite de seguridad
  if (tiempo1/1000 > limSeguridad || tiempo1/1000 > tiempo1Prog){ // en caso que se sobrepase tiempo, se apaga rele y se actualiza pin virtual. Se resetean variables.
    Blynk.virtualWrite(1,0);
    digitalWrite(relay1Pin, HIGH);
    tiempo1 = 0;
    flag1 = false; 
  }
  if (tiempo2/1000 > limSeguridad || tiempo2/1000 > tiempo2Prog){
    Blynk.virtualWrite(2,0);
    digitalWrite(relay2Pin, HIGH);
    tiempo2 = 0;
    flag2 = false; 
  }
  if (tiempo3/1000 > limSeguridad || tiempo3/1000 > tiempo3Prog){
    Blynk.virtualWrite(3,0);
    digitalWrite(relay3Pin, HIGH);
    tiempo3 = 0;
    flag3 = false; 
  }
  if (tiempo4/1000 > limSeguridad || tiempo4/1000 > tiempo4Prog){
    Blynk.virtualWrite(4,0);
    digitalWrite(relay4Pin, HIGH);
    tiempo4 = 0;
    flag4 = false; 
  }
}

void actualizarTiempos() { // actualiza las variables de tiempo de encendido dependiendo si por "flag" está el relé encendido o apagado
  if (flag1) {
    tiempo1 = millis() - tiempo1On;
  }
  else {
    tiempo1On = millis();
  }
  if (flag2) {
    tiempo2 = millis() - tiempo2On;
  }
  else {
    tiempo2On = millis();
  }
  if (flag3) {
    tiempo3 = millis() - tiempo3On;
  }
  else {
    tiempo3On = millis();
  }
  if (flag4) {
    tiempo4 = millis() - tiempo4On;
  }
  else {
    tiempo4On = millis();
  }
  
}

void tiemposAct() // Hace "push" de los tiempos de encendido de cada zona
{
  Blynk.virtualWrite(5, tiempo1/1000);
  Blynk.virtualWrite(6, tiempo2/1000);
  Blynk.virtualWrite(7, tiempo3/1000);
  Blynk.virtualWrite(8, tiempo4/1000);
}


BLYNK_WRITE(1) // recibe cambios en pin virtual 1 y aplica (lógica inversa) el valor al pin físico
{
    digitalWrite(relay1Pin, !param.asInt());
    if (param.asInt()) {
      tiempo1 = 0;
      flag1 = true;
    }
    else {
      flag1 = false;
      tiempo1 = 0;
    }
}
BLYNK_WRITE(2)
{
    digitalWrite(relay2Pin, !param.asInt());
    if (param.asInt()) {
      tiempo2 = 0;
      flag2 = true;
    }
    else {
      flag2 = false;
      tiempo2 = 0;
    }
}
BLYNK_WRITE(3)
{
    digitalWrite(relay3Pin, !param.asInt());
    if (param.asInt()) {
      tiempo3 = 0;
      flag3 = true;
    }
    else {
      flag3 = false;
      tiempo3 = 0;
    }
}
BLYNK_WRITE(4)
{
    digitalWrite(relay4Pin, !param.asInt());
    if (param.asInt()) {
      tiempo4 = 0;
      flag4 = true;
    }
    else {
      flag4 = false;
      tiempo4 = 0;
    }
}
BLYNK_WRITE(9) // lee los valores de los slider y los almacena en variables
{
  tiempo1Prog = param.asInt(); 
}
BLYNK_WRITE(10)
{
  tiempo2Prog = param.asInt();
}
BLYNK_WRITE(11)
{
  tiempo3Prog = param.asInt();
}
BLYNK_WRITE(12)
{
  tiempo4Prog = param.asInt();
}

void loop() //notar que el "loop" se mantiene limpio. Esto corresponde a las mejores prácticas de Blynk. Para mas información revisar docs
{
  Blynk.run();  //rutina que corre Blynk
  timer.run();  //rutina que corre timers
  ArduinoOTA.handle(); //rutina que corre OTA
}

