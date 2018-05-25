/*
   Verde: 12
   Naranja: 13
   Amarillo: 14
   Rojo: 16 / 2
*/

#define ESP8266_RELAY 12
#define ESP8266_DHT 16
#define ESP8266_IR_REC 13
#define ESP8266_IR_SEND 14
#define ESP8266_LIGHT 0
#include <IRsend.h>
#include <IRrecv.h>
#include <DHT.h>
#include <ESP8266WiFi.h>

const char* ssid     = "Cain";
const char* password = "dante2015";
int dhtpin = ESP8266_DHT;
int green = ESP8266_RELAY;
uint16_t RECV_PIN = ESP8266_IR_REC;

IRrecv irrecv(RECV_PIN);
IRsend irsend(ESP8266_IR_SEND);
decode_results results;
DHT dht(dhtpin, DHT11, 15);                    // dht def for esp8266
WiFiServer server(80);

void setup()
{
  Serial.println("Init");
  Serial.begin(115200);
  delay(10);
  dht.begin();
  pinMode(green, OUTPUT);  // green LED
  digitalWrite(green, HIGH);                     // first green blink = setup finished
  delay(1000);
  digitalWrite(green, LOW);
  WiFi.begin(ssid, password);
  delay(10);
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    counter += 1;
  }
  Serial.println("Connected");
  Serial.println();
  // Start the server
  server.begin();
  Serial.println("Server started");
  // Print the IP address
  Serial.println(WiFi.localIP());
  irrecv.enableIRIn();
  irsend.begin();
  delay(500);
  digitalWrite(green, HIGH);
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

void loop() {
  WiFiClient client = server.available();
  if (!client) {
    return;
  }
  
  // Wait until the client sends some data
  Serial.println("new client");
  while (!client.available()) {
    delay(1);
  }

  // Read the first line of the request
  String req = client.readStringUntil('}');
  client.flush();

  String respBody = "{";
  bool first = true;

  if (req.indexOf("\"relay1\":0") != -1) {
    if (!first) respBody = respBody + ",";
    first = false;
    digitalWrite(ESP8266_RELAY, 0);
    respBody = respBody + "\"relay1\":0";
  }

  if (req.indexOf("\"relay1\":1") != -1) {
    if (!first) respBody = respBody + ",";
    first = false;
    digitalWrite(ESP8266_RELAY, 1);
    respBody = respBody + "\"relay1\":1";
  }

  if (req.indexOf("\"luz\":1") != -1) {
    if (!first) respBody = respBody + ",";
    first = false;
    int value = 0; 
    value = analogRead(A0); 
    respBody = respBody + "\"luz\":" + String(value);
  }

  if (req.indexOf("\"dht\":1") != -1) {
    if (!first) respBody = respBody + ",";
    first = false;
    float h = dht.readHumidity();           // make sure the DHT is on already 2s before doing the reading
    delay(10);                              // and leave time between 2 readings
    float t = dht.readTemperature();
    int counter = 1;
    while (((isnan(h) || isnan(t)) || (h == 0 && t == 0)) && (counter < 5)) {          // if bad DHT reading, read again, max 5 times
      delay(1000);
      h = dht.readHumidity();
      delay(10);
      t = dht.readTemperature();
      counter += 1;
    }
    respBody = respBody + "\"dhtTemp\":" + String(t) + ",\"dhtHum\":" +  String(h);
  }
  
  if (req.indexOf("\"irRec\":1") != -1) {
    if (!first) respBody = respBody + ",";
    first = false;
    int counter = 0;
    bool readed = false;
    uint64_t lastVal;
    while ((!readed) && (counter < 50)) {
      if (irrecv.decode(&results))
      {
        if (lastVal == results.value) {
          readed = true;
          irrecv.resume();
          delay(10);
          respBody = respBody + "\"irCodeH\":\"" + String((uint32_t) (results.value >> 32), HEX) + String((uint32_t) (results.value & 0xFFFFFFFF), HEX) + "\",";
          respBody = respBody + "\"irCodeD\":\"" + String((uint32_t) (results.value), DEC) + "\",";
          respBody = respBody + "\"irType\":\"" + encoding(&results) + "\"";
        } else {
          lastVal = results.value;
        }
      } else {
        counter++;
        delay(100);
      }
    }
    if (!readed) {
      respBody = respBody + "\"irRec\":null";
    }
  }

  if (req.indexOf("\"irSend\"") != -1) {
    String part = req.substring(req.indexOf("\"irSend\":\"") + 10);
    int fin = part.indexOf("\"");
    String code = part.substring(0, fin);
    part = req.substring(req.indexOf("\"irType\":\"") + 10);
    fin = part.indexOf("\"");
    String type = part.substring(0, fin);
    uint64_t codeInt = code.toInt();
    if (type == "NEC") {
      int counter = 0;
      while (counter < 2) {
        uint64_t codeInt64 = code.toInt();
        irsend.sendNEC(codeInt64, 32);
        counter++;
      }
    }
  }

  respBody = respBody + "}";
  client.flush();

  // Prepare the response
  String s = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + respBody;

  // Send the response to the client
  client.print(s);
  delay(1);
  Serial.println("Client disonnected");
}
