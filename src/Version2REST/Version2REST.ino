#include <IRsend.h>
#include <IRrecv.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

//-------------------VARIABLES GLOBALES--------------------------
int contconexion = 0;
char ssid[50];
char pass[50];
const char *ssidConf = "ESP8266";
const char *passConf = "configurame";
int dhtPin = 99;
int lightPin = 99;
int relayPin = 99;
int irSendPin = 99;
int irRecPin = 99;
String mensaje = "";
IRrecv *irrecv = NULL;
IRsend *irsend = NULL;
DHT *dht = NULL;

//--------------------------------------------------------------
WiFiClient espClient;
ESP8266WebServer server(8266);

//-------------------Funciones EEPROM--------------------

void writeString(char add, String data) {
  int _size = data.length();
  int i;
  for (i = 0; i < _size; i++)
  {
    EEPROM.write(add + i, data[i]);
  }
  EEPROM.write(add + _size, '\0'); //Add termination null character for String Data
  EEPROM.commit();
}

void write_char(char data) {
  EEPROM.write(0, data);
  EEPROM.commit();
}

char read_char() {
  char data = EEPROM.read(0);
  return data;
}

String read_String() {
  int i;
  char data[4096]; //Max 100 Bytes
  int len = 1;
  unsigned char k;
  while (k != '\0' && len < 4000) //Read until null character
  {
    k = EEPROM.read(len);
    data[len - 1] = k;
    len++;
  }
  data[len] = '\0';
  return String(data);
}

void format_eeprom() {
  for (int i = 0 ; i < EEPROM.length() ; i++) {
    EEPROM.write(i, 0);
  }
}

//---------------------------ESCANEAR----------------------------
void escanear() {
  int n = WiFi.scanNetworks(); //devuelve el número de redes encontradas
  Serial.println("escaneo terminado");
  if (n == 0) { //si no encuentra ninguna red
    Serial.println("no se encontraron redes");
    mensaje = "no se encontraron redes";
  }
  else
  {
    Serial.print(n);
    Serial.println(" redes encontradas");
    mensaje = "";
    for (int i = 0; i < n; ++i)
    {
      // agrega al STRING "mensaje" la información de las redes encontradas
      mensaje = (mensaje) + "<p>" + String(i + 1) + ": " + WiFi.SSID(i) + " (" + WiFi.RSSI(i) + ") Ch: " + WiFi.channel(i) + " Enc: " + WiFi.encryptionType(i) + " </p>\r\n";
      //WiFi.encryptionType 5:WEP 2:WPA/PSK 4:WPA2/PSK 7:open network 8:WPA/WPA2/PSK
      delay(10);
    }
    Serial.println(mensaje);
    paginaconf();
  }
}

void reiniciar() {
  Serial.println("Reinicar");
  format_eeprom();
  write_char('0');
  ESP.eraseConfig();
  ESP.restart();
}

//--------------------MODO_CONFIGURACION------------------------
void modoAP() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssidConf, passConf);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("IP del acces point: ");
  Serial.println(myIP);
}

void guardar_conf() {
  StaticJsonDocument<200> doc;
  doc["ssid"] = server.arg("ssid");
  doc["pass"] = server.arg("pass");
  if (server.arg("dht") != "") doc["dht"] = server.arg("dht");
  if (server.arg("relay") != "") doc["relay"] = server.arg("relay");
  if (server.arg("ir_rec") != "") doc["ir_rec"] = server.arg("ir_rec");
  if (server.arg("ir_send") != "") doc["ir_send"] = server.arg("ir_send");
  if (server.arg("light") != "") doc["light"] = server.arg("light");

  Serial.println("Guardar");

  // Serialize JSON to file
  String jsonString;
  serializeJson(doc, jsonString);
  writeString(1, jsonString);
  write_char('1');
  mensaje = "Configuracion Guardada...";
  Serial.println("Configuracion Leida:");
  Serial.println(jsonString);
  paginaconf();
  delay(200);
  ESP.restart();
}

//------------------------SETUP WIFI-----------------------------
bool read_config() {
  char configured = read_char();
  if (configured != '1') {
    return false;
  }
  String json = read_String();
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.println("Failed to read file, using default configuration");
    return false;
  }
  Serial.println("Configuracion Leida:");
  Serial.println(json);
  strlcpy(ssid, doc["ssid"], sizeof(ssid));
  strlcpy(pass, doc["pass"], sizeof(pass));
  if (doc.containsKey("dht")) {
    dhtPin = atoi(doc["dht"]);
    dht = new DHT(dhtPin, DHT11);
    dht->begin();
  }
  if (doc.containsKey("relay")) {
    relayPin = atoi(doc["relay"]);
    pinMode(relayPin, OUTPUT);
  }
  if (doc.containsKey("ir_rec")) {
    irRecPin = atoi(doc["ir_rec"]);
    irrecv = new IRrecv(irRecPin);
    irrecv->enableIRIn();
  }
  if (doc.containsKey("ir_send")) {
    irSendPin = atoi(doc["ir_send"]);
    irsend = new IRsend(irSendPin);
    irsend->begin();
  }
  if (doc.containsKey("light")) lightPin = atoi(doc["light"]);
  return true;
}

void setup_wifi() {
  // Conexión WIFI
  WiFi.mode(WIFI_STA);
  Serial.print(ssid);
  Serial.print(pass);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED and contconexion < 50) { //Cuenta hasta 50 si no se puede conectar lo cancela
    ++contconexion;
    delay(250);
    Serial.print(".");
  }
  if (contconexion < 50) {
    Serial.println("WiFi conectado");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Error de conexion");
    modoAP();
  }
}

void config_inicial() {
  bool configurado = read_config();
  if (configurado) {
    Serial.println("configurado");
    setup_wifi();
  } else {
    Serial.println("NO configurado");
    modoAP();
  }
  confPage();
}

//-----------CODIGO HTML PAGINA DE CONFIGURACION---------------
String pagina = "<!DOCTYPE html>"
                "<html>"
                "<head>"
                "<title>Tutorial Eeprom</title>"
                "<meta charset='UTF-8'>"
                "</head>"
                "<body>"
                "</form>"
                "<form action='guardar_conf' method='get'>"
                "SSID:<br><br>"
                "<input class='input1' name='ssid' type='text'><br>"
                "PASSWORD:<br><br>"
                "<input class='input1' name='pass' type='password'><br><br>"
                "TB IP:<br><br>"
                "<input class='input1' name='tb_ip' type='text'><br><br>"
                "TB Token:<br><br>"
                "<input class='input1' name='tb_token' type='text'><br><br>"
                "DHT:<br><br>"
                "<input class='input1' name='dht' type='text'><br><br>"
                "RELAY:<br><br>"
                "<input class='input1' name='relay' type='text'><br><br>"
                "IR REC:<br><br>"
                "<input class='input1' name='ir_rec' type='text'><br><br>"
                "IR SEND:<br><br>"
                "<input class='input1' name='ir_send' type='text'><br><br>"
                "LIGHT:<br><br>"
                "<input class='input1' name='light' type='text'><br><br>"
                "<input class='boton' type='submit' value='GUARDAR'/><br><br>"
                "</form>"
                "<a href='escanear'><button class='boton'>ESCANEAR</button></a><br><br>"
                "<a href='reiniciar'><button class='boton'>REINICIAR</button></a><br><br>";

String paginafin = "</body>"
                   "</html>";

void paginaconf() {
  server.send(200, "text/html", pagina + mensaje + paginafin);
}

void confPage() {
  Serial.println("WebServer iniciado...");
  server.on("/", paginaconf); //esta es la pagina de configuracion
  server.on("/guardar_conf", guardar_conf); //Graba en la eeprom la configuracion
  server.on("/escanear", escanear); //Escanean las redes wifi disponibles
  server.on("/reiniciar", reiniciar);
  server.on("/api", apiHandler);
  server.begin();
}

//-----------API Handler---------------
void apiHandler() {
  StaticJsonDocument<200> responseJson;
  String body = server.arg("plain");
  Serial.println("Body recibido");
  Serial.println(body);
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    Serial.println("Failed to read file, using default configuration");
    server.send(500, "application/json", "{'error':'error de lectura JSON'}");
    return;
  }
  if (doc.containsKey("dht")) responseJson["dht"] = handleDHT(doc["dht"]);
  if (doc.containsKey("relay")) responseJson["relay"] = handleRelay(doc["relay"]);
  if (doc.containsKey("ir_rec")) responseJson["ir_rec"] = handleIrRec(doc["ir_rec"]);
  if (doc.containsKey("ir_send")) responseJson["ir_send"] = handleIrSend(doc["ir_send"]);
  if (doc.containsKey("light")) responseJson["light"] = handleLigth(doc["light"]);
  String responseString;
  serializeJson(responseJson, responseString);
  server.send(200, "application/json", responseString);
  return;
}

String handleDHT(String value) {
  if (dhtPin == 99) return "disabled";
  int counter = 0;
  StaticJsonDocument<200> responseJson;
  float h = dht->readHumidity();
  delay(10);
  float t = dht->readTemperature();
  while (((isnan(h) || isnan(t)) || (h == 0 && t == 0)) && (counter < 5)) {
    delay(1000);
    h = dht->readHumidity();
    delay(10);
    t = dht->readTemperature();
    counter += 1;
  }
  responseJson["temp"] = String(t);
  responseJson["hum"] = String(h);
  String responseString;
  serializeJson(responseJson, responseString);
  return responseString;
}

String handleRelay(String value) {
  if (relayPin == 99) return "disabled";
  int valueInt = value.toInt();
  digitalWrite(relayPin, valueInt);
  return value;
}

String handleIrRec(String value) {
  if (irRecPin == 99) return "disabled";
  StaticJsonDocument<200> responseJson;
  decode_results results;
  int counter = 0;
  bool readed = false;
  uint64_t lastVal;
  while ((!readed) && (counter < 50)) {
    if (irrecv->decode(&results))
    {
      if (lastVal == results.value) {
        readed = true;
        irrecv->resume();
        delay(10);
        responseJson["irCodeH"] = String((uint32_t) (results.value >> 32), HEX) + String((uint32_t) (results.value & 0xFFFFFFFF), HEX);
        responseJson["irCodeD"] = String((uint32_t) (results.value), DEC);
        responseJson["irType"] = encoding(&results);
      } else {
        lastVal = results.value;
      }
    } else {
      counter++;
      delay(100);
    }
  }
  if (!readed) return "null";
  String responseString;
  serializeJson(responseJson, responseString);
  return responseString;
}

String handleIrSend(String value) {
  if (irSendPin == 99) return "disabled";
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, value);
  if (error) {
    Serial.println("Failed to read file, using default configuration");
    return "error de formato json";
  }
  
  String type = doc["irType"];
  uint64_t codeInt = atoi(doc["irSend"]);
  if (type == "NEC") {
    int counter = 0;
    while (counter < 2) {
      uint64_t codeInt64 = atoi(doc["irSend"]);
      irsend->sendNEC(codeInt64, 32);
      counter++;
    }
  }
  return "ok";
}

String handleLigth(String value) {
  if (lightPin == 99) return "disabled";
  int valueOut = analogRead(lightPin);
  return String(valueOut);
}

String encoding(decode_results *results) {
  switch (results->decode_type) {
    default:
    case UNKNOWN:      return "UNKNOWN";       break;
    case NEC:          return "NEC";           break;
    case SONY:         return "SONY";          break;
    case RC5:          return "RC5";           break;
    case RC6:          return "RC6";           break;
    case DISH:         return "DISH";          break;
    case SHARP:        return "SHARP";         break;
    case JVC:          return "JVC";           break;
    case SANYO:        return "SANYO";         break;
    case SANYO_LC7461: return "SANYO_LC7461";  break;
    case MITSUBISHI:   return "MITSUBISHI";    break;
    case SAMSUNG:      return "SAMSUNG";       break;
    case LG:           return "LG";            break;
    case WHYNTER:      return "WHYNTER";       break;
    case AIWA_RC_T501: return "AIWA_RC_T501";  break;
    case PANASONIC:    return "PANASONIC";     break;
    case DENON:        return "DENON";         break;
    case COOLIX:       return "COOLIX";        break;
  }
}

//------------------------Main-----------------------------

void setup()
{
  // Inicia Serial
  Serial.begin(115200);
  Serial.println("");
  EEPROM.begin(4096);
  config_inicial();
}

void loop() {
  delay(1000);
  server.handleClient();
}
