#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include <DHT.h>

#define EEPROM_SIZE 512
#define FLASH_BUTTON_PIN 0  // GPIO0 je obvykle připojen k tlačítku Flash

// DHT22 nastavení
#define DHTPIN 5  // Pin připojený k datovému pinu DHT22
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

String wifi_ssid = "";
String wifi_password = "";
String mqtt_server = "";
String mqtt_port = "1883";
String mqtt_user = "";
String mqtt_password = "";
String device_id = ""; // Device ID (MAC adresa)

String temperature_topic = "";
String humidity_topic = "";
String status_topic = "";

ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);

// Funkce pro uložení do EEPROM
void saveToEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  String data = wifi_ssid + "|" + wifi_password + "|" + mqtt_server + "|" + mqtt_port + "|" + mqtt_user + "|" + mqtt_password;
  for (int i = 0; i < data.length(); i++) {
    EEPROM.write(i, data[i]);
  }
  EEPROM.write(data.length(), '\0');
  EEPROM.commit();
  Serial.println("Nastavení uloženo do EEPROM!");
}

// Funkce pro načtení z EEPROM
void loadFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  String data = "";
  char c;
  for (int i = 0; i < EEPROM_SIZE; i++) {
    c = EEPROM.read(i);
    if (c == '\0') break;
    data += c;
  }

  int index1 = data.indexOf('|');
  int index2 = data.indexOf('|', index1 + 1);
  int index3 = data.indexOf('|', index2 + 1);
  int index4 = data.indexOf('|', index3 + 1);
  int index5 = data.indexOf('|', index4 + 1);

  wifi_ssid = data.substring(0, index1);
  wifi_password = data.substring(index1 + 1, index2);
  mqtt_server = data.substring(index2 + 1, index3);
  mqtt_port = data.substring(index3 + 1, index4);
  mqtt_user = data.substring(index4 + 1, index5);
  mqtt_password = data.substring(index5 + 1);

  Serial.println("Nastavení načteno z EEPROM:");
  Serial.println("Wi-Fi SSID: " + wifi_ssid);
  Serial.println("MQTT Server: " + mqtt_server);
}

// Funkce pro získání HTML pro nastavení zařízení
String getHTML() {
  String html = "<html><head>";
  html += "<meta charset=\"UTF-8\">";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #f4f4f4; color: #333; }";
  html += "h1 { color: #333; text-align: center; font-size: 24px; margin-top: 20px; }";
  html += "form { background-color: #fff; padding: 20px; width: 300px; margin: 0 auto; border-radius: 8px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }";
  html += "input[type='text'], input[type='password'] { padding: 8px; margin: 10px 0 20px; width: 100%; border: 1px solid #ccc; border-radius: 4px; box-sizing: border-box; }";
  html += "input[type='submit'] { padding: 10px 20px; background-color: #4CAF50; color: white; border: none; cursor: pointer; border-radius: 4px; width: 100%; }";
  html += "input[type='submit']:hover { background-color: #45a049; }";
  html += "h3 { text-align: center; color: #555; }";
  html += "</style></head><body>";
  html += "<h1>Nastavení zařízení</h1>";
  html += "<form action='/save' method='POST'>";
  html += "Wi-Fi SSID: <input type='text' name='ssid' value='" + wifi_ssid + "'><br>";
  html += "Wi-Fi Heslo: <input type='password' name='password' value='" + wifi_password + "'><br>";
  html += "MQTT Server: <input type='text' name='mqtt_server' value='" + mqtt_server + "'><br>";
  html += "MQTT Port: <input type='text' name='mqtt_port' value='" + mqtt_port + "'><br>";
  html += "MQTT Uživatelské jméno: <input type='text' name='mqtt_user' value='" + mqtt_user + "'><br>";
  html += "MQTT Heslo: <input type='password' name='mqtt_password' value='" + mqtt_password + "'><br>";
  html += "<input type='submit' value='Uložit'>";
  html += "</form>";
  html += "<h3>Device ID: " + device_id + "</h3>";
  html += "</body></html>";
  return html;
}

// Funkce pro zpracování požadavku na hlavní stránku
void handleRoot() {
  server.send(200, "text/html", getHTML());
}

// Funkce pro zpracování uložení nastavení
void handleSave() {
  if (server.hasArg("ssid")) wifi_ssid = server.arg("ssid");
  if (server.hasArg("password")) wifi_password = server.arg("password");
  if (server.hasArg("mqtt_server")) mqtt_server = server.arg("mqtt_server");
  if (server.hasArg("mqtt_port")) mqtt_port = server.arg("mqtt_port");
  if (server.hasArg("mqtt_user")) mqtt_user = server.arg("mqtt_user");
  if (server.hasArg("mqtt_password")) mqtt_password = server.arg("mqtt_password");

  saveToEEPROM();

  server.sendHeader("Content-Type", "text/html; charset=UTF-8");
  server.send(200, "text/html", "<h1>Nastavení uloženo! Zařízení se restartuje...</h1>");
  delay(1000);
  ESP.restart();
}

// Funkce pro připojení k Wi-Fi
void connectToWiFi() {
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  Serial.print("Připojuji se k Wi-Fi...");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(1000);
    Serial.print("...");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Připojeno!");
    Serial.println("IP adresa: " + WiFi.localIP().toString());
  } else {
    Serial.println("Nepodařilo se připojit k Wi-Fi.");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(device_id); // Vytvoření Wi-Fi sítě s názvem device_id (MAC adresa)
    Serial.println("Vytvořena Wi-Fi síť: " + device_id);
  }
}

// Funkce pro připojení k MQTT
void connectToMQTT() {
  while (!client.connected()) {
    Serial.print("Připojuji se k MQTT...");
    if (client.connect(device_id.c_str(), mqtt_user.c_str(), mqtt_password.c_str())) {
      Serial.println("Připojeno k MQTT!");
      client.publish(status_topic.c_str(), "ESP je online");
    } else {
      Serial.print("Chyba, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

// Funkce pro aktualizaci MQTT topiců
void updateMQTTTopics() {
  temperature_topic = "home/esp/" + device_id + "/temperature";
  humidity_topic = "home/esp/" + device_id + "/humidity";
  status_topic = "home/esp/" + device_id + "/status";
}

// Funkce pro kontrolu tlačítka pro tovární reset
void checkFlashButton() {
  static unsigned long pressStartTime = 0;

  if (digitalRead(FLASH_BUTTON_PIN) == LOW) {
    if (pressStartTime == 0) {
      pressStartTime = millis();
    }
    if (millis() - pressStartTime > 5000) {
      Serial.println("Flash tlačítko stisknuto déle než 5 sekund. Obnovuji tovární nastavení...");
      resetToFactorySettings();
    }
  } else {
    pressStartTime = 0;
  }
}

// Funkce pro provedení továrního resetu
void resetToFactorySettings() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  Serial.println("Tovární reset proveden!");
  ESP.restart();
}

void setup() {
  Serial.begin(115200);

  pinMode(FLASH_BUTTON_PIN, INPUT_PULLUP);
  dht.begin();

  // Nastavení device_id na Wi-Fi SSID nebo MAC adresu bez dvojteček
  device_id = WiFi.macAddress();
  device_id.replace(":", ""); // Odstraní dvojtečky z MAC adresy
  device_id = "esp_" + device_id; // Prefix pro název sítě
  Serial.println("Device ID (Wi-Fi SSID/MAC): " + device_id);

  loadFromEEPROM();

  if (wifi_ssid != "") {
    connectToWiFi();
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(device_id); // Nastaví název AP na device_id
    Serial.println("Vytvořena Wi-Fi síť: " + device_id);
  }

  updateMQTTTopics();  // Aktualizace MQTT topiců podle device_id
  client.setServer(mqtt_server.c_str(), mqtt_port.toInt());

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  Serial.println("Webový server spuštěn!");
}

void loop() {
  server.handleClient();
  checkFlashButton();

  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    connectToMQTT();
  }

  client.loop();

  static unsigned long lastReadTime = 0;
  if (millis() - lastReadTime > 10000) { // Každých 10 sekund
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Nepodařilo se načíst data ze senzoru DHT!");
    } else {
      client.publish(temperature_topic.c_str(), String(temperature).c_str());
      client.publish(humidity_topic.c_str(), String(humidity).c_str());
      Serial.print("Teplota: ");
      Serial.print(temperature);
      Serial.println(" °C");
      Serial.print("Vlhkost: ");
      Serial.print(humidity);
      Serial.println(" %");
    }
    lastReadTime = millis();
  }
}
