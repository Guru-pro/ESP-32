/*
  ============================================================
       MOORTHY SMART SIGN TESTER V1.6
       ESP32 + WS2811 + CX1903 + VOLTAGE MONITOR
  ============================================================

  DATA PIN       : GPIO 5 / D5
  VOLTAGE SENSOR : GPIO 35 / D35

  DEFAULT LED    : CX1903
  CX1903         : UCS1903B + BRG
  WS2811         : WS2811 + BRG

  WiFi AP:
  SSID     : MOORTHY-TESTER
  PASSWORD : 12345678
  IP       : 192.168.4.1
*/

#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>

// ============================================================
// HARDWARE
// ============================================================

#define LED_PIN       5
#define VOLTAGE_PIN   35

#define MAX_LEDS      500
#define DEFAULT_LEDS  21

CRGB leds[MAX_LEDS];

WebServer server(80);

// ============================================================
// LED CONTROLLERS
// ============================================================

CLEDController *wsController;
CLEDController *cxController;
CLEDController *activeController;

// ============================================================
// LED SETTINGS
// ============================================================

enum LED_TYPE_SELECT {
  LED_WS2811,
  LED_CX1903
};

LED_TYPE_SELECT currentLEDType = LED_CX1903;

int ledCount = DEFAULT_LEDS;
int brightness = 100;

int sectionStart = 1;
int sectionEnd   = DEFAULT_LEDS;

String currentMode = "OFF";
String currentColor = "OFF";

// ============================================================
// VOLTAGE SETTINGS
// ============================================================

// Common 0-25V voltage sensor module.
// At 12V input, output is approximately 2.4V.

float voltage = 0.0;

// Start with 1.000.
// We will calibrate this against your multimeter.
float voltageCalibration = 1.000;

// Voltage update timing
unsigned long lastVoltageRead = 0;

#define VOLTAGE_UPDATE_INTERVAL 250

// ============================================================
// TIMING
// ============================================================

unsigned long lastUpdate = 0;
unsigned long lastStep = 0;

int chasePosition = 0;

int scanPixel = 0;
bool scanRunning = false;
bool reverseScan = false;

uint8_t rainbowHue = 0;

// ============================================================
// COLOUR
// ============================================================

CRGB selectedColor = CRGB::Black;

// ============================================================
// SHOW ONLY SELECTED CONTROLLER
// ============================================================

void showLEDs() {

  if (activeController != nullptr) {

    activeController->showLeds(brightness);
  }
}

// ============================================================
// CLEAR
// ============================================================

void clearLEDs() {

  fill_solid(
    leds,
    MAX_LEDS,
    CRGB::Black
  );

  showLEDs();
}

// ============================================================
// SET LED TYPE
// ============================================================

void selectLEDType() {

  if (currentLEDType == LED_WS2811) {

    activeController = wsController;

    Serial.println("LED TYPE: WS2811");
    Serial.println("PROTOCOL: WS2811");
    Serial.println("COLOUR : BRG");

  } else {

    activeController = cxController;

    Serial.println("LED TYPE: CX1903");
    Serial.println("PROTOCOL: UCS1903B");
    Serial.println("COLOUR : BRG");
  }

  clearLEDs();
}

// ============================================================
// VOLTAGE READING
// ============================================================

void readVoltage() {

  if (millis() - lastVoltageRead <
      VOLTAGE_UPDATE_INTERVAL) {

    return;
  }

  lastVoltageRead = millis();

  const int samples = 20;

  uint32_t total = 0;

  for (int i = 0; i < samples; i++) {

    total += analogRead(VOLTAGE_PIN);

    delayMicroseconds(200);
  }

  float averageRaw =
    (float)total / samples;

  // ESP32 ADC approximate conversion.
  // 11dB attenuation is used below.

  float adcVoltage =
    (averageRaw / 4095.0) * 3.3;

  // 0-25V module approximately divides
  // input voltage by 5.

  voltage =
    adcVoltage * 5.0;

  // Calibration multiplier

  voltage =
    voltage * voltageCalibration;

  // Prevent tiny noise around zero

  if (voltage < 0.05) {

    voltage = 0.0;
  }
}

// ============================================================
// SOLID COLOUR
// ============================================================

void solidColor(CRGB color) {

  currentMode = "SOLID";

  selectedColor = color;

  fill_solid(
    leds,
    ledCount,
    color
  );

  for (
    int i = ledCount;
    i < MAX_LEDS;
    i++
  ) {

    leds[i] = CRGB::Black;
  }

  showLEDs();
}

// ============================================================
// CHASE
// ============================================================

void chaseEffect() {

  if (millis() - lastStep < 70)
    return;

  lastStep = millis();

  fill_solid(
    leds,
    ledCount,
    CRGB::Black
  );

  CRGB colours[] = {

    CRGB::Red,
    CRGB::Yellow,
    CRGB::Green,
    CRGB::Blue,
    CRGB::Magenta,
    CRGB::Cyan,
    CRGB::Orange
  };

  int colourCount = 7;

  for (int tail = 0; tail < 4; tail++) {

    int pos =
      chasePosition - tail;

    while (pos < 0)
      pos += ledCount;

    leds[pos % ledCount] =
      colours[
        (chasePosition + tail)
        % colourCount
      ];
  }

  showLEDs();

  chasePosition++;

  if (chasePosition >= ledCount)
    chasePosition = 0;
}

// ============================================================
// RAINBOW
// ============================================================

void rainbowEffect() {

  if (millis() - lastUpdate < 30)
    return;

  lastUpdate = millis();

  for (int i = 0; i < ledCount; i++) {

    leds[i] =
      CHSV(
        rainbowHue +
        ((i * 255) / ledCount),

        255,

        255
      );
  }

  showLEDs();

  rainbowHue++;
}

// ============================================================
// FIRE
// ============================================================

void fireEffect() {

  if (millis() - lastUpdate < 45)
    return;

  lastUpdate = millis();

  for (int i = 0; i < ledCount; i++) {

    byte heat =
      random8(80, 255);

    if (heat < 100) {

      leds[i] =
        CRGB(
          heat,
          heat / 4,
          0
        );

    }

    else if (heat < 180) {

      leds[i] =
        CRGB(
          255,
          heat / 3,
          0
        );

    }

    else {

      leds[i] =
        CRGB(
          255,
          heat / 2,
          heat / 8
        );
    }
  }

  showLEDs();
}

// ============================================================
// SPARKLE
// ============================================================

void sparkleEffect() {

  if (millis() - lastUpdate < 70)
    return;

  lastUpdate = millis();

  for (int i = 0; i < ledCount; i++) {

    leds[i].fadeToBlackBy(35);
  }

  int p =
    random(0, ledCount);

  leds[p] =
    CRGB::White;

  showLEDs();
}

// ============================================================
// PIXEL SCAN
// ============================================================

void pixelScanEffect() {

  if (!scanRunning)
    return;

  if (millis() - lastStep < 120)
    return;

  lastStep = millis();

  fill_solid(
    leds,
    ledCount,
    CRGB::Black
  );

  int position;

  if (!reverseScan) {

    position = scanPixel;

  } else {

    position =
      ledCount - 1 - scanPixel;
  }

  if (
    position >= 0 &&
    position < ledCount
  ) {

    leds[position] =
      CRGB::Red;
  }

  showLEDs();

  scanPixel++;

  if (scanPixel >= ledCount) {

    scanPixel = 0;

    scanRunning = false;

    currentMode = "OFF";

    clearLEDs();
  }
}

// ============================================================
// SECTION TEST
// ============================================================

void sectionTest() {

  fill_solid(
    leds,
    ledCount,
    CRGB::Black
  );

  int start =
    sectionStart - 1;

  int end =
    sectionEnd - 1;

  if (start < 0)
    start = 0;

  if (end >= ledCount)
    end = ledCount - 1;

  if (start > end)
    return;

  for (
    int i = start;
    i <= end;
    i++
  ) {

    leds[i] =
      CRGB::Green;
  }

  showLEDs();
}

// ============================================================
// AUTO TEST
// ============================================================

void autoTest() {

  static int stage = 0;

  static unsigned long stageTime = 0;

  if (millis() - stageTime < 1800)
    return;

  stageTime = millis();

  switch (stage) {

    case 0:

      solidColor(CRGB::Red);

      break;

    case 1:

      solidColor(CRGB::Green);

      break;

    case 2:

      solidColor(CRGB::Blue);

      break;

    case 3:

      solidColor(CRGB::White);

      break;

    case 4:

      currentMode = "CHASE";

      break;

    case 5:

      currentMode = "RAINBOW";

      break;

    case 6:

      currentMode = "FIRE";

      break;

    case 7:

      currentMode = "SPARKLE";

      break;

    case 8:

      currentMode = "OFF";

      clearLEDs();

      break;
  }

  stage++;

  if (stage > 8)
    stage = 0;
}

// ============================================================
// EFFECT ENGINE
// ============================================================

void runEffects() {

  if (currentMode == "OFF")
    return;

  if (currentMode == "SOLID")
    return;

  if (currentMode == "CHASE") {

    chaseEffect();

    return;
  }

  if (currentMode == "RAINBOW") {

    rainbowEffect();

    return;
  }

  if (currentMode == "FIRE") {

    fireEffect();

    return;
  }

  if (currentMode == "SPARKLE") {

    sparkleEffect();

    return;
  }

  if (currentMode == "PIXEL_SCAN") {

    pixelScanEffect();

    return;
  }

  if (currentMode == "SECTION") {

    sectionTest();

    currentMode = "OFF";

    return;
  }

  if (currentMode == "AUTO") {

    autoTest();

    return;
  }
}

// ============================================================
// HTML PAGE
// ============================================================

String htmlPage() {

  String page = R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta name="viewport"
content="width=device-width, initial-scale=1">

<title>Moorthy Smart Sign Tester</title>

<style>

body {
  background:#111;
  color:white;
  font-family:Arial;
  text-align:center;
  margin:0;
  padding:15px;
}

h1 {
  font-size:24px;
  margin-bottom:5px;
}

.card {
  background:#1e1e1e;
  border-radius:15px;
  padding:15px;
  margin:12px auto;
  max-width:600px;
}

button {
  border:0;
  border-radius:10px;
  padding:13px;
  margin:5px;
  font-size:15px;
  font-weight:bold;
}

.red {
  background:#e53935;
  color:white;
}

.green {
  background:#43a047;
  color:white;
}

.blue {
  background:#1e88e5;
  color:white;
}

.white {
  background:white;
  color:black;
}

.yellow {
  background:#ffd600;
  color:black;
}

.gold {
  background:#d4af37;
  color:black;
}

.cyan {
  background:#00bcd4;
  color:black;
}

.magenta {
  background:#e040fb;
  color:white;
}

.orange {
  background:#ff6d00;
  color:white;
}

.off {
  background:#444;
  color:white;
}

.effect {
  background:#333;
  color:white;
}

.danger {
  background:#b71c1c;
  color:white;
}

input {
  width:90%;
  padding:10px;
  margin:5px;
  border-radius:8px;
  border:1px solid #555;
  background:#222;
  color:white;
  font-size:16px;
}

select {
  width:90%;
  padding:12px;
  background:#222;
  color:white;
  border-radius:8px;
  font-size:16px;
}

.status {
  font-size:17px;
  line-height:1.7;
}

.voltage {
  font-size:32px;
  font-weight:bold;
  margin:10px;
}

</style>

</head>

<body>

<h1>MOORTHY SMART SIGN TESTER</h1>


<div class="card">

<h2>⚡ POWER MONITOR</h2>

<div class="voltage">

<span id="voltage">0.00</span> V

</div>

</div>


<div class="card">

<h2>LED TYPE</h2>

<select onchange="setType(this.value)">

<option value="CX1903">
CX1903
</option>

<option value="WS2811">
WS2811
</option>

</select>

</div>


<div class="card">

<h2>LED SETTINGS</h2>

<input
id="count"
type="number"
value="21"
min="1"
max="500"
placeholder="LED COUNT">

<br>

<input
id="brightness"
type="number"
value="100"
min="1"
max="255"
placeholder="BRIGHTNESS">

<br>

<button
class="effect"
onclick="applySettings()">

APPLY SETTINGS

</button>

</div>


<div class="card">

<h2>COLOURS</h2>

<button
class="red"
onclick="cmd('red')">
RED
</button>

<button
class="green"
onclick="cmd('green')">
GREEN
</button>

<button
class="blue"
onclick="cmd('blue')">
BLUE
</button>

<button
class="white"
onclick="cmd('white')">
WHITE
</button>

<button
class="yellow"
onclick="cmd('yellow')">
YELLOW
</button>

<button
class="gold"
onclick="cmd('gold')">
ROYAL GOLD
</button>

<button
class="cyan"
onclick="cmd('cyan')">
CYAN
</button>

<button
class="magenta"
onclick="cmd('magenta')">
MAGENTA
</button>

<button
class="orange"
onclick="cmd('orange')">
ORANGE
</button>

<button
class="off"
onclick="cmd('off')">
OFF
</button>

</div>


<div class="card">

<h2>EFFECTS</h2>

<button
class="effect"
onclick="cmd('chase')">

MULTI COLOUR CHASE

</button>

<br>

<button
class="effect"
onclick="cmd('rainbow')">

RAINBOW

</button>

<button
class="effect"
onclick="cmd('fire')">

FIRE 🔥

</button>

<button
class="effect"
onclick="cmd('sparkle')">

SPARKLE ✨

</button>

<br>

<button
class="effect"
onclick="cmd('auto')">

AUTO TEST

</button>

</div>


<div class="card">

<h2>DIAGNOSTICS</h2>

<input
id="start"
type="number"
value="1"
min="1"
max="500"
placeholder="START PIXEL">

<br>

<input
id="end"
type="number"
value="21"
min="1"
max="500"
placeholder="END PIXEL">

<br>

<button
class="effect"
onclick="scanForward()">

PIXEL SCAN →

</button>

<button
class="effect"
onclick="scanReverse()">

← REVERSE SCAN

</button>

<br>

<button
class="effect"
onclick="sectionTest()">

TEST SECTION

</button>

</div>


<div class="card">

<h2>STATUS</h2>

<div
class="status"
id="status">

Connecting...

</div>

</div>


<script>

function cmd(x) {

  fetch('/cmd?value=' + x);

}


function setType(x) {

  fetch('/type?value=' + x);

}


function applySettings() {

  let count =
    document.getElementById('count').value;

  let brightness =
    document.getElementById('brightness').value;

  fetch(
    '/settings?count='
    + count
    + '&brightness='
    + brightness
  );

}


function scanForward() {

  fetch('/scan?reverse=0');

}


function scanReverse() {

  fetch('/scan?reverse=1');

}


function sectionTest() {

  let start =
    document.getElementById('start').value;

  let end =
    document.getElementById('end').value;

  fetch(
    '/section?start='
    + start
    + '&end='
    + end
  );

}


function updateStatus() {

  fetch('/status')

  .then(response =>
    response.json()
  )

  .then(data => {

    document.getElementById(
      'voltage'
    ).innerHTML =
      data.voltage.toFixed(2);

    document.getElementById(
      'status'
    ).innerHTML =

      "LED TYPE: "
      + data.type

      + "<br>PROTOCOL: "
      + data.protocol

      + "<br>COLOUR ORDER: "
      + data.order

      + "<br>LED COUNT: "
      + data.count

      + "<br>BRIGHTNESS: "
      + data.brightness

      + "<br>MODE: "
      + data.mode;

  });

}


setInterval(
  updateStatus,
  500
);

updateStatus();

</script>

</body>

</html>

)rawliteral";

  return page;
}

// ============================================================
// WEB COMMAND
// ============================================================

void handleCommand() {

  String value =
    server.arg("value");

  if (value == "red") {

    solidColor(CRGB::Red);

    currentColor = "RED";
  }

  else if (value == "green") {

    solidColor(CRGB::Green);

    currentColor = "GREEN";
  }

  else if (value == "blue") {

    solidColor(CRGB::Blue);

    currentColor = "BLUE";
  }

  else if (value == "white") {

    solidColor(CRGB::White);

    currentColor = "WHITE";
  }

  else if (value == "yellow") {

    solidColor(CRGB::Yellow);

    currentColor = "YELLOW";
  }

  else if (value == "gold") {

    solidColor(
      CRGB(255, 150, 20)
    );

    currentColor = "ROYAL GOLD";
  }

  else if (value == "cyan") {

    solidColor(CRGB::Cyan);

    currentColor = "CYAN";
  }

  else if (value == "magenta") {

    solidColor(CRGB::Magenta);

    currentColor = "MAGENTA";
  }

  else if (value == "orange") {

    solidColor(CRGB::Orange);

    currentColor = "ORANGE";
  }

  else if (value == "off") {

    currentMode = "OFF";

    currentColor = "OFF";

    clearLEDs();
  }

  else if (value == "chase") {

    currentMode = "CHASE";

    chasePosition = 0;
  }

  else if (value == "rainbow") {

    currentMode = "RAINBOW";

    rainbowHue = 0;
  }

  else if (value == "fire") {

    currentMode = "FIRE";
  }

  else if (value == "sparkle") {

    currentMode = "SPARKLE";
  }

  else if (value == "auto") {

    currentMode = "AUTO";
  }

  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// ============================================================
// LED TYPE
// ============================================================

void handleType() {

  String value =
    server.arg("value");

  if (value == "WS2811") {

    currentLEDType =
      LED_WS2811;

  }

  else {

    currentLEDType =
      LED_CX1903;
  }

  selectLEDType();

  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// ============================================================
// SETTINGS
// ============================================================

void handleSettings() {

  if (server.hasArg("count")) {

    ledCount =
      server.arg("count").toInt();

    if (ledCount < 1)
      ledCount = 1;

    if (ledCount > MAX_LEDS)
      ledCount = MAX_LEDS;
  }

  if (server.hasArg("brightness")) {

    brightness =
      server.arg("brightness").toInt();

    if (brightness < 1)
      brightness = 1;

    if (brightness > 255)
      brightness = 255;
  }

  clearLEDs();

  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// ============================================================
// SCAN
// ============================================================

void handleScan() {

  reverseScan =
    server.arg("reverse") == "1";

  scanPixel = 0;

  scanRunning = true;

  currentMode =
    "PIXEL_SCAN";

  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// ============================================================
// SECTION
// ============================================================

void handleSection() {

  sectionStart =
    server.arg("start").toInt();

  sectionEnd =
    server.arg("end").toInt();

  if (sectionStart < 1)
    sectionStart = 1;

  if (sectionEnd > ledCount)
    sectionEnd = ledCount;

  currentMode =
    "SECTION";

  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// ============================================================
// STATUS
// ============================================================

void handleStatus() {

  String type;

  String protocol;

  if (
    currentLEDType ==
    LED_WS2811
  ) {

    type = "WS2811";

    protocol = "WS2811";

  }

  else {

    type = "CX1903";

    protocol = "UCS1903B";
  }

  String json = "{";

  json +=
    "\"type\":\""
    + type
    + "\",";

  json +=
    "\"protocol\":\""
    + protocol
    + "\",";

  json +=
    "\"order\":\"BRG\",";

  json +=
    "\"count\":"
    + String(ledCount)
    + ",";

  json +=
    "\"brightness\":"
    + String(brightness)
    + ",";

  json +=
    "\"mode\":\""
    + currentMode
    + "\",";

  json +=
    "\"voltage\":"
    + String(voltage, 3);

  json += "}";

  server.send(
    200,
    "application/json",
    json
  );
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

  delay(500);

  Serial.println();

  Serial.println(
    "=============================="
  );

  Serial.println(
    "MOORTHY SMART SIGN TESTER"
  );

  Serial.println(
    "VERSION 1.6"
  );

  Serial.println(
    "=============================="
  );

  // ----------------------------------------------------------
  // VOLTAGE ADC
  // ----------------------------------------------------------

  pinMode(
    VOLTAGE_PIN,
    INPUT
  );

  analogSetPinAttenuation(
    VOLTAGE_PIN,
    ADC_11db
  );

  Serial.println(
    "Voltage Sensor: GPIO35 / D35"
  );

  Serial.println(
    "Voltage Calibration: 1.000"
  );

  // ----------------------------------------------------------
  // LED CONTROLLERS
  // ----------------------------------------------------------

  wsController =
    &FastLED.addLeds<
      WS2811,
      LED_PIN,
      BRG
    >(
      leds,
      MAX_LEDS
    );

  cxController =
    &FastLED.addLeds<
      UCS1903B,
      LED_PIN,
      BRG
    >(
      leds,
      MAX_LEDS
    );

  // ----------------------------------------------------------
  // DEFAULT CX1903
  // ----------------------------------------------------------

  activeController =
    cxController;

  Serial.println(
    "Default LED TYPE: CX1903"
  );

  Serial.println(
    "Protocol: UCS1903B"
  );

  Serial.println(
    "Colour Order: BRG"
  );

  clearLEDs();

  // ----------------------------------------------------------
  // WIFI AP
  // ----------------------------------------------------------

  WiFi.mode(WIFI_AP);

  WiFi.softAP(
    "MOORTHY-TESTER",
    "12345678"
  );

  Serial.println();

  Serial.println(
    "WiFi AP Started"
  );

  Serial.println(
    "SSID: MOORTHY-TESTER"
  );

  Serial.println(
    "Password: 12345678"
  );

  Serial.print(
    "IP: "
  );

  Serial.println(
    WiFi.softAPIP()
  );

  // ----------------------------------------------------------
  // WEB ROUTES
  // ----------------------------------------------------------

  server.on(
    "/",
    []() {

      server.send(
        200,
        "text/html",
        htmlPage()
      );

    }
  );

  server.on(
    "/cmd",
    handleCommand
  );

  server.on(
    "/type",
    handleType
  );

  server.on(
    "/settings",
    handleSettings
  );

  server.on(
    "/scan",
    handleScan
  );

  server.on(
    "/section",
    handleSection
  );

  server.on(
    "/status",
    handleStatus
  );

  server.begin();

  Serial.println(
    "Web Server Started"
  );

  Serial.println(
    "=============================="
  );
}

// ============================================================
// LOOP
// ============================================================

void loop() {

  server.handleClient();

  readVoltage();

  runEffects();
}