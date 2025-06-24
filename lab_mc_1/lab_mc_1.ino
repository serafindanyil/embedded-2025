#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define LED1_PIN 0  // D3
#define LED2_PIN 14 // D5
#define LED3_PIN 12 // D6
#define BTN_PIN 13  // GPIO13 

uint8_t lastButtonState = HIGH;
uint32_t lastDebounceTime = 0;
const uint32_t debounceDelay = 100;
bool ledRunning = false;
uint8_t lastActiveLed = LED1_PIN;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LED Controller</title>
    <style>
        body {
            font-family: Arial, Helvetica, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            background-image: linear-gradient(to right top, #d16ba5, #c777b9, #ba83ca, #aa8fd8, #9a9ae1, #8aa7ec, #79b3f4, #69bff8, #52cffe, #41dfff, #46eefa, #5ffbf1);
        }
        .card {
            display: flex;
            flex-direction: column;
            justify-content: center;
            align-items: center;
            gap: 1.5rem;
            padding: 2rem 1.5rem;
            background-color: rgba(255, 255, 255, 0.932);
            border-radius: 10px;
            box-shadow: 3px 3px 90px rgba(0, 0, 0, 0.1);
        }
        h1 {
            font-size: 2rem;
            color: #373134;
        }
        button {
            all: unset;
            font-size: 1.5rem;
            font-weight: 500;
            color: white;
            padding: 1rem 6rem;
            background-color: #aa8fd8;
            border-radius: 15rem;
            cursor: pointer;
            transition: 0.3s;
            user-select: none; /* Запобігає виділенню тексту */
            -webkit-user-select: none; /* Для Safari */
            -moz-user-select: none; /* Для Firefox */
            -ms-user-select: none; /* Для Internet Explorer/Edge */
        }
        button:hover, button:active {
            background-color: #6d5792;
        }
    </style>
</head>
<body>
    <div class="card">
        <h1>Start LED Algorithm</h1>
        <button onmousedown="toggleLeds(true);" onmouseup="toggleLeds(false);">Hold me!</button>
    </div>
    <script>
        function toggleLeds(state) {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/toggle_leds?state=" + (state ? 1 : 0), true);
            xhr.send();
        }
    </script>
</body>
</html>
)rawliteral";

AsyncWebServer server(80);

void setup() {
    pinMode(LED1_PIN, OUTPUT);
    pinMode(LED2_PIN, OUTPUT);
    pinMode(LED3_PIN, OUTPUT);
    pinMode(BTN_PIN, INPUT_PULLUP);

    Serial.begin(57600);
    WiFi.begin("Mi_Home", "asd.fgh.jkl.");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting...");
    }
    Serial.println("Connected! IP: " + WiFi.localIP().toString());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });

    server.on("/toggle_leds", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("state")) {
            String state = request->getParam("state")->value();
            ledRunning = (state == "1");
            if (!ledRunning) {
                digitalWrite(lastActiveLed, HIGH);
            } else {
              digitalWrite(lastActiveLed, LOW);
            }
        }
        request->send(200, "text/plain", "LEDs toggled");
    });

    server.begin();
}

void loop() {
    handleButton();
    if (ledRunning) {
        handleLeds();
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

void handleLeds() {
    static uint32_t lastToggle = 0;
    static uint8_t sequence[] = {LED1_PIN, LED2_PIN, LED3_PIN, LED2_PIN, LED1_PIN};
    static uint8_t index = 0;

    if (millis() - lastToggle >= 1000) {
        lastToggle = millis();
        digitalWrite(sequence[index], HIGH);
        delay(200);
        digitalWrite(sequence[index], LOW);
        lastActiveLed = sequence[index];
        index = (index + 1) % 5;
    }
}