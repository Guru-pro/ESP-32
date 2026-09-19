/*
  ============================================================
       MOORTHY SMART SIGN TESTER V2.1
       INDIVIDUAL LED COLOUR ORDER PROFILES
  ============================================================

  DATA PIN       : GPIO 5 / D5
  VOLTAGE SENSOR : GPIO 34 / D34
  CURRENT SENSOR : GPIO 35 / D35

  LED PROFILES:
  1. WS2811
  2. WS2811 3-LED MODULE
  3. ADDRESSABLE PIXEL
  4. CX1903

  EACH PROFILE HAS ITS OWN:
  - Colour order
  - Sections
  - Physical LEDs / section
  - Brightness

  COLOUR ORDERS:
  RGB / RBG / GRB / GBR / BRG / BGR

  ============================================================
*/

#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>

// ============================================================
// HARDWARE
// ============================================================

#define LED_PIN       5
#define VOLTAGE_PIN   34
#define CURRENT_PIN   35

#define MAX_SECTIONS  500

CRGB leds[MAX_SECTIONS];

WebServer server(80);

// ============================================================
// CONTROLLERS
// ============================================================

CLEDController *ctrlRGB = nullptr;
CLEDController *ctrlRBG = nullptr;
CLEDController *ctrlGRB = nullptr;
CLEDController *ctrlGBR = nullptr;
CLEDController *ctrlBRG = nullptr;
CLEDController *ctrlBGR = nullptr;

CLEDController *cxController = nullptr;

CLEDController *activeController = nullptr;

// ============================================================
// LED TYPES
// ============================================================

enum LED_TYPE_SELECT
{
  LED_WS2811,
  LED_WS2811_MODULE,
  LED_PIXEL,
  LED_CX1903
};

LED_TYPE_SELECT currentLEDType = LED_WS2811;

// ============================================================
// COLOUR ORDERS
// ============================================================

enum COLOUR_ORDER_SELECT
{
  ORDER_RGB,
  ORDER_RBG,
  ORDER_GRB,
  ORDER_GBR,
  ORDER_BRG,
  ORDER_BGR
};

// Individual profile colour orders

COLOUR_ORDER_SELECT wsColourOrder =
  ORDER_BRG;

COLOUR_ORDER_SELECT moduleColourOrder =
  ORDER_GRB;

COLOUR_ORDER_SELECT pixelColourOrder =
  ORDER_GRB;

COLOUR_ORDER_SELECT cxColourOrder =
  ORDER_BRG;

// Active colour order

COLOUR_ORDER_SELECT currentColourOrder =
  ORDER_BRG;

// ============================================================
// PROFILE SETTINGS
// ============================================================

// WS2811 STRIP

int wsSections = 7;
int wsLedsPerSection = 3;

// WS2811 3-LED MODULE

int moduleSections = 7;
int moduleLedsPerSection = 3;

// ADDRESSABLE PIXEL

int pixelSections = 7;
int pixelLedsPerSection = 1;

// CX1903

int cxSections = 7;
int cxLedsPerSection = 3;

// ============================================================
// ACTIVE SETTINGS
// ============================================================

int sectionCount = 7;
int physicalLEDsPerSection = 3;

int brightness = 100;

// ============================================================
// DIAGNOSTICS
// ============================================================

int sectionStart = 1;
int sectionEnd = 7;

int scanPixel = 0;

bool scanRunning = false;
bool reverseScan = false;

// ============================================================
// STATUS
// ============================================================

String currentMode = "OFF";
String currentColor = "OFF";

CRGB selectedColor = CRGB::Black;

// ============================================================
// TIMING
// ============================================================

unsigned long lastUpdate = 0;
unsigned long lastStep = 0;

int chasePosition = 0;

uint8_t rainbowHue = 0;

// ============================================================
// VOLTAGE
// ============================================================

float voltageDividerRatio = 5.0;

float voltageCalibration = 1.0;

// ============================================================
// CURRENT
// ============================================================

bool currentSensorEnabled = false;

float currentZeroVoltage = 2.50;

float currentSensitivity = 0.066;

float currentDividerRatio = 3.0;

// ============================================================
// TOTAL PHYSICAL LEDS
// ============================================================

int totalPhysicalLEDs()
{
  return sectionCount *
         physicalLEDsPerSection;
}

// ============================================================
// LED TYPE NAME
// ============================================================

String getLEDTypeName()
{
  if (currentLEDType == LED_WS2811)
    return "WS2811";

  if (currentLEDType == LED_WS2811_MODULE)
    return "WS2811 3-LED MODULE";

  if (currentLEDType == LED_PIXEL)
    return "ADDRESSABLE PIXEL";

  return "CX1903";
}

// ============================================================
// PROTOCOL NAME
// ============================================================

String getProtocolName()
{
  if (currentLEDType == LED_WS2811)
    return "WS2811";

  if (currentLEDType == LED_WS2811_MODULE)
    return "WS2811";

  if (currentLEDType == LED_PIXEL)
    return "WS2811 - VERIFY";

  return "UCS1903B";
}

// ============================================================
// COLOUR ORDER NAME
// ============================================================

String colourOrderName(
  COLOUR_ORDER_SELECT order
)
{
  switch (order)
  {
    case ORDER_RGB:
      return "RGB";

    case ORDER_RBG:
      return "RBG";

    case ORDER_GRB:
      return "GRB";

    case ORDER_GBR:
      return "GBR";

    case ORDER_BRG:
      return "BRG";

    case ORDER_BGR:
      return "BGR";
  }

  return "BRG";
}

// ============================================================
// ACTIVE COLOUR ORDER
// ============================================================

String getColourOrder()
{
  return colourOrderName(
    currentColourOrder
  );
}

// ============================================================
// PROFILE VOLTAGE
// ============================================================

String getProfileVoltage()
{
  if (currentLEDType == LED_PIXEL)
    return "VERIFY";

  return "12V";
}

// ============================================================
// LOAD PROFILE
// ============================================================

void loadProfile()
{
  if (currentLEDType == LED_WS2811)
  {
    sectionCount =
      wsSections;

    physicalLEDsPerSection =
      wsLedsPerSection;

    currentColourOrder =
      wsColourOrder;
  }

  else if (
    currentLEDType ==
    LED_WS2811_MODULE
  )
  {
    sectionCount =
      moduleSections;

    physicalLEDsPerSection =
      moduleLedsPerSection;

    currentColourOrder =
      moduleColourOrder;
  }

  else if (
    currentLEDType ==
    LED_PIXEL
  )
  {
    sectionCount =
      pixelSections;

    physicalLEDsPerSection =
      pixelLedsPerSection;

    currentColourOrder =
      pixelColourOrder;
  }

  else
  {
    sectionCount =
      cxSections;

    physicalLEDsPerSection =
      cxLedsPerSection;

    currentColourOrder =
      cxColourOrder;
  }

  sectionStart = 1;

  sectionEnd =
    sectionCount;
}

// ============================================================
// SAVE PROFILE
// ============================================================

void saveProfile()
{
  if (currentLEDType == LED_WS2811)
  {
    wsSections =
      sectionCount;

    wsLedsPerSection =
      physicalLEDsPerSection;

    wsColourOrder =
      currentColourOrder;
  }

  else if (
    currentLEDType ==
    LED_WS2811_MODULE
  )
  {
    moduleSections =
      sectionCount;

    moduleLedsPerSection =
      physicalLEDsPerSection;

    moduleColourOrder =
      currentColourOrder;
  }

  else if (
    currentLEDType ==
    LED_PIXEL
  )
  {
    pixelSections =
      sectionCount;

    pixelLedsPerSection =
      physicalLEDsPerSection;

    pixelColourOrder =
      currentColourOrder;
  }

  else
  {
    cxSections =
      sectionCount;

    cxLedsPerSection =
      physicalLEDsPerSection;

    cxColourOrder =
      currentColourOrder;
  }
}

// ============================================================
// SELECT CONTROLLER
// ============================================================

void selectController()
{
  // CX1903

  if (
    currentLEDType ==
    LED_CX1903
  )
  {
    activeController =
      cxController;

    return;
  }

  // WS2811 family

  switch (
    currentColourOrder
  )
  {
    case ORDER_RGB:
      activeController =
        ctrlRGB;
      break;

    case ORDER_RBG:
      activeController =
        ctrlRBG;
      break;

    case ORDER_GRB:
      activeController =
        ctrlGRB;
      break;

    case ORDER_GBR:
      activeController =
        ctrlGBR;
      break;

    case ORDER_BRG:
      activeController =
        ctrlBRG;
      break;

    case ORDER_BGR:
      activeController =
        ctrlBGR;
      break;
  }
}

// ============================================================
// SHOW
// ============================================================

void showLEDs()
{
  if (
    activeController !=
    nullptr
  )
  {
    activeController->showLeds(
      brightness
    );
  }
}

// ============================================================
// CLEAR
// ============================================================

void clearLEDs()
{
  fill_solid(
    leds,
    MAX_SECTIONS,
    CRGB::Black
  );

  showLEDs();
}

// ============================================================
// SELECT LED TYPE
// ============================================================

void selectLEDType()
{
  loadProfile();

  selectController();

  Serial.println();
  Serial.println(
    "------------------------------"
  );

  Serial.print(
    "LED TYPE : "
  );

  Serial.println(
    getLEDTypeName()
  );

  Serial.print(
    "PROTOCOL : "
  );

  Serial.println(
    getProtocolName()
  );

  Serial.print(
    "ORDER    : "
  );

  Serial.println(
    getColourOrder()
  );

  Serial.print(
    "VOLTAGE  : "
  );

  Serial.println(
    getProfileVoltage()
  );

  Serial.println(
    "------------------------------"
  );

  clearLEDs();
}

// ============================================================
// SOLID
// ============================================================

void solidColor(
  CRGB color
)
{
  currentMode =
    "SOLID";

  selectedColor =
    color;

  fill_solid(
    leds,
    MAX_SECTIONS,
    CRGB::Black
  );

  for (
    int i = 0;
    i < sectionCount;
    i++
  )
  {
    leds[i] =
      color;
  }

  showLEDs();
}

// ============================================================
// CHASE
// ============================================================

void chaseEffect()
{
  if (
    sectionCount <= 0
  )
    return;

  if (
    millis() -
    lastStep <
    70
  )
    return;

  lastStep =
    millis();

  fill_solid(
    leds,
    MAX_SECTIONS,
    CRGB::Black
  );

  CRGB colours[] =
  {
    CRGB::Red,
    CRGB::Yellow,
    CRGB::Green,
    CRGB::Blue,
    CRGB::Magenta,
    CRGB::Cyan,
    CRGB::Orange
  };

  for (
    int tail = 0;
    tail < 4;
    tail++
  )
  {
    int pos =
      chasePosition -
      tail;

    while (
      pos < 0
    )
      pos +=
        sectionCount;

    leds[
      pos %
      sectionCount
    ] =
      colours[
        (
          chasePosition +
          tail
        ) % 7
      ];
  }

  showLEDs();

  chasePosition++;

  if (
    chasePosition >=
    sectionCount
  )
  {
    chasePosition = 0;
  }
}

// ============================================================
// RAINBOW
// ============================================================

void rainbowEffect()
{
  if (
    sectionCount <= 0
  )
    return;

  if (
    millis() -
    lastUpdate <
    30
  )
    return;

  lastUpdate =
    millis();

  fill_solid(
    leds,
    MAX_SECTIONS,
    CRGB::Black
  );

  for (
    int i = 0;
    i < sectionCount;
    i++
  )
  {
    leds[i] =
      CHSV(
        rainbowHue +
        (
          (
            i * 255
          ) /
          sectionCount
        ),
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

void fireEffect()
{
  if (
    sectionCount <= 0
  )
    return;

  if (
    millis() -
    lastUpdate <
    45
  )
    return;

  lastUpdate =
    millis();

  for (
    int i = 0;
    i < sectionCount;
    i++
  )
  {
    byte heat =
      random8(
        80,
        255
      );

    if (
      heat < 100
    )
    {
      leds[i] =
        CRGB(
          heat,
          heat / 4,
          0
        );
    }

    else if (
      heat < 180
    )
    {
      leds[i] =
        CRGB(
          255,
          heat / 3,
          0
        );
    }

    else
    {
      leds[i] =
        CRGB(
          255,
          heat / 2,
          heat / 8
        );
    }
  }

  for (
    int i =
      sectionCount;

    i <
      MAX_SECTIONS;

    i++
  )
  {
    leds[i] =
      CRGB::Black;
  }

  showLEDs();
}

// ============================================================
// SPARKLE
// ============================================================

void sparkleEffect()
{
  if (
    sectionCount <= 0
  )
    return;

  if (
    millis() -
    lastUpdate <
    70
  )
    return;

  lastUpdate =
    millis();

  for (
    int i = 0;
    i < sectionCount;
    i++
  )
  {
    leds[i].fadeToBlackBy(
      35
    );
  }

  for (
    int i =
      sectionCount;

    i <
      MAX_SECTIONS;

    i++
  )
  {
    leds[i] =
      CRGB::Black;
  }

  int p =
    random(
      0,
      sectionCount
    );

  leds[p] =
    CRGB::White;

  showLEDs();
}

// ============================================================
// SECTION SCAN
// ============================================================

void pixelScanEffect()
{
  if (!scanRunning)
    return;

  if (
    sectionCount <= 0
  )
    return;

  if (
    millis() -
    lastStep <
    150
  )
    return;

  lastStep =
    millis();

  fill_solid(
    leds,
    MAX_SECTIONS,
    CRGB::Black
  );

  int rangeCount =
    sectionEnd -
    sectionStart +
    1;

  int position;

  if (!reverseScan)
  {
    position =
      (
        sectionStart -
        1
      ) +
      scanPixel;
  }

  else
  {
    position =
      (
        sectionEnd -
        1
      ) -
      scanPixel;
  }

  if (
    position >= 0 &&
    position < sectionCount
  )
  {
    leds[position] =
      CRGB::Red;
  }

  showLEDs();

  scanPixel++;

  if (
    scanPixel >=
    rangeCount
  )
  {
    scanPixel = 0;

    scanRunning =
      false;

    currentMode =
      "OFF";

    clearLEDs();
  }
}

// ============================================================
// SECTION TEST
// ============================================================

void sectionTest()
{
  fill_solid(
    leds,
    MAX_SECTIONS,
    CRGB::Black
  );

  int start =
    sectionStart - 1;

  int end =
    sectionEnd - 1;

  if (
    start < 0
  )
    start = 0;

  if (
    end >= sectionCount
  )
    end =
      sectionCount - 1;

  if (
    start <= end
  )
  {
    for (
      int i = start;
      i <= end;
      i++
    )
    {
      leds[i] =
        CRGB::Green;
    }
  }

  showLEDs();
}

// ============================================================
// AUTO TEST
// ============================================================

void autoTest()
{
  static int stage = 0;

  static unsigned long stageTime =
    0;

  if (
    millis() -
    stageTime <
    1800
  )
    return;

  stageTime =
    millis();

  switch (stage)
  {
    case 0:
      solidColor(
        CRGB::Red
      );
      break;

    case 1:
      solidColor(
        CRGB::Green
      );
      break;

    case 2:
      solidColor(
        CRGB::Blue
      );
      break;

    case 3:
      solidColor(
        CRGB::White
      );
      break;

    case 4:
      currentMode =
        "CHASE";

      chasePosition = 0;
      break;

    case 5:
      currentMode =
        "RAINBOW";

      rainbowHue = 0;
      break;

    case 6:
      currentMode =
        "FIRE";
      break;

    case 7:
      currentMode =
        "SPARKLE";
      break;

    case 8:
      currentMode =
        "OFF";

      clearLEDs();
      break;
  }

  stage++;

  if (
    stage > 8
  )
    stage = 0;
}

// ============================================================
// VOLTAGE
// ============================================================

float readVoltage()
{
  long totalMilliVolts =
    0;

  const int samples =
    20;

  for (
    int i = 0;
    i < samples;
    i++
  )
  {
    totalMilliVolts +=
      analogReadMilliVolts(
        VOLTAGE_PIN
      );

    delayMicroseconds(
      300
    );
  }

  float sensorVoltage =
    (
      totalMilliVolts /
      (float)samples
    ) /
    1000.0;

  float supplyVoltage =
    sensorVoltage *
    voltageDividerRatio;

  supplyVoltage *=
    voltageCalibration;

  if (
    supplyVoltage <
    0.05
  )
    supplyVoltage = 0.0;

  return supplyVoltage;
}

// ============================================================
// CURRENT
// ============================================================

float readCurrent()
{
  if (
    !currentSensorEnabled
  )
    return 0.0;

  long totalMilliVolts =
    0;

  const int samples =
    50;

  for (
    int i = 0;
    i < samples;
    i++
  )
  {
    totalMilliVolts +=
      analogReadMilliVolts(
        CURRENT_PIN
      );

    delayMicroseconds(
      300
    );
  }

  float adcVoltage =
    (
      totalMilliVolts /
      (float)samples
    ) /
    1000.0;

  float sensorVoltage =
    adcVoltage *
    currentDividerRatio;

  float current =
    (
      sensorVoltage -
      currentZeroVoltage
    ) /
    currentSensitivity;

  if (
    current <
    0.05
  )
    current = 0.0;

  return current;
}

// ============================================================
// EFFECT ENGINE
// ============================================================

void runEffects()
{
  if (
    currentMode ==
    "OFF"
  )
    return;

  if (
    currentMode ==
    "SOLID"
  )
    return;

  if (
    currentMode ==
    "CHASE"
  )
  {
    chaseEffect();
    return;
  }

  if (
    currentMode ==
    "RAINBOW"
  )
  {
    rainbowEffect();
    return;
  }

  if (
    currentMode ==
    "FIRE"
  )
  {
    fireEffect();
    return;
  }

  if (
    currentMode ==
    "SPARKLE"
  )
  {
    sparkleEffect();
    return;
  }

  if (
    currentMode ==
    "PIXEL_SCAN"
  )
  {
    pixelScanEffect();
    return;
  }

  if (
    currentMode ==
    "SECTION"
  )
  {
    sectionTest();

    currentMode =
      "OFF";

    return;
  }

  if (
    currentMode ==
    "AUTO"
  )
  {
    autoTest();
  }
}

// ============================================================
// HTML
// ============================================================

String htmlPage()
{
  String page =
R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta charset="UTF-8">

<meta
name="viewport"
content="width=device-width, initial-scale=1">

<title>
Moorthy Smart Sign Tester V2.1
</title>

<style>

body {
  background:#111;
  color:white;
  font-family:Arial,sans-serif;
  text-align:center;
  margin:0;
  padding:15px;
}

h1 {
  font-size:24px;
}

.card {
  background:#1e1e1e;
  border-radius:15px;
  padding:15px;
  margin:12px auto;
  max-width:600px;
  box-sizing:border-box;
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

.off,
.effect {
  background:#333;
  color:white;
}

input,
select {
  width:90%;
  padding:12px;
  margin:5px;
  border-radius:8px;
  border:1px solid #555;
  background:#222;
  color:white;
  font-size:18px;
  box-sizing:border-box;
}

.status {
  font-size:17px;
  line-height:1.8;
}

.powerGrid {
  display:grid;
  grid-template-columns:
    repeat(3, 1fr);
  gap:8px;
}

.powerBox {
  background:#292929;
  border-radius:10px;
  padding:10px;
}

.powerValue {
  font-size:24px;
  font-weight:bold;
}

.powerLabel {
  font-size:12px;
  color:#aaa;
}

.label {
  font-size:14px;
  color:#aaa;
  margin-top:10px;
}

.total {
  font-size:20px;
  font-weight:bold;
  margin:10px;
}

.profileInfo {
  color:#aaa;
  font-size:14px;
  margin-top:8px;
}

</style>

</head>

<body>

<h1>
MOORTHY SMART SIGN TESTER
</h1>

<div class="card">

<h2>POWER MONITOR</h2>

<div class="powerGrid">

<div class="powerBox">

<div class="powerLabel">
VOLTAGE
</div>

<div class="powerValue">
<span id="voltage">0.00</span> V
</div>

</div>

<div class="powerBox">

<div class="powerLabel">
CURRENT
</div>

<div class="powerValue">
<span id="current">0.00</span> A
</div>

</div>

<div class="powerBox">

<div class="powerLabel">
POWER
</div>

<div class="powerValue">
<span id="power">0.00</span> W
</div>

</div>

</div>

<div class="profileInfo">
Current sensor:
<span id="currentSensor">
NOT ENABLED
</span>
</div>

</div>


<div class="card">

<h2>LED TYPE</h2>

<select
id="ledType"
onchange="setType(this.value)">

<option value="WS2811">
WS2811
</option>

<option value="WS2811_MODULE">
WS2811 3-LED MODULE
</option>

<option value="PIXEL">
ADDRESSABLE PIXEL
</option>

<option value="CX1903">
CX1903
</option>

</select>

<div
class="profileInfo"
id="profileInfo">
12V | WS2811 | BRG
</div>

</div>


<div class="card">

<h2>COLOUR ORDER</h2>

<select
id="colourOrder"
onchange="setColourOrder(this.value)">

<option value="RGB">RGB</option>
<option value="RBG">RBG</option>
<option value="GRB">GRB</option>
<option value="GBR">GBR</option>
<option value="BRG">BRG</option>
<option value="BGR">BGR</option>

</select>

<div class="profileInfo">
Each LED type has its own colour order.
</div>

</div>


<div class="card">

<h2>LED SETTINGS</h2>

<div class="label">
ADDRESSABLE SECTIONS / PIXELS
</div>

<input
id="count"
type="number"
value="7"
min="1"
max="500">

<div class="label">
PHYSICAL LEDs / SECTION
</div>

<input
id="ledsPerSection"
type="number"
value="3"
min="1"
max="20">

<div class="total">

TOTAL PHYSICAL LEDs:
<span id="totalLEDs">21</span>

</div>

<div class="label">
BRIGHTNESS
</div>

<input
id="brightness"
type="number"
value="100"
min="1"
max="255">

<br>

<button
class="effect"
onclick="applySettings()">

APPLY SETTINGS

</button>

</div>


<div class="card">

<h2>COLOURS</h2>

<button class="red"
onclick="cmd('red')">
RED
</button>

<button class="green"
onclick="cmd('green')">
GREEN
</button>

<button class="blue"
onclick="cmd('blue')">
BLUE
</button>

<button class="white"
onclick="cmd('white')">
WHITE
</button>

<button class="yellow"
onclick="cmd('yellow')">
YELLOW
</button>

<button class="gold"
onclick="cmd('gold')">
ROYAL GOLD
</button>

<button class="cyan"
onclick="cmd('cyan')">
CYAN
</button>

<button class="magenta"
onclick="cmd('magenta')">
MAGENTA
</button>

<button class="orange"
onclick="cmd('orange')">
ORANGE
</button>

<button class="off"
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

<div class="label">
START SECTION / PIXEL
</div>

<input
id="start"
type="number"
value="1"
min="1"
max="500">

<div class="label">
END SECTION / PIXEL
</div>

<input
id="end"
type="number"
value="7"
min="1"
max="500">

<br>

<button
class="effect"
onclick="scanForward()">

SECTION SCAN →

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

function cmd(x)
{
  fetch(
    '/cmd?value=' + x
  );
}


// ==========================================================
// TYPE
// ==========================================================

function setType(x)
{
  fetch(
    '/type?value=' + x
  )
  .then(() =>
  {
    updateStatus(true);
  });
}


// ==========================================================
// COLOUR ORDER
// ==========================================================

function setColourOrder(x)
{
  fetch(
    '/order?value=' + x
  )
  .then(() =>
  {
    updateStatus(true);
  });
}


// ==========================================================
// TOTAL
// ==========================================================

function updateTotal()
{
  let count =
    parseInt(
      document.getElementById(
        'count'
      ).value
    ) || 0;

  let leds =
    parseInt(
      document.getElementById(
        'ledsPerSection'
      ).value
    ) || 0;

  document.getElementById(
    'totalLEDs'
  ).innerHTML =
    count * leds;
}


document.getElementById(
  'count'
)
.addEventListener(
  'input',
  updateTotal
);


document.getElementById(
  'ledsPerSection'
)
.addEventListener(
  'input',
  updateTotal
);


// ==========================================================
// APPLY SETTINGS
// ==========================================================

function applySettings()
{
  let count =
    document.getElementById(
      'count'
    ).value;

  let leds =
    document.getElementById(
      'ledsPerSection'
    ).value;

  let bright =
    document.getElementById(
      'brightness'
    ).value;

  fetch(
    '/settings?count='
    + count
    + '&leds='
    + leds
    + '&brightness='
    + bright
  )
  .then(() =>
  {
    updateTotal();
    updateStatus(true);
  });
}


// ==========================================================
// DIAGNOSTIC RANGE
// ==========================================================

function getDiagnosticRange()
{
  let start =
    parseInt(
      document.getElementById(
        'start'
      ).value
    ) || 1;

  let end =
    parseInt(
      document.getElementById(
        'end'
      ).value
    ) || 1;

  return {
    start:start,
    end:end
  };
}


// ==========================================================
// FORWARD
// ==========================================================

function scanForward()
{
  let r =
    getDiagnosticRange();

  fetch(
    '/scan?reverse=0'
    + '&start='
    + r.start
    + '&end='
    + r.end
  );
}


// ==========================================================
// REVERSE
// ==========================================================

function scanReverse()
{
  let r =
    getDiagnosticRange();

  fetch(
    '/scan?reverse=1'
    + '&start='
    + r.start
    + '&end='
    + r.end
  );
}


// ==========================================================
// SECTION TEST
// ==========================================================

function sectionTest()
{
  let r =
    getDiagnosticRange();

  fetch(
    '/section?start='
    + r.start
    + '&end='
    + r.end
  );
}


// ==========================================================
// STATUS
// ==========================================================

function updateStatus(
  updateInputs
)
{
  fetch('/status')

  .then(
    response =>
      response.json()
  )

  .then(
    data =>
    {
      document.getElementById(
        'voltage'
      ).innerHTML =
        data.voltage.toFixed(2);

      document.getElementById(
        'current'
      ).innerHTML =
        data.current.toFixed(2);

      document.getElementById(
        'power'
      ).innerHTML =
        data.power.toFixed(2);

      document.getElementById(
        'currentSensor'
      ).innerHTML =
        data.currentSensor;

      if (
        updateInputs === true
      )
      {
        document.getElementById(
          'ledType'
        ).value =
          data.typeID;

        document.getElementById(
          'colourOrder'
        ).value =
          data.order;

        document.getElementById(
          'count'
        ).value =
          data.sections;

        document.getElementById(
          'ledsPerSection'
        ).value =
          data.ledsPerSection;

        document.getElementById(
          'brightness'
        ).value =
          data.brightness;

        document.getElementById(
          'start'
        ).value =
          data.sectionStart;

        document.getElementById(
          'end'
        ).value =
          data.sectionEnd;

        updateTotal();
      }

      document.getElementById(
        'profileInfo'
      ).innerHTML =
        data.profileVoltage
        + " | "
        + data.protocol
        + " | "
        + data.order;

      document.getElementById(
        'status'
      ).innerHTML =

        "LED TYPE: "
        + data.type

        + "<br>PROTOCOL: "
        + data.protocol

        + "<br>COLOUR ORDER: "
        + data.order

        + "<br>PROFILE VOLTAGE: "
        + data.profileVoltage

        + "<br>ADDRESSABLE SECTIONS: "
        + data.sections

        + "<br>PHYSICAL LEDs/SECTION: "
        + data.ledsPerSection

        + "<br>TOTAL PHYSICAL LEDs: "
        + data.totalLEDs

        + "<br>BRIGHTNESS: "
        + data.brightness

        + "<br>SCAN RANGE: "
        + data.sectionStart
        + " → "
        + data.sectionEnd

        + "<br>MODE: "
        + data.mode;
    }
  )

  .catch(
    error =>
    {
      document.getElementById(
        'status'
      ).innerHTML =
        "Connection lost";
    }
  );
}


// ==========================================================
// REFRESH
// ==========================================================

setInterval(
  function()
  {
    updateStatus(false);
  },
  1000
);


// ==========================================================
// INITIAL
// ==========================================================

updateStatus(true);

updateTotal();

</script>

</body>

</html>

)rawliteral";

  return page;
}

// ============================================================
// COMMAND HANDLER
// ============================================================

void handleCommand()
{
  String value =
    server.arg(
      "value"
    );

  if (
    value == "red"
  )
  {
    solidColor(
      CRGB::Red
    );

    currentColor =
      "RED";
  }

  else if (
    value == "green"
  )
  {
    solidColor(
      CRGB::Green
    );

    currentColor =
      "GREEN";
  }

  else if (
    value == "blue"
  )
  {
    solidColor(
      CRGB::Blue
    );

    currentColor =
      "BLUE";
  }

  else if (
    value == "white"
  )
  {
    solidColor(
      CRGB::White
    );

    currentColor =
      "WHITE";
  }

  else if (
    value == "yellow"
  )
  {
    solidColor(
      CRGB::Yellow
    );

    currentColor =
      "YELLOW";
  }

  else if (
    value == "gold"
  )
  {
    solidColor(
      CRGB(
        255,
        150,
        20
      )
    );

    currentColor =
      "ROYAL GOLD";
  }

  else if (
    value == "cyan"
  )
  {
    solidColor(
      CRGB::Cyan
    );

    currentColor =
      "CYAN";
  }

  else if (
    value == "magenta"
  )
  {
    solidColor(
      CRGB::Magenta
    );

    currentColor =
      "MAGENTA";
  }

  else if (
    value == "orange"
  )
  {
    solidColor(
      CRGB::Orange
    );

    currentColor =
      "ORANGE";
  }

  else if (
    value == "off"
  )
  {
    currentMode =
      "OFF";

    currentColor =
      "OFF";

    scanRunning =
      false;

    clearLEDs();
  }

  else if (
    value == "chase"
  )
  {
    currentMode =
      "CHASE";

    chasePosition =
      0;

    lastStep =
      0;
  }

  else if (
    value == "rainbow"
  )
  {
    currentMode =
      "RAINBOW";

    rainbowHue =
      0;

    lastUpdate =
      0;
  }

  else if (
    value == "fire"
  )
  {
    currentMode =
      "FIRE";

    lastUpdate =
      0;
  }

  else if (
    value == "sparkle"
  )
  {
    currentMode =
      "SPARKLE";

    lastUpdate =
      0;
  }

  else if (
    value == "auto"
  )
  {
    currentMode =
      "AUTO";
  }

  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// ============================================================
// TYPE HANDLER
// ============================================================

void handleType()
{
  String value =
    server.arg(
      "value"
    );

  saveProfile();

  if (
    value ==
    "WS2811"
  )
  {
    currentLEDType =
      LED_WS2811;
  }

  else if (
    value ==
    "WS2811_MODULE"
  )
  {
    currentLEDType =
      LED_WS2811_MODULE;
  }

  else if (
    value ==
    "PIXEL"
  )
  {
    currentLEDType =
      LED_PIXEL;
  }

  else
  {
    currentLEDType =
      LED_CX1903;
  }

  scanRunning =
    false;

  currentMode =
    "OFF";

  selectLEDType();

  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// ============================================================
// COLOUR ORDER HANDLER
// ============================================================

void handleColourOrder()
{
  String value =
    server.arg(
      "value"
    );

  if (
    value == "RGB"
  )
    currentColourOrder =
      ORDER_RGB;

  else if (
    value == "RBG"
  )
    currentColourOrder =
      ORDER_RBG;

  else if (
    value == "GRB"
  )
    currentColourOrder =
      ORDER_GRB;

  else if (
    value == "GBR"
  )
    currentColourOrder =
      ORDER_GBR;

  else if (
    value == "BRG"
  )
    currentColourOrder =
      ORDER_BRG;

  else if (
    value == "BGR"
  )
    currentColourOrder =
      ORDER_BGR;

  selectController();

  saveProfile();

  currentMode =
    "OFF";

  clearLEDs();

  Serial.print(
    "COLOUR ORDER CHANGED: "
  );

  Serial.println(
    getColourOrder()
  );

  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// ============================================================
// SETTINGS HANDLER
// ============================================================

void handleSettings()
{
  if (
    server.hasArg(
      "count"
    )
  )
  {
    sectionCount =
      server.arg(
        "count"
      ).toInt();

    if (
      sectionCount < 1
    )
      sectionCount = 1;

    if (
      sectionCount >
      MAX_SECTIONS
    )
      sectionCount =
        MAX_SECTIONS;
  }

  if (
    server.hasArg(
      "leds"
    )
  )
  {
    physicalLEDsPerSection =
      server.arg(
        "leds"
      ).toInt();

    if (
      physicalLEDsPerSection <
      1
    )
      physicalLEDsPerSection =
        1;

    if (
      physicalLEDsPerSection >
      20
    )
      physicalLEDsPerSection =
        20;
  }

  if (
    server.hasArg(
      "brightness"
    )
  )
  {
    brightness =
      server.arg(
        "brightness"
      ).toInt();

    if (
      brightness < 1
    )
      brightness = 1;

    if (
      brightness > 255
    )
      brightness = 255;
  }

  saveProfile();

  sectionStart =
    1;

  sectionEnd =
    sectionCount;

  scanRunning =
    false;

  currentMode =
    "OFF";

  clearLEDs();

  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// ============================================================
// SCAN HANDLER
// ============================================================

void handleScan()
{
  if (
    server.hasArg(
      "start"
    )
  )
  {
    sectionStart =
      server.arg(
        "start"
      ).toInt();
  }

  if (
    server.hasArg(
      "end"
    )
  )
  {
    sectionEnd =
      server.arg(
        "end"
      ).toInt();
  }

  if (
    sectionStart < 1
  )
    sectionStart = 1;

  if (
    sectionEnd < 1
  )
    sectionEnd = 1;

  if (
    sectionStart >
    sectionCount
  )
    sectionStart =
      sectionCount;

  if (
    sectionEnd >
    sectionCount
  )
    sectionEnd =
      sectionCount;

  if (
    sectionStart >
    sectionEnd
  )
  {
    int temp =
      sectionStart;

    sectionStart =
      sectionEnd;

    sectionEnd =
      temp;
  }

  reverseScan =
    server.arg(
      "reverse"
    ) == "1";

  scanPixel =
    0;

  scanRunning =
    true;

  currentMode =
    "PIXEL_SCAN";

  lastStep =
    0;

  clearLEDs();

  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// ============================================================
// SECTION HANDLER
// ============================================================

void handleSection()
{
  sectionStart =
    server.arg(
      "start"
    ).toInt();

  sectionEnd =
    server.arg(
      "end"
    ).toInt();

  if (
    sectionStart < 1
  )
    sectionStart = 1;

  if (
    sectionEnd < 1
  )
    sectionEnd = 1;

  if (
    sectionStart >
    sectionCount
  )
    sectionStart =
      sectionCount;

  if (
    sectionEnd >
    sectionCount
  )
    sectionEnd =
      sectionCount;

  if (
    sectionStart >
    sectionEnd
  )
  {
    int temp =
      sectionStart;

    sectionStart =
      sectionEnd;

    sectionEnd =
      temp;
  }

  scanRunning =
    false;

  currentMode =
    "SECTION";

  server.send(
    200,
    "text/plain",
    "OK"
  );
}

// ============================================================
// STATUS HANDLER
// ============================================================

void handleStatus()
{
  float voltage =
    readVoltage();

  float current =
    readCurrent();

  float power =
    voltage *
    current;

  String json =
    "{";

  json +=
    "\"type\":\""
    + getLEDTypeName()
    + "\",";

  json +=
    "\"typeID\":\"";

  if (
    currentLEDType ==
    LED_WS2811
  )
    json +=
      "WS2811";

  else if (
    currentLEDType ==
    LED_WS2811_MODULE
  )
    json +=
      "WS2811_MODULE";

  else if (
    currentLEDType ==
    LED_PIXEL
  )
    json +=
      "PIXEL";

  else
    json +=
      "CX1903";

  json +=
    "\",";

  json +=
    "\"protocol\":\""
    + getProtocolName()
    + "\",";

  json +=
    "\"order\":\""
    + getColourOrder()
    + "\",";

  json +=
    "\"profileVoltage\":\""
    + getProfileVoltage()
    + "\",";

  json +=
    "\"sections\":"
    + String(
      sectionCount
    )
    + ",";

  json +=
    "\"ledsPerSection\":"
    + String(
      physicalLEDsPerSection
    )
    + ",";

  json +=
    "\"totalLEDs\":"
    + String(
      totalPhysicalLEDs()
    )
    + ",";

  json +=
    "\"brightness\":"
    + String(
      brightness
    )
    + ",";

  json +=
    "\"sectionStart\":"
    + String(
      sectionStart
    )
    + ",";

  json +=
    "\"sectionEnd\":"
    + String(
      sectionEnd
    )
    + ",";

  json +=
    "\"voltage\":"
    + String(
      voltage,
      2
    )
    + ",";

  json +=
    "\"current\":"
    + String(
      current,
      2
    )
    + ",";

  json +=
    "\"power\":"
    + String(
      power,
      2
    )
    + ",";

  json +=
    "\"currentSensor\":\"";

  if (
    currentSensorEnabled
  )
    json +=
      "ENABLED";

  else
    json +=
      "NOT ENABLED";

  json +=
    "\",";

  json +=
    "\"mode\":\""
    + currentMode
    + "\"";

  json +=
    "}";

  server.send(
    200,
    "application/json",
    json
  );
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(
    115200
  );

  delay(500);

  Serial.println();

  Serial.println(
    "================================"
  );

  Serial.println(
    "MOORTHY SMART SIGN TESTER"
  );

  Serial.println(
    "VERSION 2.1"
  );

  Serial.println(
    "================================"
  );

  // ==========================================================
  // VOLTAGE SENSOR
  // ==========================================================

  pinMode(
    VOLTAGE_PIN,
    INPUT
  );

  analogReadResolution(
    12
  );

  analogSetPinAttenuation(
    VOLTAGE_PIN,
    ADC_11db
  );

  // ==========================================================
  // CURRENT SENSOR
  // ==========================================================

  pinMode(
    CURRENT_PIN,
    INPUT
  );

  analogSetPinAttenuation(
    CURRENT_PIN,
    ADC_11db
  );

  // ==========================================================
  // WS2811 RGB
  // ==========================================================

  ctrlRGB =
    &FastLED.addLeds<
      WS2811,
      LED_PIN,
      RGB
    >(
      leds,
      MAX_SECTIONS
    );

  // ==========================================================
  // WS2811 RBG
  // ==========================================================

  ctrlRBG =
    &FastLED.addLeds<
      WS2811,
      LED_PIN,
      RBG
    >(
      leds,
      MAX_SECTIONS
    );

  // ==========================================================
  // WS2811 GRB
  // ==========================================================

  ctrlGRB =
    &FastLED.addLeds<
      WS2811,
      LED_PIN,
      GRB
    >(
      leds,
      MAX_SECTIONS
    );

  // ==========================================================
  // WS2811 GBR
  // ==========================================================

  ctrlGBR =
    &FastLED.addLeds<
      WS2811,
      LED_PIN,
      GBR
    >(
      leds,
      MAX_SECTIONS
    );

  // ==========================================================
  // WS2811 BRG
  // ==========================================================

  ctrlBRG =
    &FastLED.addLeds<
      WS2811,
      LED_PIN,
      BRG
    >(
      leds,
      MAX_SECTIONS
    );

  // ==========================================================
  // WS2811 BGR
  // ==========================================================

  ctrlBGR =
    &FastLED.addLeds<
      WS2811,
      LED_PIN,
      BGR
    >(
      leds,
      MAX_SECTIONS
    );

  // ==========================================================
  // CX1903
  // ==========================================================

  cxController =
    &FastLED.addLeds<
      UCS1903B,
      LED_PIN,
      BRG
    >(
      leds,
      MAX_SECTIONS
    );

  // ==========================================================
  // DEFAULT
  // ==========================================================

  currentLEDType =
    LED_WS2811;

  currentColourOrder =
    wsColourOrder;

  activeController =
    ctrlBRG;

  loadProfile();

  selectController();

  clearLEDs();

  // ==========================================================
  // WIFI
  // ==========================================================

  WiFi.mode(
    WIFI_AP
  );

  bool apStarted =
    WiFi.softAP(
      "MOORTHY-TESTER",
      "12345678"
    );

  if (
    apStarted
  )
  {
    Serial.println(
      "WiFi AP Started"
    );

    Serial.println(
      "SSID     : MOORTHY-TESTER"
    );

    Serial.println(
      "Password : 12345678"
    );

    Serial.print(
      "IP       : "
    );

    Serial.println(
      WiFi.softAPIP()
    );
  }

  // ==========================================================
  // ROUTES
  // ==========================================================

  server.on(
    "/",
    []()
    {
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
    "/order",
    handleColourOrder
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
    "CURRENT SENSOR : NOT ENABLED"
  );

  Serial.println(
    "READY"
  );
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  server.handleClient();

  runEffects();
}