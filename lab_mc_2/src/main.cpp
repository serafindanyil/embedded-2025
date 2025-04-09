#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

#define LED1_PIN 0
#define LED2_PIN 14
#define LED3_PIN 12
#define BTN_PIN 13

const char* ssid = "EOM";
const char* password = "nulp1816";

ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

uint8_t lastButtonState = HIGH;
uint32_t lastDebounceTime = 0;
const uint32_t debounceDelay = 120;
bool ledRunning = false;
uint8_t lastActiveLed = LED1_PIN;

String serialBuffer = "";

int clientCount = 0;

void handleWebSocketMessage(uint8_t *payload, size_t length) {
  StaticJsonDocument<200> doc;

  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }

  const char* board = doc["board"];
  bool state = doc["state"];

  if (strcmp(board, "current") == 0) {
    ledRunning = state;
    digitalWrite(lastActiveLed, state ? LOW : HIGH);
  } else if (strcmp(board, "uart") == 0) {
    Serial.println(state ? "ls_1" : "ls_0");
  }
}

void handleWebSocketClients(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_CONNECTED:
      clientCount++;
      Serial.printf("Client [%u] connected. Total clients: %d\n", num, clientCount);
      break;
    case WStype_DISCONNECTED:
      clientCount--;
      Serial.printf("Client [%u] disconnected. Total clients: %d\n", num, clientCount);
      break;
  }
}

void handleFileRequest(String path) {
  if (path.endsWith("/")) path += "index.html";

  String contentType = "text/plain";
  if (path.endsWith(".html")) contentType = "text/html";
  else if (path.endsWith(".css")) contentType = "text/css";
  else if (path.endsWith(".js")) contentType = "application/javascript";

  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return;
  }

  server.send(404, "text/plain", "File Not Found");
}

void setup() {
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);

  Serial.begin(57600);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
  }
  Serial.println("Connected! IP: " + WiFi.localIP().toString());

  LittleFS.begin();

  server.onNotFound([]() {
    handleFileRequest(server.uri());
  });

  webSocket.begin();
  webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    handleWebSocketClients(num, type, payload, length);
  
    if (type == WStype_TEXT) {
      handleWebSocketMessage(payload,length);
    }
  });  

  server.begin();
}

void receiveActiveLed(String board, uint8_t ledCount) {
  String json = "{\"board\": \"" + board + "\", \"activeLed\": " + String(ledCount) + "}";
  

  if (clientCount == 0) {
    Serial.println("a_" + String(ledCount)); 
  } else {
    webSocket.broadcastTXT(json);
  }
}

int getLedCount(uint8_t pin) {
  switch (pin) {
    case LED1_PIN: return 1;
    case LED2_PIN: return 2;
    case LED3_PIN: return 3;
    default: return 0;
  }
}


void handleLeds() {
  static uint32_t lastToggle = 0;
  static uint8_t sequence[] = {LED1_PIN, LED2_PIN, LED3_PIN, LED2_PIN, LED1_PIN};
  static uint8_t index = 0;

  String board = (clientCount == 0) ? "uart" : "current";

  if (millis() - lastToggle >= 1000) {
    lastToggle = millis();
    digitalWrite(sequence[index], HIGH);
    delay(200);
    digitalWrite(sequence[index], LOW);
    lastActiveLed = sequence[index];
    receiveActiveLed(board, getLedCount(lastActiveLed));
    index = (index + 1) % 5;
  }
}


void handleButton() {
  uint32_t tmp = millis();
  int btnVal = digitalRead(BTN_PIN);
  bool dbtn = false;

  if (btnVal != lastButtonState) {
    lastDebounceTime = millis();
    dbtn = true;
  }

  if (dbtn && ((tmp - lastDebounceTime) > debounceDelay)) {
    dbtn = false;
    lastDebounceTime = 0;

    if (btnVal == LOW && lastButtonState == HIGH) {
      ledRunning = true;
      digitalWrite(lastActiveLed, LOW);
    }

    if (btnVal == HIGH && lastButtonState == LOW) {
      ledRunning = false;
      digitalWrite(lastActiveLed, HIGH);
    }

    lastButtonState = btnVal;
  }
}

void handleUARTReceive() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      serialBuffer.trim();
      if (serialBuffer.startsWith("ls_")) {
        int ledState = serialBuffer.substring(3).toInt();
        ledRunning = (ledState == 1) ? true : false;
        digitalWrite(lastActiveLed, (ledState == 1) ? LOW : HIGH);
      } else if (serialBuffer.startsWith("a_")) {
        int ledCount = serialBuffer.substring(2).toInt();

        receiveActiveLed("uart", ledCount);
      }
      serialBuffer = "";
    } else {
      serialBuffer += ch;
    }
  }
}

void loop() {
  handleButton();
  handleUARTReceive();
  server.handleClient();
  webSocket.loop();
  if (ledRunning) {
    handleLeds();
  }
}