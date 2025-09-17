#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Servo.h>

// --------- CONFIGURACIÓN - CAMBIAR ESTOS VALORES ---------
const char* ssid = "popacra";                    // ¡¡CAMBIAR!!
const char* password = "42614520";            // ¡¡CAMBIAR!!
const char* API_URL = "http://192.168.220.39/app/api/get_schedule.php"; // ¡¡CAMBIAR!!

// --------- Configuración NTP ---------
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600, 60000);

// --------- Configuración Hardware ---------
Servo miServo;
const int SERVO_PIN = D4;                    // Pin del servo
const int FIN_CARRERA_IDA_PIN = D5;          // Sensor fin de carrera ida
const int FIN_CARRERA_VUELTA_PIN = D6;       // Sensor fin de carrera vuelta

// --------- Valores Servo 360° (AJUSTAR SEGÚN TU SERVO) ---------
const int VELOCIDAD_HORARIO = 0;             // Giro horario (0-89)
const int VELOCIDAD_ANTIHORARIO = 180;       // Giro antihorario (91-180)
const int VELOCIDAD_DETENIDO = 90;           // Detenido (90)

// Si tu servo no responde bien, prueba con microsegundos:
// const int VELOCIDAD_HORARIO_US = 1300;      // Giro horario
// const int VELOCIDAD_ANTIHORARIO_US = 1700;  // Giro antihorario  
// const int VELOCIDAD_DETENIDO_US = 1500;     // Detenido
// Y usa: miServo.writeMicroseconds() en lugar de miServo.write()

// --------- Variables del Sistema ---------
String horaProgramada = "";
bool cicloEjecutadoHoy = false;
unsigned long ultimaConsultaAPI = 0;
const unsigned long INTERVALO_API = 60000;   // Consultar API cada 60 segundos

// Estados del ciclo
enum EstadoCiclo {
  ESPERANDO_HORA,
  MOVIENDO_HACIA_ADELANTE,
  PAUSA_EN_POSICION_FINAL,
  REGRESANDO_A_INICIO,
  CICLO_COMPLETADO
};

EstadoCiclo estadoActual = ESPERANDO_HORA;
unsigned long tiempoInicioEstado = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================");
  Serial.println("🍽️  DISPENSADOR DE COMIDA AUTOMÁTICO");
  Serial.println("     Servo 360° + Sensores + API");
  Serial.println("========================================");
  
  // --------- Configurar Hardware ---------
  pinMode(FIN_CARRERA_IDA_PIN, INPUT_PULLUP);
  pinMode(FIN_CARRERA_VUELTA_PIN, INPUT_PULLUP);
  
  miServo.attach(SERVO_PIN);
  miServo.write(VELOCIDAD_DETENIDO);
  Serial.println("✅ Servo 360° inicializado y detenido");
  
  // --------- Conectar WiFi ---------
  Serial.print("📶 Conectando a WiFi");
  WiFi.begin(ssid, password);
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("✅ WiFi conectado!");
    Serial.print("🌐 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("❌ Error: No se pudo conectar a WiFi");
    Serial.println("⚠️  Verifica SSID y contraseña");
    return;
  }
  
  // --------- Sincronizar Hora ---------
  Serial.println("🕐 Sincronizando hora con NTP...");
  timeClient.begin();
  int intentosNTP = 0;
  while (!timeClient.update() && intentosNTP < 10) {
    delay(1000);
    Serial.print(".");
    intentosNTP++;
  }
  
  if (intentosNTP < 10) {
    Serial.println();
    Serial.print("✅ Hora sincronizada: ");
    Serial.println(timeClient.getFormattedTime());
  } else {
    Serial.println();
    Serial.println("⚠️  No se pudo sincronizar hora, continuando...");
  }
  
  // --------- Consultar Horario Inicial ---------
  consultarHorarioProgramado();
  
  Serial.println("========================================");
  Serial.println("🚀 Sistema listo y funcionando");
  Serial.println("⏰ Esperando hora programada...");
  Serial.println("========================================");
}

void loop() {
  // Actualizar hora
  timeClient.update();
  
  // Consultar API periódicamente
  if (millis() - ultimaConsultaAPI > INTERVALO_API) {
    consultarHorarioProgramado();
    ultimaConsultaAPI = millis();
  }
  
  // Resetear ciclo a medianoche
  resetearCicloMedianoche();
  
  // Verificar si es hora de alimentar
  verificarHoraAlimentacion();
  
  // Manejar el ciclo del servo
  manejarCicloServo();
  
  delay(100);
}

void consultarHorarioProgramado() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi desconectado - Reintentando...");
    WiFi.reconnect();
    return;
  }
  
  WiFiClient client;
  HTTPClient http;
  
  Serial.println("📡 Consultando horario programado...");
  http.begin(client, API_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000); // 10 segundos timeout
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("📥 Respuesta: " + payload);
    
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error && doc["success"] == true && doc["hora"] != nullptr) {
      String nuevaHora = doc["hora"].as<String>();
      nuevaHora = nuevaHora.substring(0, 5); // Solo HH:MM
      
      if (nuevaHora != horaProgramada) {
        horaProgramada = nuevaHora;
        cicloEjecutadoHoy = false; // Permitir nuevo ciclo
        Serial.println("🕐 Nueva hora programada: " + horaProgramada);
      } else {
        Serial.println("⏰ Hora confirmada: " + horaProgramada);
      }
    } else {
      Serial.println("⚠️  No hay horario programado o error en respuesta");
      if (error) {
        Serial.println("❌ Error JSON: " + String(error.c_str()));
      }
    }
  } else if (httpCode > 0) {
    Serial.println("❌ Error HTTP: " + String(httpCode));
  } else {
    Serial.println("❌ Error de conexión: " + http.errorToString(httpCode));
  }
  
  http.end();
}

void resetearCicloMedianoche() {
  static bool yaReseteo = false;
  
  if (timeClient.getHours() == 0 && timeClient.getMinutes() == 0) {
    if (cicloEjecutadoHoy && !yaReseteo) {
      cicloEjecutadoHoy = false;
      estadoActual = ESPERANDO_HORA;
      miServo.write(VELOCIDAD_DETENIDO);
      yaReseteo = true;
      Serial.println("🌅 Medianoche - Ciclo reiniciado para nuevo día");
    }
  } else {
    yaReseteo = false; // Resetear la bandera cuando no sea medianoche
  }
}

void verificarHoraAlimentacion() {
  if (cicloEjecutadoHoy || horaProgramada == "" || estadoActual != ESPERANDO_HORA) {
    return;
  }
  
  String horaActual = formatearHora(timeClient.getHours(), timeClient.getMinutes());
  
  if (horaActual == horaProgramada) {
    Serial.println("========================================");
    Serial.println("🍽️  ¡ES HORA DE ALIMENTAR!");
    Serial.println("🕐 Hora programada: " + horaProgramada);
    Serial.println("🕐 Hora actual: " + horaActual);
    Serial.println("🔄 Iniciando ciclo de dispensado...");
    Serial.println("========================================");
    iniciarCicloAlimentacion();
  }
}

void iniciarCicloAlimentacion() {
  estadoActual = MOVIENDO_HACIA_ADELANTE;
  tiempoInicioEstado = millis();
  
  // Iniciar movimiento (ajusta la dirección según tu mecanismo)
  miServo.write(VELOCIDAD_HORARIO);
  Serial.println("🔄 Servo girando hacia adelante...");
}

void manejarCicloServo() {
  switch (estadoActual) {
    
    case ESPERANDO_HORA:
      // Solo esperar, no hacer nada
      break;
      
    case MOVIENDO_HACIA_ADELANTE:
      // Verificar sensor de fin de carrera
      if (digitalRead(FIN_CARRERA_IDA_PIN) == LOW) {
        Serial.println("🛑 Fin de carrera IDA alcanzado");
        miServo.write(VELOCIDAD_DETENIDO);
        estadoActual = PAUSA_EN_POSICION_FINAL;
        tiempoInicioEstado = millis();
      }
      // Timeout de seguridad (15 segundos máximo)
      else if (millis() - tiempoInicioEstado > 15000) {
        Serial.println("⚠️  TIMEOUT en movimiento hacia adelante");
        miServo.write(VELOCIDAD_DETENIDO);
        estadoActual = REGRESANDO_A_INICIO;
        tiempoInicioEstado = millis();
      }
      break;
      
    case PAUSA_EN_POSICION_FINAL:
      // Pausa de 2 segundos para asegurar dispensado
      if (millis() - tiempoInicioEstado > 5000) {
        Serial.println("🔄 Regresando a posición inicial...");
        miServo.write(VELOCIDAD_ANTIHORARIO);
        estadoActual = REGRESANDO_A_INICIO;
        tiempoInicioEstado = millis();
      }
      break;
      
    case REGRESANDO_A_INICIO:
      // Verificar sensor de regreso
      if (digitalRead(FIN_CARRERA_VUELTA_PIN) == LOW) {
        Serial.println("🏠 Posición inicial alcanzada");
        miServo.write(VELOCIDAD_DETENIDO);
        estadoActual = CICLO_COMPLETADO;
        tiempoInicioEstado = millis();
      }
      // Timeout de seguridad (15 segundos máximo)
      else if (millis() - tiempoInicioEstado > 15000) {
        Serial.println("⚠️  TIMEOUT en regreso - Deteniendo servo");
        miServo.write(VELOCIDAD_DETENIDO);
        estadoActual = CICLO_COMPLETADO;
        tiempoInicioEstado = millis();
      }
      break;
      
    case CICLO_COMPLETADO:
      if (!cicloEjecutadoHoy) {
        cicloEjecutadoHoy = true;
        Serial.println("========================================");
        Serial.println("✅ ¡CICLO DE ALIMENTACIÓN COMPLETADO!");
        Serial.println("🐕 Comida dispensada exitosamente");
        Serial.println("⏳ Esperando hasta mañana a las " + horaProgramada);
        Serial.println("========================================");
      }
      estadoActual = ESPERANDO_HORA;
      break;
  }
}

String formatearHora(int hora, int minuto) {
  String h = (hora < 10) ? "0" + String(hora) : String(hora);
  String m = (minuto < 10) ? "0" + String(minuto) : String(minuto);
  return h + ":" + m;
}

// Función de diagnóstico (opcional)
void mostrarEstadoSistema() {
  Serial.println("\n--- ESTADO DEL SISTEMA ---");
  Serial.println("Hora actual: " + timeClient.getFormattedTime());
  Serial.println("Hora programada: " + (horaProgramada != "" ? horaProgramada : "No configurada"));
  Serial.println("Ciclo ejecutado hoy: " + String(cicloEjecutadoHoy ? "SÍ" : "NO"));
  Serial.println("Estado actual: " + String(estadoActual));
  Serial.println("WiFi conectado: " + String(WiFi.status() == WL_CONNECTED ? "SÍ" : "NO"));
  Serial.println("Sensor IDA: " + String(digitalRead(FIN_CARRERA_IDA_PIN) == LOW ? "ACTIVADO" : "LIBRE"));
  Serial.println("Sensor VUELTA: " + String(digitalRead(FIN_CARRERA_VUELTA_PIN) == LOW ? "ACTIVADO" : "LIBRE"));
  Serial.println("---------------------------\n");
}
