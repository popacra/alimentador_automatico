#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include <time.h>

// --------- CONFIGURACIÓN - CAMBIAR ESTOS VALORES ---------
const char *ssid = "popacra";                                                                    // ¡¡CAMBIAR!!
const char *password = "42614520";                                                                   // ¡¡CAMBIAR!!
const char *API_MOMENTO_URL = "https://crocmaster.tecnica4berazategui.edu.ar/api/get_schedule.php";      // ¡¡CAMBIAR!!
const char *API_HISTORIAL_URL = "https://crocmaster.tecnica4berazategui.edu.ar/api/post_historial.php";  // ¡¡CAMBIAR!!
const char *API_MAC_URL = "https://crocmaster.tecnica4berazategui.edu.ar/api/mac_handler.php";           // ¡¡CAMBIAR!!

// --------- Configuración NTP ---------
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600;  // Argentina (UTC-3)
const int daylightOffset_sec = 0;

// --------- Configuración Hardware ---------
Servo miServo;
const int SERVO_PIN = D4;               // Pin del servo
const int FIN_CARRERA_IDA_PIN = D5;     // Sensor fin de carrera ida
const int FIN_CARRERA_VUELTA_PIN = D6;  // Sensor fin de carrera vuelta
String MAC = WiFi.macAddress();
// --------- Valores Servo 360° (AJUSTAR SEGÚN TU SERVO) ---------
const int VELOCIDAD_HORARIO = 0;        // Giro horario (0-89)
const int VELOCIDAD_ANTIHORARIO = 180;  // Giro antihorario (91-180)
const int VELOCIDAD_DETENIDO = 90;      // Detenido (90)

// --------- Variables del Sistema ---------
String horaProgramada = "";
bool cicloEjecutadoHoy = false;
unsigned long ultimaConsultaAPI = 0;
const unsigned long INTERVALO_API = 60000;  // Consultar API cada 60 segundos

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

  //---------- Configuracion de MAC -------

  // Consulta a la base de datos si existe la MAC
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi desconectado - Reintentando...");
    WiFi.reconnect();
    return;
  }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = String(API_MAC_URL) + "?mac=" + MAC;
  Serial.println("🌐 Consultando: " + url);

  http.begin(client, url);

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000);  // 10 segundos timeout
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("📥 Respuesta: " + payload);

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.println("❌ Error al parsear JSON: " + String(error.c_str()));
    } else {
      // Solo entramos aquí si el JSON se leyo bien
      bool exists = doc["exists"] | false;
      const char *MACRESPONSE = doc["mac"] | "";
      if (!exists) {
        Serial.println("⚠️  MAC no reconocida en base de datos");

        StaticJsonDocument<200> doc;
        doc["mac"] = MAC;
        String jsonResponse;
        serializeJson(doc, jsonResponse);

        //inicio cliente a la URL de la api
        http.begin(client, API_MAC_URL);

        // Especifico que tipo de archivo voy a pasar
        http.addHeader("Content-Type", "application/json");
        // Mensaje a pasar a la API

        int httpResponseCode = http.POST(jsonResponse);

        // Respuestas que tira a la solicitud
        // Peticion Exitosa
        if (httpResponseCode > 0) {
          Serial.println("HTTP POST codigo de respuesta: ");
          Serial.println(httpResponseCode);
          Serial.println(http.getString());
          Serial.println("✅ MAC guardada correctamente: " + String(MAC));
        } else {  // Peticion Fallida
          Serial.printf("Error en la solicitud HTTP POST: $s\n", http.errorToString(httpResponseCode).c_str());
        }

        http.end();
      } else {
        Serial.println("✅ MAC válida: " + String(MACRESPONSE));
      }
    }
  } else if (httpCode > 0) {
    Serial.println("❌ Error HTTP: " + String(httpCode));
  } else {
    Serial.println("❌ Error de conexión: " + http.errorToString(httpCode));
  }

  http.end();

  // --------- Sincronizar Hora ---------
  Serial.println("🕐 Sincronizando hora con NTP...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Esperar hasta obtener hora válida
  struct tm timeinfo;
  int intentosNTP = 0;
  while (!getLocalTime(&timeinfo) && intentosNTP < 10) {
    Serial.print(".");
    delay(1000);
    intentosNTP++;
  }

  if (getLocalTime(&timeinfo)) {
    Serial.println();
    Serial.print("✅ Hora sincronizada: ");
    char momento[25];
    strftime(momento, sizeof(momento), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    Serial.println(momento);
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

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String(API_MOMENTO_URL) + "?mac=" + MAC;

  Serial.println("📡 Consultando horario programado...");
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000);  // 10 segundos timeout

  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("📥 Respuesta: " + payload);

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error && doc["success"] == true && doc["hora"] != nullptr) {
      String nuevaHora = doc["hora"].as<String>();
      nuevaHora = nuevaHora.substring(0, 5);  // Solo HH:MM

      if (nuevaHora != horaProgramada) {
        horaProgramada = nuevaHora;
        cicloEjecutadoHoy = false;  // Permitir nuevo ciclo
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
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0) {
    if (cicloEjecutadoHoy && !yaReseteo) {
      cicloEjecutadoHoy = false;
      estadoActual = ESPERANDO_HORA;
      miServo.write(VELOCIDAD_DETENIDO);
      yaReseteo = true;
      Serial.println("🌅 Medianoche - Ciclo reiniciado para nuevo día");
    }
  } else {
    yaReseteo = false;
  }
}
void enviodeHoraHistorial() {

  // Verificar el estado de la conexion a internet, osea que este activa a la hora de usar la funcion
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    // Setear struct de time.h
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Error al obtener la hora en envio a historial");
      return;
    }
    //obtener hora y fecha en variables
    char momento[20];
    char hora[20];
    char fecha[25];
    String descripcion = "Alimentacion completada correctamente";
    // Guardar datos
    strftime(momento, sizeof(momento), "%Y:%m:%d %H:%M:%S", &timeinfo);
    strftime(hora, sizeof(hora), "%H:%M:%S", &timeinfo);
    strftime(fecha, sizeof(fecha), "%Y-%m-%d", &timeinfo);

    // Transformar a JSON
    StaticJsonDocument<200> doc;
    doc["momento"] = momento;
    doc["fecha"] = fecha;
    doc["hora"] = hora;
    doc["descripcion"] = descripcion;
    String jsonString;
    serializeJson(doc, jsonString);

    //inicio cliente a la URL de la api
    http.begin(client, API_HISTORIAL_URL);

    // Especifico que tipo de archivo voy a pasar
    http.addHeader("Content-Type", "application/json");
    // Mensaje a pasar a la API

    int httpResponseCode = http.POST(jsonString);

    // Respuestas que tira a la solicitud
    // Peticion Exitosa
    if (httpResponseCode > 0) {
      Serial.println("HTTP POST codigo de respuesta: ");
      Serial.println(httpResponseCode);
      Serial.println(http.getString());
    } else {  // Peticion Fallida
      Serial.printf("Error en la solicitud HTTP POST: $s\n", http.errorToString(httpResponseCode).c_str());
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

void verificarHoraAlimentacion() {
  if (cicloEjecutadoHoy || horaProgramada == "" || estadoActual != ESPERANDO_HORA) {
    return;
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  String horaActual = formatearHora(timeinfo.tm_hour, timeinfo.tm_min);

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
        enviodeHoraHistorial();
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

