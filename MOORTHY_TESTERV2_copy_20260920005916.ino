/*
===========================================================
 MOORTHY AARTS
 SMART SIGN TESTER - ULTIMATE
 V2.2.0

 V2.1 = Manual LED Tester
 V2.2 = Professional Diagnostic Dashboard

 ESP32
 LED DATA       GPIO 5
 VOLTAGE SENSOR GPIO 34
 CURRENT SENSOR GPIO 35

 AP:
 MOORTHY-TESTER
 Password:
 12345678

===========================================================
*/

#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>
#include <Preferences.h>

// =========================================================
// HARDWARE
// =========================================================

#define LED_PIN       5
#define VOLTAGE_PIN   34
#define CURRENT_PIN   35

#define MAX_SECTIONS  500

WebServer server(80);
Preferences preferences;

// =========================================================
// LED DATA
// =========================================================

CRGB leds[MAX_SECTIONS];

// =========================================================
// LED TYPES
// =========================================================

enum LED_TYPE_SELECT
{
  TYPE_WS2811 = 0,
  TYPE_WS2811_MODULE,
  TYPE_ADDRESSABLE_PIXEL,
  TYPE_CX1903
};

LED_TYPE_SELECT currentLEDType = TYPE_WS2811;

// =========================================================
// COLOUR ORDER
// =========================================================

enum COLOUR_ORDER_SELECT
{
  ORDER_RGB = 0,
  ORDER_RBG,
  ORDER_GRB,
  ORDER_GBR,
  ORDER_BRG,
  ORDER_BGR
};

COLOUR_ORDER_SELECT wsColourOrder       = ORDER_BRG;
COLOUR_ORDER_SELECT moduleColourOrder   = ORDER_GRB;
COLOUR_ORDER_SELECT pixelColourOrder    = ORDER_GRB;
COLOUR_ORDER_SELECT cxColourOrder       = ORDER_BRG;

// =========================================================
// FASTLED CONTROLLERS
// =========================================================

CLEDController *ctrlRGB = nullptr;
CLEDController *ctrlRBG = nullptr;
CLEDController *ctrlGRB = nullptr;
CLEDController *ctrlGBR = nullptr;
CLEDController *ctrlBRG = nullptr;
CLEDController *ctrlBGR = nullptr;

CLEDController *cxController = nullptr;

CLEDController *activeController = nullptr;

// =========================================================
// PROFILE SETTINGS
// =========================================================

int wsSections = 7;
int wsLedsPerSection = 3;

int moduleSections = 7;
int moduleLedsPerSection = 3;

int pixelSections = 7;
int pixelLedsPerSection = 1;

int cxSections = 7;
int cxLedsPerSection = 3;

// Active settings
int sectionCount = 7;
int physicalLEDsPerSection = 3;
int brightness = 100;

// =========================================================
// DIAGNOSTIC RANGE
// =========================================================

int sectionStart = 1;
int sectionEnd = 7;

// =========================================================
// VOLTAGE
// =========================================================

float voltageDividerRatio = 5.0;
float voltageCalibration = 1.0;

// =========================================================
// CURRENT
// =========================================================

// Disabled for now because ACS712 is not connected
bool currentSensorEnabled = false;

float currentCalibration = 1.0;
float currentZero = 2.50;

// =========================================================
// EFFECT ENGINE
// =========================================================

String currentMode = "READY";

unsigned long effectTimer = 0;
int effectPosition = 0;

bool effectRunning = false;

// =========================================================
// DIAGNOSTIC ENGINE
// =========================================================

enum DIAGNOSTIC_STATE
{
  DIAG_IDLE,
  DIAG_RED,
  DIAG_GREEN,
  DIAG_BLUE,
  DIAG_WHITE,
  DIAG_SCAN_FORWARD,
  DIAG_SCAN_REVERSE,
  DIAG_COMPLETE
};

DIAGNOSTIC_STATE diagnosticState = DIAG_IDLE;

bool diagnosticRunning = false;

int diagnosticStep = 0;
unsigned long diagnosticTimer = 0;

String diagnosticMessage = "READY";
String diagnosticResult = "READY";

int diagnosticCurrentSection = 0;

// =========================================================
// WIFI
// =========================================================

const char *AP_SSID = "MOORTHY-TESTER";
const char *AP_PASSWORD = "12345678";

// =========================================================
// PROFILE HELPERS
// =========================================================

String getLEDTypeName()
{
  switch (currentLEDType)
  {
    case TYPE_WS2811:
      return "WS2811";

    case TYPE_WS2811_MODULE:
      return "WS2811 3-LED MODULE";

    case TYPE_ADDRESSABLE_PIXEL:
      return "ADDRESSABLE PIXEL";

    case TYPE_CX1903:
      return "CX1903";
  }

  return "UNKNOWN";
}

// ---------------------------------------------------------

String getProtocolName()
{
  switch (currentLEDType)
  {
    case TYPE_WS2811:
      return "WS2811";

    case TYPE_WS2811_MODULE:
      return "WS2811";

    case TYPE_ADDRESSABLE_PIXEL:
      return "WS2811";

    case TYPE_CX1903:
      return "UCS1903B";
  }

  return "UNKNOWN";
}

// ---------------------------------------------------------

String getColourOrderName(COLOUR_ORDER_SELECT order)
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

// ---------------------------------------------------------

COLOUR_ORDER_SELECT getCurrentColourOrder()
{
  switch (currentLEDType)
  {
    case TYPE_WS2811:
      return wsColourOrder;

    case TYPE_WS2811_MODULE:
      return moduleColourOrder;

    case TYPE_ADDRESSABLE_PIXEL:
      return pixelColourOrder;

    case TYPE_CX1903:
      return cxColourOrder;
  }

  return ORDER_BRG;
}

// ---------------------------------------------------------

String getCurrentColourOrderName()
{
  return getColourOrderName(getCurrentColourOrder());
}

// ---------------------------------------------------------

int getProfileSections()
{
  switch (currentLEDType)
  {
    case TYPE_WS2811:
      return wsSections;

    case TYPE_WS2811_MODULE:
      return moduleSections;

    case TYPE_ADDRESSABLE_PIXEL:
      return pixelSections;

    case TYPE_CX1903:
      return cxSections;
  }

  return 7;
}

// ---------------------------------------------------------

int getProfileLedsPerSection()
{
  switch (currentLEDType)
  {
    case TYPE_WS2811:
      return wsLedsPerSection;

    case TYPE_WS2811_MODULE:
      return moduleLedsPerSection;

    case TYPE_ADDRESSABLE_PIXEL:
      return pixelLedsPerSection;

    case TYPE_CX1903:
      return cxLedsPerSection;
  }

  return 3;
}

// =========================================================
// SAVE PROFILE
// =========================================================

void saveProfile()
{
  preferences.begin("mst", false);

  preferences.putInt("type", (int)currentLEDType);

  preferences.putInt("wsOrd", (int)wsColourOrder);
  preferences.putInt("modOrd", (int)moduleColourOrder);
  preferences.putInt("pixOrd", (int)pixelColourOrder);
  preferences.putInt("cxOrd", (int)cxColourOrder);

  preferences.putInt("wsSec", wsSections);
  preferences.putInt("wsLed", wsLedsPerSection);

  preferences.putInt("modSec", moduleSections);
  preferences.putInt("modLed", moduleLedsPerSection);

  preferences.putInt("pixSec", pixelSections);
  preferences.putInt("pixLed", pixelLedsPerSection);

  preferences.putInt("cxSec", cxSections);
  preferences.putInt("cxLed", cxLedsPerSection);

  preferences.putInt("brightness", brightness);

  preferences.end();
}

// =========================================================
// LOAD PROFILE
// =========================================================

void loadProfile()
{
  preferences.begin("mst", true);

  currentLEDType =
    (LED_TYPE_SELECT)preferences.getInt("type", TYPE_WS2811);

  wsColourOrder =
    (COLOUR_ORDER_SELECT)preferences.getInt("wsOrd", ORDER_BRG);

  moduleColourOrder =
    (COLOUR_ORDER_SELECT)preferences.getInt("modOrd", ORDER_GRB);

  pixelColourOrder =
    (COLOUR_ORDER_SELECT)preferences.getInt("pixOrd", ORDER_GRB);

  cxColourOrder =
    (COLOUR_ORDER_SELECT)preferences.getInt("cxOrd", ORDER_BRG);

  wsSections =
    preferences.getInt("wsSec", 7);

  wsLedsPerSection =
    preferences.getInt("wsLed", 3);

  moduleSections =
    preferences.getInt("modSec", 7);

  moduleLedsPerSection =
    preferences.getInt("modLed", 3);

  pixelSections =
    preferences.getInt("pixSec", 7);

  pixelLedsPerSection =
    preferences.getInt("pixLed", 1);

  cxSections =
    preferences.getInt("cxSec", 7);

  cxLedsPerSection =
    preferences.getInt("cxLed", 3);

  brightness =
    preferences.getInt("brightness", 100);

  preferences.end();
}

// =========================================================
// APPLY PROFILE
// =========================================================

void applyProfileSettings()
{
  sectionCount = getProfileSections();
  physicalLEDsPerSection = getProfileLedsPerSection();

  if (sectionCount < 1)
    sectionCount = 1;

  if (sectionCount > MAX_SECTIONS)
    sectionCount = MAX_SECTIONS;

  if (physicalLEDsPerSection < 1)
    physicalLEDsPerSection = 1;

  if (sectionStart < 1)
    sectionStart = 1;

  if (sectionEnd > sectionCount)
    sectionEnd = sectionCount;

  if (sectionStart > sectionEnd)
    sectionStart = sectionEnd;
}

// =========================================================
// SELECT CONTROLLER
// =========================================================

void selectController()
{
  if (currentLEDType == TYPE_CX1903)
  {
    activeController = cxController;
    return;
  }

  COLOUR_ORDER_SELECT order = getCurrentColourOrder();

  switch (order)
  {
    case ORDER_RGB:
      activeController = ctrlRGB;
      break;

    case ORDER_RBG:
      activeController = ctrlRBG;
      break;

    case ORDER_GRB:
      activeController = ctrlGRB;
      break;

    case ORDER_GBR:
      activeController = ctrlGBR;
      break;

    case ORDER_BRG:
      activeController = ctrlBRG;
      break;

    case ORDER_BGR:
      activeController = ctrlBGR;
      break;
  }
}

// =========================================================
// SHOW LEDS
// =========================================================

void showLEDs()
{
  if (activeController != nullptr)
  {
    activeController->showLeds(brightness);
  }
}

// =========================================================
// CLEAR
// =========================================================

void clearLEDs()
{
  fill_solid(leds, MAX_SECTIONS, CRGB::Black);
  showLEDs();
}

// =========================================================
// SET SECTION COLOUR
// =========================================================

void setSectionColour(int section, CRGB colour)
{
  if (section < 1 || section > sectionCount)
    return;

  leds[section - 1] = colour;
}

// =========================================================
// SOLID COLOUR
// =========================================================

void solidColor(CRGB colour)
{
  effectRunning = false;
  currentMode = "SOLID";

  for (int i = 0; i < sectionCount; i++)
  {
    leds[i] = colour;
  }

  showLEDs();
}

// =========================================================
// RED
// =========================================================

void showRed()
{
  solidColor(CRGB::Red);
}

// =========================================================
// GREEN
// =========================================================

void showGreen()
{
  solidColor(CRGB::Green);
}

// =========================================================
// BLUE
// =========================================================

void showBlue()
{
  solidColor(CRGB::Blue);
}

// =========================================================
// WHITE
// =========================================================

void showWhite()
{
  solidColor(CRGB::White);
}

// =========================================================
// YELLOW
// =========================================================

void showYellow()
{
  solidColor(CRGB::Yellow);
}

// =========================================================
// GOLD
// =========================================================

void showGold()
{
  solidColor(CRGB(255, 150, 20));
}

// =========================================================
// CYAN
// =========================================================

void showCyan()
{
  solidColor(CRGB::Cyan);
}

// =========================================================
// MAGENTA
// =========================================================

void showMagenta()
{
  solidColor(CRGB::Magenta);
}

// =========================================================
// ORANGE
// =========================================================

void showOrange()
{
  solidColor(CRGB(255, 70, 0));
}

// =========================================================
// OFF
// =========================================================

void showOff()
{
  effectRunning = false;
  currentMode = "OFF";

  clearLEDs();
}

// =========================================================
// CHASE
// =========================================================

void startChase()
{
  effectRunning = true;
  currentMode = "CHASE";
  effectPosition = 0;
  effectTimer = millis();
}

// =========================================================
// RAINBOW
// =========================================================

void startRainbow()
{
  effectRunning = true;
  currentMode = "RAINBOW";
  effectPosition = 0;
  effectTimer = millis();
}

// =========================================================
// FIRE
// =========================================================

void startFire()
{
  effectRunning = true;
  currentMode = "FIRE";
  effectTimer = millis();
}

// =========================================================
// SPARKLE
// =========================================================

void startSparkle()
{
  effectRunning = true;
  currentMode = "SPARKLE";
  effectTimer = millis();
}

// =========================================================
// CHASE EFFECT
// =========================================================

void runChase()
{
  if (millis() - effectTimer < 90)
    return;

  effectTimer = millis();

  fill_solid(leds, sectionCount, CRGB::Black);

  CRGB colours[] =
  {
    CRGB::Red,
    CRGB::Green,
    CRGB::Blue,
    CRGB::Yellow,
    CRGB::Cyan,
    CRGB::Magenta,
    CRGB::White
  };

  for (int tail = 0; tail < 3; tail++)
  {
    int p = effectPosition - tail;

    if (p >= 0 && p < sectionCount)
    {
      leds[p] = colours[(effectPosition + tail) % 7];
    }
  }

  showLEDs();

  effectPosition++;

  if (effectPosition >= sectionCount + 3)
    effectPosition = 0;
}

// =========================================================
// RAINBOW EFFECT
// =========================================================

void runRainbow()
{
  if (millis() - effectTimer < 35)
    return;

  effectTimer = millis();

  for (int i = 0; i < sectionCount; i++)
  {
    leds[i] =
      CHSV(
        (uint8_t)(effectPosition + i * 8),
        255,
        255
      );
  }

  showLEDs();

  effectPosition++;
}

// =========================================================
// FIRE EFFECT
// =========================================================

void runFire()
{
  if (millis() - effectTimer < 45)
    return;

  effectTimer = millis();

  for (int i = 0; i < sectionCount; i++)
  {
    byte heat = random8(120, 255);

    leds[i] = CRGB(
      heat,
      heat / 3,
      heat / 15
    );
  }

  showLEDs();
}

// =========================================================
// SPARKLE EFFECT
// =========================================================

void runSparkle()
{
  if (millis() - effectTimer < 70)
    return;

  effectTimer = millis();

  fadeToBlackBy(leds, sectionCount, 40);

  int p = random(sectionCount);

  leds[p] = CRGB::White;

  showLEDs();
}

// =========================================================
// EFFECT ENGINE
// =========================================================

void runEffects()
{
  if (!effectRunning)
    return;

  if (currentMode == "CHASE")
    runChase();

  else if (currentMode == "RAINBOW")
    runRainbow();

  else if (currentMode == "FIRE")
    runFire();

  else if (currentMode == "SPARKLE")
    runSparkle();
}

// =========================================================
// AUTO TEST
// =========================================================

void startAuto()
{
  effectRunning = true;
  currentMode = "AUTO";
  diagnosticRunning = true;

  diagnosticState = DIAG_RED;
  diagnosticStep = 0;
  diagnosticTimer = millis();

  diagnosticMessage = "AUTO TEST";
  diagnosticResult = "TESTING";
}

// =========================================================
// VOLTAGE
// =========================================================

float readVoltage()
{
  long totalMilliVolts = 0;

  const int samples = 20;

  for (int i = 0; i < samples; i++)
  {
    totalMilliVolts += analogReadMilliVolts(VOLTAGE_PIN);

    delayMicroseconds(300);
  }

  float sensorVoltage =
    (totalMilliVolts / (float)samples) / 1000.0;

  float supplyVoltage =
    sensorVoltage * voltageDividerRatio;

  supplyVoltage *= voltageCalibration;

  if (supplyVoltage < 0.05)
    supplyVoltage = 0.0;

  return supplyVoltage;
}

// =========================================================
// CURRENT
// =========================================================

float readCurrent()
{
  if (!currentSensorEnabled)
    return 0.0;

  long totalMilliVolts = 0;

  const int samples = 20;

  for (int i = 0; i < samples; i++)
  {
    totalMilliVolts += analogReadMilliVolts(CURRENT_PIN);

    delayMicroseconds(300);
  }

  float sensorVoltage =
    (totalMilliVolts / (float)samples) / 1000.0;

  float current =
    (sensorVoltage - currentZero) / 0.066;

  current *= currentCalibration;

  if (current < 0)
    current = 0;

  return current;
}

// =========================================================
// POWER
// =========================================================

float readPower()
{
  float voltage = readVoltage();
  float current = readCurrent();

  return voltage * current;
}

// =========================================================
// DIAGNOSTIC FULL TEST
// =========================================================

void startDiagnostic()
{
  effectRunning = false;

  diagnosticRunning = true;

  diagnosticState = DIAG_RED;

  diagnosticStep = 0;

  diagnosticCurrentSection = 0;

  diagnosticTimer = millis();

  diagnosticMessage = "Checking RED";
  diagnosticResult = "TESTING";

  currentMode = "DIAGNOSTIC";
}

// =========================================================
// DIAGNOSTIC ENGINE
// =========================================================

void runDiagnostic()
{
  if (!diagnosticRunning)
    return;

  unsigned long now = millis();

  // -------------------------------------------------------
  // RED
  // -------------------------------------------------------

  if (diagnosticState == DIAG_RED)
  {
    if (diagnosticStep == 0)
    {
      showRed();

      diagnosticMessage = "RED COLOUR TEST";
      diagnosticStep = 1;
      diagnosticTimer = now;
      return;
    }

    if (now - diagnosticTimer > 1800)
    {
      diagnosticState = DIAG_GREEN;
      diagnosticStep = 0;
    }

    return;
  }

  // -------------------------------------------------------
  // GREEN
  // -------------------------------------------------------

  if (diagnosticState == DIAG_GREEN)
  {
    if (diagnosticStep == 0)
    {
      showGreen();

      diagnosticMessage = "GREEN COLOUR TEST";
      diagnosticStep = 1;
      diagnosticTimer = now;
      return;
    }

    if (now - diagnosticTimer > 1800)
    {
      diagnosticState = DIAG_BLUE;
      diagnosticStep = 0;
    }

    return;
  }

  // -------------------------------------------------------
  // BLUE
  // -------------------------------------------------------

  if (diagnosticState == DIAG_BLUE)
  {
    if (diagnosticStep == 0)
    {
      showBlue();

      diagnosticMessage = "BLUE COLOUR TEST";
      diagnosticStep = 1;
      diagnosticTimer = now;
      return;
    }

    if (now - diagnosticTimer > 1800)
    {
      diagnosticState = DIAG_WHITE;
      diagnosticStep = 0;
    }

    return;
  }

  // -------------------------------------------------------
  // WHITE
  // -------------------------------------------------------

  if (diagnosticState == DIAG_WHITE)
  {
    if (diagnosticStep == 0)
    {
      showWhite();

      diagnosticMessage = "WHITE COLOUR TEST";
      diagnosticStep = 1;
      diagnosticTimer = now;
      return;
    }

    if (now - diagnosticTimer > 1800)
    {
      diagnosticState = DIAG_SCAN_FORWARD;
      diagnosticStep = 0;
      diagnosticCurrentSection = sectionStart;
    }

    return;
  }

  // -------------------------------------------------------
  // FORWARD SCAN
  // -------------------------------------------------------

  if (diagnosticState == DIAG_SCAN_FORWARD)
  {
    if (diagnosticCurrentSection > sectionEnd)
    {
      diagnosticState = DIAG_SCAN_REVERSE;
      diagnosticCurrentSection = sectionEnd;
      diagnosticStep = 0;
      return;
    }

    if (diagnosticStep == 0)
    {
      clearLEDs();

      setSectionColour(
        diagnosticCurrentSection,
        CRGB::White
      );

      showLEDs();

      diagnosticMessage =
        "CHECK SECTION " +
        String(diagnosticCurrentSection);

      diagnosticStep = 1;
      diagnosticTimer = now;

      return;
    }

    if (now - diagnosticTimer > 700)
    {
      diagnosticCurrentSection++;
      diagnosticStep = 0;
    }

    return;
  }

  // -------------------------------------------------------
  // REVERSE SCAN
  // -------------------------------------------------------

  if (diagnosticState == DIAG_SCAN_REVERSE)
  {
    if (diagnosticCurrentSection < sectionStart)
    {
      diagnosticState = DIAG_COMPLETE;
      diagnosticStep = 0;
      return;
    }

    if (diagnosticStep == 0)
    {
      clearLEDs();

      setSectionColour(
        diagnosticCurrentSection,
        CRGB::Cyan
      );

      showLEDs();

      diagnosticMessage =
        "REVERSE CHECK " +
        String(diagnosticCurrentSection);

      diagnosticStep = 1;
      diagnosticTimer = now;

      return;
    }

    if (now - diagnosticTimer > 700)
    {
      diagnosticCurrentSection--;
      diagnosticStep = 0;
    }

    return;
  }

  // -------------------------------------------------------
  // COMPLETE
  // -------------------------------------------------------

  if (diagnosticState == DIAG_COMPLETE)
  {
    clearLEDs();

    diagnosticRunning = false;

    diagnosticMessage =
      "DIAGNOSTIC TEST COMPLETE";

    diagnosticResult =
      "VISUAL INSPECTION REQUIRED";

    currentMode = "DIAGNOSTIC COMPLETE";

    diagnosticState = DIAG_IDLE;
  }
}

// =========================================================
// HOME PAGE
// =========================================================

String homePage()
{
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">

<title>Moorthy Smart Sign Tester</title>

<style>

*{
  box-sizing:border-box;
}

body{
  margin:0;
  background:#07090d;
  color:#f4f7fb;
  font-family:Arial,Helvetica,sans-serif;
}

.container{
  max-width:900px;
  margin:auto;
  padding:22px;
}

.logo{
  text-align:center;
  padding:25px 10px 20px;
}

.logo h1{
  margin:0;
  font-size:28px;
  letter-spacing:2px;
}

.logo p{
  color:#8d98a8;
  margin-top:8px;
}

.card{
  background:linear-gradient(145deg,#121720,#0d1118);
  border:1px solid #252e3b;
  border-radius:22px;
  padding:25px;
  margin:18px 0;
  box-shadow:0 12px 40px rgba(0,0,0,.35);
}

.card h2{
  margin-top:0;
}

.btn{
  width:100%;
  border:0;
  border-radius:15px;
  padding:18px;
  margin-top:12px;
  font-size:17px;
  font-weight:bold;
  cursor:pointer;
  color:white;
  background:#1e293b;
}

.diagnostic{
  background:linear-gradient(135deg,#075985,#0e7490);
}

.manual{
  background:linear-gradient(135deg,#312e81,#6d28d9);
}

.small{
  color:#9ca9b9;
  font-size:14px;
  line-height:1.5;
}

</style>
</head>

<body>

<div class="container">

<div class="logo">
<h1>MOORTHY SMART SIGN TESTER</h1>
<p>ULTIMATE • V2.2.0</p>
</div>

<div class="card">

<h2>🔧 Ultimate Diagnostics</h2>

<p class="small">
Automatic sign testing, section scanning,
fault investigation and service reporting.
</p>

<button class="btn diagnostic"
onclick="location.href='/v22'">
OPEN V2.2 DIAGNOSTICS
</button>

</div>

<div class="card">

<h2>🎛 Manual LED Tester</h2>

<p class="small">
Your familiar V2.1 manual testing controls,
colour tests, effects and section diagnostics.
</p>

<button class="btn manual"
onclick="location.href='/v21'">
OPEN V2.1 MANUAL TESTER
</button>

</div>

</div>

</body>
</html>
)rawliteral";

  return html;
}

// =========================================================
// V2.2 PAGE
// =========================================================

String v22Page()
{
  String html = R"rawliteral(
<!DOCTYPE html>
<html>

<head>

<meta name="viewport"
content="width=device-width, initial-scale=1">

<title>Moorthy Diagnostics</title>

<style>

*{
 box-sizing:border-box;
}

body{
 margin:0;
 background:
 radial-gradient(circle at top right,#082f49 0,#070b11 35%,#05070a 100%);
 color:#f5f7fa;
 font-family:Arial,Helvetica,sans-serif;
}

.container{
 max-width:950px;
 margin:auto;
 padding:16px;
}

.header{
 display:flex;
 justify-content:space-between;
 align-items:center;
 padding:12px 4px 20px;
}

.brand{
 font-size:21px;
 font-weight:bold;
 letter-spacing:1px;
}

.version{
 font-size:12px;
 color:#7dd3fc;
 margin-top:5px;
}

.status{
 display:flex;
 align-items:center;
 gap:8px;
 font-size:12px;
 color:#a7f3d0;
}

.dot{
 width:9px;
 height:9px;
 background:#22c55e;
 border-radius:50%;
 box-shadow:0 0 12px #22c55e;
}

.card{
 background:rgba(15,23,32,.88);
 border:1px solid rgba(148,163,184,.16);
 border-radius:22px;
 padding:19px;
 margin-bottom:15px;
 box-shadow:0 14px 40px rgba(0,0,0,.25);
 backdrop-filter:blur(12px);
}

.cardTitle{
 color:#8fa1b5;
 font-size:12px;
 text-transform:uppercase;
 letter-spacing:1.5px;
 margin-bottom:10px;
}

.profileName{
 font-size:23px;
 font-weight:bold;
}

.profileSub{
 color:#94a3b8;
 margin-top:5px;
}

.grid{
 display:grid;
 grid-template-columns:repeat(3,1fr);
 gap:10px;
}

.metric{
 background:#0b1118;
 border:1px solid #202b38;
 border-radius:16px;
 padding:15px;
}

.metricLabel{
 color:#7f8da0;
 font-size:11px;
 text-transform:uppercase;
}

.metricValue{
 font-size:22px;
 font-weight:bold;
 margin-top:6px;
}

.metricUnit{
 font-size:11px;
 color:#64748b;
}

.mainButton{
 width:100%;
 border:0;
 border-radius:18px;
 padding:21px;
 background:linear-gradient(135deg,#0891b2,#2563eb);
 color:white;
 font-size:18px;
 font-weight:bold;
 box-shadow:0 10px 30px rgba(8,145,178,.25);
 cursor:pointer;
}

.mainButton:active{
 transform:scale(.98);
}

.secondaryGrid{
 display:grid;
 grid-template-columns:repeat(2,1fr);
 gap:10px;
}

.btn{
 border:1px solid #263445;
 background:#101720;
 color:#e5edf6;
 border-radius:14px;
 padding:15px;
 font-weight:bold;
 cursor:pointer;
}

.btn:active{
 transform:scale(.97);
}

.sectionMap{
 display:grid;
 grid-template-columns:repeat(7,1fr);
 gap:7px;
}

.section{
 background:#101923;
 border:1px solid #273545;
 border-radius:12px;
 padding:12px 4px;
 text-align:center;
 font-size:12px;
 color:#94a3b8;
}

.section.active{
 background:#075985;
 border-color:#38bdf8;
 color:white;
 box-shadow:0 0 18px rgba(56,189,248,.25);
}

.section.pass{
 border-color:#16a34a;
 color:#86efac;
}

.diagnosticBox{
 border-radius:17px;
 padding:17px;
 background:#0b1118;
 border:1px solid #263445;
}

.diagnosticStatus{
 font-size:20px;
 font-weight:bold;
}

.diagnosticText{
 color:#94a3b8;
 margin-top:7px;
 line-height:1.45;
}

.report{
 white-space:pre-wrap;
 background:#05080c;
 border:1px solid #202b38;
 border-radius:14px;
 padding:15px;
 font-family:monospace;
 font-size:12px;
 color:#cbd5e1;
}

.nav{
 display:flex;
 gap:8px;
 margin-bottom:14px;
}

.nav button{
 flex:1;
}

@media(max-width:600px){

 .grid{
   grid-template-columns:1fr 1fr 1fr;
 }

 .metricValue{
   font-size:18px;
 }

 .sectionMap{
   grid-template-columns:repeat(4,1fr);
 }

 .profileName{
   font-size:19px;
 }

}

</style>

</head>

<body>

<div class="container">

<div class="header">

<div>
<div class="brand">MOORTHY SMART SIGN TESTER</div>
<div class="version">ULTIMATE DIAGNOSTICS • V2.2.0</div>
</div>

<div class="status">
<div class="dot"></div>
READY
</div>

</div>

<div class="nav">

<button class="btn"
onclick="location.href='/'">
HOME
</button>

<button class="btn"
onclick="location.href='/v21'">
V2.1 MANUAL
</button>

</div>

<div class="card">

<div class="cardTitle">
Connected Sign
</div>

<div class="profileName"
id="profileName">
Loading...
</div>

<div class="profileSub"
id="profileDetails">
Loading profile...
</div>

</div>

<div class="card">

<div class="cardTitle">
Live Power
</div>

<div class="grid">

<div class="metric">
<div class="metricLabel">Voltage</div>
<div class="metricValue" id="voltage">0.00</div>
<div class="metricUnit">V</div>
</div>

<div class="metric">
<div class="metricLabel">Current</div>
<div class="metricValue" id="current">0.00</div>
<div class="metricUnit">A</div>
</div>

<div class="metric">
<div class="metricLabel">Power</div>
<div class="metricValue" id="power">0.00</div>
<div class="metricUnit">W</div>
</div>

</div>

</div>

<div class="card">

<div class="cardTitle">
Sign Section Map
</div>

<div class="sectionMap"
id="sectionMap">
</div>

</div>

<div class="card">

<div class="cardTitle">
Primary Diagnostic
</div>

<button class="mainButton"
onclick="startDiagnostic()">
▶ FULL SIGN DIAGNOSTIC
</button>

</div>

<div class="card">

<div class="cardTitle">
Quick Diagnostics
</div>

<div class="secondaryGrid">

<button class="btn"
onclick="testPower()">
⚡ POWER
</button>

<button class="btn"
onclick="testData()">
〽 DATA
</button>

<button class="btn"
onclick="scanForward()">
→ SECTION SCAN
</button>

<button class="btn"
onclick="scanReverse()">
← REVERSE SCAN
</button>

</div>

</div>

<div class="card">

<div class="cardTitle">
Diagnostic Result
</div>

<div class="diagnosticBox">

<div class="diagnosticStatus"
id="diagResult">
READY
</div>

<div class="diagnosticText"
id="diagMessage">
Connect the sign and start a diagnostic test.
</div>

</div>

</div>

<div class="card">

<div class="cardTitle">
Troubleshooting
</div>

<div class="secondaryGrid">

<button class="btn"
onclick="location.href='/troubleshoot'">
🛠 TROUBLESHOOT
</button>

<button class="btn"
onclick="generateReport()">
📋 REPORT
</button>

</div>

</div>

</div>

<script>

let sectionCount = 7;

function updateStatus(){

 fetch('/status')
 .then(r => r.json())
 .then(d => {

   sectionCount = d.sections;

   document.getElementById('voltage').innerText =
     Number(d.voltage).toFixed(2);

   document.getElementById('current').innerText =
     Number(d.current).toFixed(2);

   document.getElementById('power').innerText =
     Number(d.power).toFixed(2);

   document.getElementById('profileName').innerText =
     d.type;

   document.getElementById('profileDetails').innerText =
     d.protocol + ' • ' +
     d.order + ' • ' +
     d.profileVoltage + ' • ' +
     d.sections + ' Sections • ' +
     d.ledsPerSection + ' LEDs/Section';

   drawSections(d);

 });

}

function drawSections(d){

 let box =
   document.getElementById('sectionMap');

 box.innerHTML = '';

 for(let i=1;i<=d.sections;i++){

   let div =
     document.createElement('div');

   div.className = 'section';

   if(i >= d.scanStart &&
      i <= d.scanEnd){

     div.classList.add('pass');
   }

   div.innerText =
     String(i).padStart(2,'0');

   box.appendChild(div);
 }

}

function startDiagnostic(){

 document.getElementById('diagResult').innerText =
   'TESTING';

 document.getElementById('diagMessage').innerText =
   'Running colour and section diagnostic...';

 fetch('/diagnostic/start');

}

function testPower(){

 document.getElementById('diagResult').innerText =
   'POWER CHECK';

 document.getElementById('diagMessage').innerText =
   'Check the live voltage reading and power supply connections.';

}

function testData(){

 document.getElementById('diagResult').innerText =
   'DATA TEST';

 document.getElementById('diagMessage').innerText =
   'Running data-chain test. Watch for the first section that fails to respond.';

 fetch('/command?cmd=white');

}

function scanForward(){

 fetch('/scan?reverse=0&start=1&end=' + sectionCount);

 document.getElementById('diagResult').innerText =
   'FORWARD SCAN';

 document.getElementById('diagMessage').innerText =
   'Sections are being tested from first to last.';

}

function scanReverse(){

 fetch('/scan?reverse=1&start=1&end=' + sectionCount);

 document.getElementById('diagResult').innerText =
   'REVERSE SCAN';

 document.getElementById('diagMessage').innerText =
   'Sections are being tested from last to first.';

}

function generateReport(){

 fetch('/report')
 .then(r => r.text())
 .then(t => {

   document.getElementById('diagResult').innerText =
     'REPORT READY';

   document.getElementById('diagMessage').innerText =
     t;

 });

}

setInterval(updateStatus,1000);

updateStatus();

</script>

</body>
</html>
)rawliteral";

  return html;
}

// =========================================================
// TROUBLESHOOT PAGE
// =========================================================

String troubleshootPage()
{
  String html = R"rawliteral(
<!DOCTYPE html>
<html>

<head>

<meta name="viewport"
content="width=device-width, initial-scale=1">

<title>Troubleshooting</title>

<style>

body{
 margin:0;
 background:#070a0f;
 color:#f4f7fb;
 font-family:Arial,sans-serif;
}

.container{
 max-width:800px;
 margin:auto;
 padding:20px;
}

.card{
 background:#101720;
 border:1px solid #253141;
 border-radius:20px;
 padding:20px;
 margin-bottom:14px;
}

h1{
 font-size:25px;
}

h3{
 color:#38bdf8;
}

p{
 color:#a5b1c0;
 line-height:1.6;
}

button{
 width:100%;
 padding:16px;
 border:0;
 border-radius:14px;
 background:#1e293b;
 color:white;
 font-weight:bold;
 margin-top:10px;
}

</style>

</head>

<body>

<div class="container">

<h1>🛠 Troubleshooting Guide</h1>

<div class="card">

<h3>⚡ POWER FAULT</h3>

<p>
If the sign is dim, unstable or completely dead,
check the supply voltage at the sign input.
</p>

<p>
Possible causes:
<br>• SMPS problem
<br>• Loose +12V connection
<br>• Ground connection problem
<br>• Excessive voltage drop
<br>• Excessive load
</p>

</div>

<div class="card">

<h3>〽 DATA CHAIN FAULT</h3>

<p>
If sections before a particular point work while
later sections stop responding, inspect the data
connection around the first failed section.
</p>

<p>
Check:
<br>• DATA IN
<br>• DATA OUT
<br>• Direction arrow
<br>• Broken wire
<br>• Damaged controller/module
</p>

</div>

<div class="card">

<h3>💡 SINGLE SECTION FAULT</h3>

<p>
If only one module or section does not respond while
the surrounding sections work, inspect that module,
its power connection and its data connection.
</p>

</div>

<div class="card">

<h3>🎨 WRONG COLOUR</h3>

<p>
If red produces green, green produces blue, etc.,
check the Colour Order profile.
</p>

<p>
Available orders:
<br>RGB
<br>RBG
<br>GRB
<br>GBR
<br>BRG
<br>BGR
</p>

</div>

<button
onclick="location.href='/v22'">
← BACK TO V2.2
</button>

</div>

</body>

</html>
)rawliteral";

  return html;
}

// =========================================================
// V2.1 MANUAL PAGE
// =========================================================

String v21Page()
{
  String html = R"rawliteral(
<!DOCTYPE html>
<html>

<head>

<meta name="viewport"
content="width=device-width, initial-scale=1">

<title>Moorthy V2.1 Manual Tester</title>

<style>

*{
 box-sizing:border-box;
}

body{
 margin:0;
 background:#080a0e;
 color:white;
 font-family:Arial,sans-serif;
}

.container{
 max-width:850px;
 margin:auto;
 padding:16px;
}

h1{
 text-align:center;
 font-size:24px;
}

.subtitle{
 text-align:center;
 color:#8995a5;
 margin-bottom:18px;
}

.card{
 background:#121820;
 border:1px solid #263241;
 border-radius:18px;
 padding:17px;
 margin-bottom:14px;
}

.title{
 color:#94a3b8;
 font-size:12px;
 text-transform:uppercase;
 letter-spacing:1px;
 margin-bottom:12px;
}

button,
select,
input{
 width:100%;
 padding:13px;
 border-radius:11px;
 border:1px solid #334155;
 background:#0b1118;
 color:white;
 margin-top:8px;
}

button{
 font-weight:bold;
 cursor:pointer;
}

.grid{
 display:grid;
 grid-template-columns:repeat(2,1fr);
 gap:8px;
}

.colours{
 display:grid;
 grid-template-columns:repeat(3,1fr);
 gap:8px;
}

.status{
 background:#080d13;
 border-radius:12px;
 padding:14px;
 line-height:1.8;
 color:#cbd5e1;
}

</style>

</head>

<body>

<div class="container">

<h1>MOORTHY SMART SIGN TESTER</h1>

<div class="subtitle">
V2.1 MANUAL TESTER
</div>

<div class="card">

<div class="title">LED Profile</div>

<select id="type"
onchange="changeType()">

<option value="0">WS2811</option>
<option value="1">WS2811 3-LED MODULE</option>
<option value="2">ADDRESSABLE PIXEL</option>
<option value="3">CX1903</option>

</select>

<select id="order"
onchange="changeOrder()">

<option value="0">RGB</option>
<option value="1">RBG</option>
<option value="2">GRB</option>
<option value="3">GBR</option>
<option value="4">BRG</option>
<option value="5">BGR</option>

</select>

</div>

<div class="card">

<div class="title">LED Settings</div>

<input id="sections"
type="number"
min="1"
max="500"
placeholder="Sections">

<input id="leds"
type="number"
min="1"
max="20"
placeholder="Physical LEDs per section">

<input id="brightness"
type="number"
min="1"
max="255"
placeholder="Brightness">

<button onclick="saveSettings()">
SAVE SETTINGS
</button>

</div>

<div class="card">

<div class="title">Colour Test</div>

<div class="colours">

<button onclick="cmd('red')">RED</button>
<button onclick="cmd('green')">GREEN</button>
<button onclick="cmd('blue')">BLUE</button>
<button onclick="cmd('white')">WHITE</button>
<button onclick="cmd('yellow')">YELLOW</button>
<button onclick="cmd('gold')">GOLD</button>
<button onclick="cmd('cyan')">CYAN</button>
<button onclick="cmd('magenta')">MAGENTA</button>
<button onclick="cmd('orange')">ORANGE</button>

</div>

<button onclick="cmd('off')">
OFF
</button>

</div>

<div class="card">

<div class="title">Effects</div>

<div class="grid">

<button onclick="cmd('chase')">
CHASE
</button>

<button onclick="cmd('rainbow')">
RAINBOW
</button>

<button onclick="cmd('fire')">
FIRE
</button>

<button onclick="cmd('sparkle')">
SPARKLE
</button>

<button onclick="cmd('auto')">
AUTO TEST
</button>

</div>

</div>

<div class="card">

<div class="title">Section Diagnostics</div>

<input id="start"
type="number"
min="1"
placeholder="Start section">

<input id="end"
type="number"
min="1"
placeholder="End section">

<div class="grid">

<button onclick="scan(0)">
FORWARD SCAN
</button>

<button onclick="scan(1)">
REVERSE SCAN
</button>

</div>

<button onclick="sectionTest()">
SECTION TEST
</button>

</div>

<div class="card">

<div class="title">Live Status</div>

<div class="status"
id="status">
Loading...
</div>

</div>

<button onclick="location.href='/'">
← HOME
</button>

</div>

<script>

function cmd(c){

 fetch('/command?cmd='+c);

}

function changeType(){

 let v =
 document.getElementById('type').value;

 fetch('/type?type='+v)
 .then(()=>update(true));

}

function changeOrder(){

 let v =
 document.getElementById('order').value;

 fetch('/colourorder?order='+v);

}

function saveSettings(){

 let s =
 document.getElementById('sections').value;

 let l =
 document.getElementById('leds').value;

 let b =
 document.getElementById('brightness').value;

 fetch('/settings?sections='+s+
 '&leds='+l+
 '&brightness='+b)
 .then(()=>update(true));

}

function scan(reverse){

 let s =
 document.getElementById('start').value;

 let e =
 document.getElementById('end').value;

 if(!s) s=1;

 if(!e) e=
 document.getElementById('sections').value;

 fetch('/scan?reverse='+reverse+
 '&start='+s+
 '&end='+e);

}

function sectionTest(){

 let s =
 document.getElementById('start').value;

 let e =
 document.getElementById('end').value;

 if(!s) s=1;

 if(!e) e=s;

 fetch('/section?start='+s+'&end='+e);

}

function update(updateInputs){

 fetch('/status')
 .then(r=>r.json())
 .then(d=>{

   document.getElementById('status').innerHTML =
   'LED TYPE: '+d.type+
   '<br>PROTOCOL: '+d.protocol+
   '<br>COLOUR ORDER: '+d.order+
   '<br>VOLTAGE: '+Number(d.voltage).toFixed(2)+' V'+
   '<br>CURRENT: '+Number(d.current).toFixed(2)+' A'+
   '<br>POWER: '+Number(d.power).toFixed(2)+' W'+
   '<br>SECTIONS: '+d.sections+
   '<br>LEDS / SECTION: '+d.ledsPerSection+
   '<br>TOTAL PHYSICAL LEDS: '+d.totalLEDs+
   '<br>BRIGHTNESS: '+d.brightness+
   '<br>MODE: '+d.mode;

   if(updateInputs){

     document.getElementById('type').value =
       d.typeIndex;

     document.getElementById('order').value =
       d.orderIndex;

     document.getElementById('sections').value =
       d.sections;

     document.getElementById('leds').value =
       d.ledsPerSection;

     document.getElementById('brightness').value =
       d.brightness;

     document.getElementById('start').value =
       d.scanStart;

     document.getElementById('end').value =
       d.scanEnd;

   }

 });

}

update(true);

setInterval(function(){
 update(false);
},1500);

</script>

</body>

</html>
)rawliteral";

  return html;
}

// =========================================================
// COMMAND HANDLER
// =========================================================

void handleCommand()
{
  String cmd = server.arg("cmd");

  if (cmd == "red")
    showRed();

  else if (cmd == "green")
    showGreen();

  else if (cmd == "blue")
    showBlue();

  else if (cmd == "white")
    showWhite();

  else if (cmd == "yellow")
    showYellow();

  else if (cmd == "gold")
    showGold();

  else if (cmd == "cyan")
    showCyan();

  else if (cmd == "magenta")
    showMagenta();

  else if (cmd == "orange")
    showOrange();

  else if (cmd == "off")
    showOff();

  else if (cmd == "chase")
    startChase();

  else if (cmd == "rainbow")
    startRainbow();

  else if (cmd == "fire")
    startFire();

  else if (cmd == "sparkle")
    startSparkle();

  else if (cmd == "auto")
    startAuto();

  server.send(200, "text/plain", "OK");
}

// =========================================================
// TYPE HANDLER
// =========================================================

void handleType()
{
  int type =
    server.arg("type").toInt();

  if (type < 0 || type > 3)
  {
    server.send(400, "text/plain", "Invalid type");
    return;
  }

  currentLEDType =
    (LED_TYPE_SELECT)type;

  applyProfileSettings();
  selectController();

  clearLEDs();

  saveProfile();

  server.send(200, "text/plain", "OK");
}

// =========================================================
// COLOUR ORDER HANDLER
// =========================================================

void handleColourOrder()
{
  int order =
    server.arg("order").toInt();

  if (order < 0 || order > 5)
  {
    server.send(400, "text/plain", "Invalid colour order");
    return;
  }

  COLOUR_ORDER_SELECT selected =
    (COLOUR_ORDER_SELECT)order;

  switch (currentLEDType)
  {
    case TYPE_WS2811:
      wsColourOrder = selected;
      break;

    case TYPE_WS2811_MODULE:
      moduleColourOrder = selected;
      break;

    case TYPE_ADDRESSABLE_PIXEL:
      pixelColourOrder = selected;
      break;

    case TYPE_CX1903:
      cxColourOrder = selected;
      break;
  }

  selectController();

  clearLEDs();

  saveProfile();

  server.send(200, "text/plain", "OK");
}

// =========================================================
// SETTINGS HANDLER
// =========================================================

void handleSettings()
{
  int sections =
    server.arg("sections").toInt();

  int ledsPerSection =
    server.arg("leds").toInt();

  int newBrightness =
    server.arg("brightness").toInt();

  if (sections < 1)
    sections = 1;

  if (sections > MAX_SECTIONS)
    sections = MAX_SECTIONS;

  if (ledsPerSection < 1)
    ledsPerSection = 1;

  if (ledsPerSection > 20)
    ledsPerSection = 20;

  if (newBrightness < 1)
    newBrightness = 1;

  if (newBrightness > 255)
    newBrightness = 255;

  brightness = newBrightness;

  switch (currentLEDType)
  {
    case TYPE_WS2811:
      wsSections = sections;
      wsLedsPerSection = ledsPerSection;
      break;

    case TYPE_WS2811_MODULE:
      moduleSections = sections;
      moduleLedsPerSection = ledsPerSection;
      break;

    case TYPE_ADDRESSABLE_PIXEL:
      pixelSections = sections;
      pixelLedsPerSection = ledsPerSection;
      break;

    case TYPE_CX1903:
      cxSections = sections;
      cxLedsPerSection = ledsPerSection;
      break;
  }

  applyProfileSettings();

  if (sectionEnd > sectionCount)
    sectionEnd = sectionCount;

  saveProfile();

  clearLEDs();

  server.send(200, "text/plain", "OK");
}

// =========================================================
// SCAN
// =========================================================

void handleScan()
{
  int start =
    server.arg("start").toInt();

  int end =
    server.arg("end").toInt();

  bool reverse =
    server.arg("reverse").toInt() == 1;

  if (start < 1)
    start = 1;

  if (end > sectionCount)
    end = sectionCount;

  if (start > sectionCount)
    start = sectionCount;

  if (end < 1)
    end = 1;

  if (start > end)
  {
    int temp = start;
    start = end;
    end = temp;
  }

  sectionStart = start;
  sectionEnd = end;

  effectRunning = false;

  clearLEDs();

  if (!reverse)
  {
    for (int i = start; i <= end; i++)
    {
      clearLEDs();

      leds[i - 1] = CRGB::White;

      showLEDs();

      delay(450);
    }
  }
  else
  {
    for (int i = end; i >= start; i--)
    {
      clearLEDs();

      leds[i - 1] = CRGB::Cyan;

      showLEDs();

      delay(450);
    }
  }

  clearLEDs();

  currentMode =
    reverse ? "REVERSE SCAN" : "FORWARD SCAN";

  server.send(200, "text/plain", "OK");
}

// =========================================================
// SECTION TEST
// =========================================================

void handleSection()
{
  int start =
    server.arg("start").toInt();

  int end =
    server.arg("end").toInt();

  if (start < 1)
    start = 1;

  if (end > sectionCount)
    end = sectionCount;

  if (start > end)
  {
    int temp = start;
    start = end;
    end = temp;
  }

  effectRunning = false;

  clearLEDs();

  for (int i = start; i <= end; i++)
  {
    leds[i - 1] = CRGB::Green;
  }

  showLEDs();

  currentMode =
    "SECTION TEST";

  server.send(200, "text/plain", "OK");
}

// =========================================================
// DIAGNOSTIC START
// =========================================================

void handleDiagnosticStart()
{
  startDiagnostic();

  server.send(200, "text/plain", "OK");
}

// =========================================================
// REPORT
// =========================================================

void handleReport()
{
  float voltage = readVoltage();
  float current = readCurrent();
  float power = voltage * current;

  String report = "";

  report += "MOORTHY AARTS - SIGN TEST REPORT\n";
  report += "================================\n\n";

  report += "LED TYPE: ";
  report += getLEDTypeName();
  report += "\n";

  report += "PROTOCOL: ";
  report += getProtocolName();
  report += "\n";

  report += "COLOUR ORDER: ";
  report += getCurrentColourOrderName();
  report += "\n";

  report += "VOLTAGE PROFILE: 12V\n";

  report += "SECTIONS: ";
  report += String(sectionCount);
  report += "\n";

  report += "PHYSICAL LEDS / SECTION: ";
  report += String(physicalLEDsPerSection);
  report += "\n";

  report += "TOTAL PHYSICAL LEDS: ";
  report += String(sectionCount * physicalLEDsPerSection);
  report += "\n\n";

  report += "POWER\n";
  report += "------\n";

  report += "Voltage: ";
  report += String(voltage, 2);
  report += " V\n";

  report += "Current: ";
  report += String(current, 2);
  report += " A\n";

  report += "Power: ";
  report += String(power, 2);
  report += " W\n\n";

  report += "DIAGNOSTIC\n";
  report += "----------\n";

  report += diagnosticMessage;
  report += "\n";

  report += "RESULT: ";
  report += diagnosticResult;
  report += "\n\n";

  report += "NOTE\n";
  report += "----\n";
  report += "Visual inspection is required to confirm physical LED/module damage.\n";

  server.send(
    200,
    "text/plain",
    report
  );
}

// =========================================================
// STATUS JSON
// =========================================================

void handleStatus()
{
  float voltage =
    readVoltage();

  float current =
    readCurrent();

  float power =
    voltage * current;

  String json = "{";

  json += "\"type\":\"";
  json += getLEDTypeName();
  json += "\",";

  json += "\"typeIndex\":";
  json += String((int)currentLEDType);
  json += ",";

  json += "\"protocol\":\"";
  json += getProtocolName();
  json += "\",";

  json += "\"order\":\"";
  json += getCurrentColourOrderName();
  json += "\",";

  json += "\"orderIndex\":";
  json += String((int)getCurrentColourOrder());
  json += ",";

  json += "\"profileVoltage\":\"12V\",";

  json += "\"sections\":";
  json += String(sectionCount);
  json += ",";

  json += "\"ledsPerSection\":";
  json += String(physicalLEDsPerSection);
  json += ",";

  json += "\"totalLEDs\":";
  json += String(
    sectionCount * physicalLEDsPerSection
  );
  json += ",";

  json += "\"brightness\":";
  json += String(brightness);
  json += ",";

  json += "\"scanStart\":";
  json += String(sectionStart);
  json += ",";

  json += "\"scanEnd\":";
  json += String(sectionEnd);
  json += ",";

  json += "\"voltage\":";
  json += String(voltage, 2);
  json += ",";

  json += "\"current\":";
  json += String(current, 2);
  json += ",";

  json += "\"power\":";
  json += String(power, 2);
  json += ",";

  json += "\"mode\":\"";
  json += currentMode;
  json += "\",";

  json += "\"currentSensor\":";
  json += currentSensorEnabled ? "true" : "false";
  json += ",";

  json += "\"diagnosticRunning\":";
  json += diagnosticRunning ? "true" : "false";
  json += ",";

  json += "\"diagnosticMessage\":\"";
  json += diagnosticMessage;
  json += "\",";

  json += "\"diagnosticResult\":\"";
  json += diagnosticResult;
  json += "\"";

  json += "}";

  server.send(
    200,
    "application/json",
    json
  );
}

// =========================================================
// ROOT
// =========================================================

void handleRoot()
{
  server.send(
    200,
    "text/html",
    homePage()
  );
}

// =========================================================
// SETUP
// =========================================================

void setup()
{
  Serial.begin(115200);

  delay(500);

  // -------------------------------------------------------
  // VOLTAGE INPUT
  // -------------------------------------------------------

  pinMode(VOLTAGE_PIN, INPUT);

  analogReadResolution(12);

  analogSetPinAttenuation(
    VOLTAGE_PIN,
    ADC_11db
  );

  // -------------------------------------------------------
  // CURRENT INPUT
  // -------------------------------------------------------

  pinMode(CURRENT_PIN, INPUT);

  analogSetPinAttenuation(
    CURRENT_PIN,
    ADC_11db
  );

  // -------------------------------------------------------
  // LOAD SAVED PROFILE
  // -------------------------------------------------------

  loadProfile();

  // -------------------------------------------------------
  // FASTLED CONTROLLERS
  // -------------------------------------------------------

  ctrlRGB =
    &FastLED.addLeds<
      WS2811,
      LED_PIN,
      RGB
    >(leds, MAX_SECTIONS);

  ctrlRBG =
    &FastLED.addLeds<
      WS2811,
      LED_PIN,
      RBG
    >(leds, MAX_SECTIONS);

  ctrlGRB =
    &FastLED.addLeds<
      WS2811,
      LED_PIN,
      GRB
    >(leds, MAX_SECTIONS);

  ctrlGBR =
    &FastLED.addLeds<
      WS2811,
      LED_PIN,
      GBR
    >(leds, MAX_SECTIONS);

  ctrlBRG =
    &FastLED.addLeds<
      WS2811,
      LED_PIN,
      BRG
    >(leds, MAX_SECTIONS);

  ctrlBGR =
    &FastLED.addLeds<
      WS2811,
      LED_PIN,
      BGR
    >(leds, MAX_SECTIONS);

  cxController =
    &FastLED.addLeds<
      UCS1903B,
      LED_PIN,
      BRG
    >(leds, MAX_SECTIONS);

  // -------------------------------------------------------
  // PROFILE
  // -------------------------------------------------------

  applyProfileSettings();

  selectController();

  clearLEDs();

  // -------------------------------------------------------
  // WIFI AP
  // -------------------------------------------------------

  WiFi.mode(WIFI_AP);

  WiFi.softAP(
    AP_SSID,
    AP_PASSWORD
  );

  Serial.println();
  Serial.println(
    "========================================"
  );

  Serial.println(
    "MOORTHY SMART SIGN TESTER V2.2"
  );

  Serial.println(
    "========================================"
  );

  Serial.print(
    "AP IP: "
  );

  Serial.println(
    WiFi.softAPIP()
  );

  // -------------------------------------------------------
  // WEB ROUTES
  // -------------------------------------------------------

  server.on(
    "/",
    handleRoot
  );

  server.on(
    "/v22",
    []()
    {
      server.send(
        200,
        "text/html",
        v22Page()
      );
    }
  );

  server.on(
    "/v21",
    []()
    {
      server.send(
        200,
        "text/html",
        v21Page()
      );
    }
  );

  server.on(
    "/troubleshoot",
    []()
    {
      server.send(
        200,
        "text/html",
        troubleshootPage()
      );
    }
  );

  server.on(
    "/command",
    handleCommand
  );

  server.on(
    "/type",
    handleType
  );

  server.on(
    "/colourorder",
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

  server.on(
    "/diagnostic/start",
    handleDiagnosticStart
  );

  server.on(
    "/report",
    handleReport
  );

  server.begin();

  Serial.println(
    "Web server started."
  );

  Serial.println(
    "Open: http://192.168.4.1"
  );
}

// =========================================================
// LOOP
// =========================================================

void loop()
{
  server.handleClient();

  runEffects();

  runDiagnostic();
}