#include <DisplayManager.h>
#include "DisplayManager_Internal.h"
#include "Globals.h"
#include "Functions.h"
#include "MQTTManager.h"
#include "PeripheryManager.h"
#include "effects.h"
#include "Apps.h"
#include "Games/GameManager.h"
#include "Dictionary.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include "Overlays.h"

String CRGBtoHex(CRGB color)
{
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", color.r, color.g, color.b);
  return String(buf);
}

String getOverlayName()
{
  switch (GLOBAL_OVERLAY)
  {
  case DRIZZLE:
    return "drizzle";
  case RAIN:
    return "rain";
  case SNOW:
    return "snow";
  case STORM:
    return "storm";
  case THUNDER:
    return "thunder";
  case FROST:
    return "frost";
  case NONE:
    return "clear";
  default:
    Serial.println(F("Invalid effect."));
    return "invalid"; // Oder einen leeren String oder einen Fehlerwert zurückgeben
  }
}

float logMap(float x, float in_min, float in_max, float out_min, float out_max, float mid_point_out)
{
  if (x < in_min)
    return out_min;
  if (x > in_max)
    return out_max;
  float scale = (mid_point_out - out_min) / log(in_max - in_min + 1);
  if (x <= (in_max + in_min) / 2.0)
  {
    return scale * log(x - in_min + 1) + out_min;
  }
  else
  {
    float upper_scale = (out_max - mid_point_out) / log(in_max - (in_max + in_min) / 2.0 + 1);
    return upper_scale * log(x - (in_max + in_min) / 2.0 + 1) + mid_point_out;
  }
}

String DisplayManager_::getStats()
{
  StaticJsonDocument<1024> doc;
  char buffer[20];

#ifdef awtrix2_upgrade
  doc[F("type")] = 1;
#else
  doc[BatKey] = BATTERY_PERCENT;
  doc[BatRawKey] = BATTERY_RAW;
  doc[F("type")] = 0;
#endif
  doc[LuxKey] = static_cast<int>(CURRENT_LUX);
  doc[LDRRawKey] = LDR_RAW;
  doc[RamKey] = ESP.getFreeHeap() + ESP.getFreePsram();
  doc[BrightnessKey] = BRIGHTNESS;
  if (SENSOR_READING)
  {
    double formattedTemp = roundToDecimalPlaces(CURRENT_TEMP, TEMP_DECIMAL_PLACES);
    doc[TempKey] = formattedTemp;
    doc[HumKey] = static_cast<uint8_t>(CURRENT_HUM);
  }
  doc[UpTimeKey] = PeripheryManager.readUptime();
  doc[SignalStrengthKey] = WiFi.RSSI();
  doc[MessagesKey] = RECEIVED_MESSAGES;
  doc[VersionKey] = VERSION;
  doc[F("indicator1")] = ui->indicator1State;
  doc[F("indicator2")] = ui->indicator2State;
  doc[F("indicator3")] = ui->indicator3State;
  doc[F("app")] = CURRENT_APP;
  doc[F("uid")] = uniqueID;
  doc[F("matrix")] = !MATRIX_OFF;
  doc[IpAddrKey] = WiFi.localIP();
  String jsonString;
  serializeJson(doc, jsonString);
  return jsonString;
}

void DisplayManager_::setMatrixLayout(int layout)
{
  if (DEBUG_MODE)
    DEBUG_PRINTF("Set matrix layout to %i", layout);

  FastLED_NeoMatrix *newMatrix = nullptr;
  switch (layout)
  {
  case 0:
    newMatrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);
    break;
  case 1:
    newMatrix = new FastLED_NeoMatrix(leds, 8, 8, 4, 1, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE);
    break;
  case 2:
    newMatrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);
    break;
  default:
    return; // Unknown layout, keep current matrix/ui intact
  }

  MatrixDisplayUi *newUi = new MatrixDisplayUi(newMatrix);

  delete ui;
  delete matrix;
  matrix = newMatrix;
  ui = newUi;
}

// Indicator helper functions to avoid switch-case repetition
static void setIndState(uint8_t ind, bool state)
{
  switch (ind)
  {
  case 1: ui->setIndicator1State(state); break;
  case 2: ui->setIndicator2State(state); break;
  case 3: ui->setIndicator3State(state); break;
  }
}

static void setIndColor(uint8_t ind, uint32_t color)
{
  switch (ind)
  {
  case 1: ui->setIndicator1Color(color); break;
  case 2: ui->setIndicator2Color(color); break;
  case 3: ui->setIndicator3Color(color); break;
  }
}

static void setIndBlink(uint8_t ind, int blink)
{
  switch (ind)
  {
  case 1: ui->setIndicator1Blink(blink); break;
  case 2: ui->setIndicator2Blink(blink); break;
  case 3: ui->setIndicator3Blink(blink); break;
  }
}

static void setIndFade(uint8_t ind, int fade)
{
  switch (ind)
  {
  case 1: ui->setIndicator1Fade(fade); break;
  case 2: ui->setIndicator2Fade(fade); break;
  case 3: ui->setIndicator3Fade(fade); break;
  }
}

static bool getIndState(uint8_t ind)
{
  switch (ind)
  {
  case 1: return ui->indicator1State;
  case 2: return ui->indicator2State;
  case 3: return ui->indicator3State;
  default: return false;
  }
}

static uint32_t getIndColor(uint8_t ind)
{
  switch (ind)
  {
  case 1: return ui->indicator1Color;
  case 2: return ui->indicator2Color;
  case 3: return ui->indicator3Color;
  default: return 0;
  }
}

bool DisplayManager_::indicatorParser(uint8_t indicator, const char *json)
{
  if (strcmp(json, "") == 0 || strcmp(json, "{}") == 0)
  {
    setIndState(indicator, false);
    setIndFade(indicator, 0);
    setIndBlink(indicator, 0);
    MQTTManager.setIndicatorState(indicator, getIndState(indicator), getIndColor(indicator));
    return true;
  }

  DynamicJsonDocument doc(128);
  DeserializationError error = deserializeJson(doc, json);
  if (error)
    return false;

  if (doc.containsKey("color"))
  {
    uint32_t col = getColorFromJsonVariant(doc["color"], TEXTCOLOR_888);
    if (col > 0)
    {
      setIndState(indicator, true);
      setIndColor(indicator, col);
    }
    else
    {
      setIndState(indicator, false);
    }
  }

  setIndBlink(indicator, doc.containsKey("blink") ? doc["blink"].as<int>() : 0);
  setIndFade(indicator, doc.containsKey("fade") ? doc["fade"].as<int>() : 0);

  doc.clear();
  MQTTManager.setIndicatorState(indicator, getIndState(indicator), getIndColor(indicator));
  return true;
}

void DisplayManager_::setIndicator1Color(uint32_t color)
{
  ui->setIndicator1Color(color);
}

void DisplayManager_::setIndicator1State(bool state)
{
  ui->setIndicator1State(state);
}

void DisplayManager_::setIndicator2Color(uint32_t color)
{
  ui->setIndicator2Color(color);
}

void DisplayManager_::setIndicator2State(bool state)
{
  ui->setIndicator2State(state);
}

void DisplayManager_::setIndicator3Color(uint32_t color)
{
  ui->setIndicator3Color(color);
}

void DisplayManager_::setIndicator3State(bool state)
{
  ui->setIndicator3State(state);
}

void DisplayManager_::gammaCorrection()
{
  float gamma = logMap(actualBri, 2, 180, 0.535, 2.3, 1.9);
  memcpy(ledsCopy, leds, sizeof(leds));
  for (int i = 0; i < 256; i++)
  {
    leds[i] = applyGamma_video(leds[i], gamma);
  }

  if (MIRROR_DISPLAY)
  {
    for (int y = 0; y < MATRIX_HEIGHT; y++)
    {
      for (int x = 0; x < MATRIX_WIDTH / 2; x++)
      {
        int index1 = y * MATRIX_WIDTH + x;
        int index2 = y * MATRIX_WIDTH + (MATRIX_WIDTH - x - 1);
        std::swap(leds[index1], leds[index2]);
      }
    }
  }
}

void DisplayManager_::sendAppLoop()
{
  MQTTManager.publish("stats/loop", getAppsAsJson().c_str());
}

String DisplayManager_::getSettings()
{
  StaticJsonDocument<1024> doc;
  doc["MATP"] = !MATRIX_OFF;
  doc["ABRI"] = AUTO_BRIGHTNESS;
  doc["BRI"] = BRIGHTNESS;
  doc["ATRANS"] = AUTO_TRANSITION;
  doc["TCOL"] = TEXTCOLOR_888;
  doc["TEFF"] = TRANS_EFFECT;
  doc["TSPEED"] = TIME_PER_TRANSITION;
  doc["ATIME"] = TIME_PER_APP / 1000;
  doc["TMODE"] = TIME_MODE;
  doc["CHCOL"] = CALENDAR_HEADER_COLOR;
  doc["CTCOL"] = CALENDAR_TEXT_COLOR;
  doc["CBCOL"] = CALENDAR_BODY_COLOR;
  doc["TFORMAT"] = TIME_FORMAT;
  doc["DFORMAT"] = DATE_FORMAT;
  doc["SOM"] = START_ON_MONDAY;
  doc["CEL"] = IS_CELSIUS;
  doc["BLOCKN"] = BLOCK_NAVIGATION;
  doc["MAT"] = MATRIX_LAYOUT;
  doc["SOUND"] = SOUND_ACTIVE;
  doc["GAMMA"] = GAMMA;
  doc["UPPERCASE"] = UPPERCASE_LETTERS;
  doc["CCORRECTION"] = CRGBtoHex(COLOR_CORRECTION);
  doc["CTEMP"] = CRGBtoHex(COLOR_TEMPERATURE);
  doc["WD"] = SHOW_WEEKDAY;
  doc["WDCA"] = WDC_ACTIVE;
  doc["WDCI"] = WDC_INACTIVE;
  doc["TIME_COL"] = TIME_COLOR;
  doc["DATE_COL"] = DATE_COLOR;
  doc["HUM_COL"] = HUM_COLOR;
  doc["TEMP_COL"] = TEMP_COLOR;
  doc["BAT_COL"] = BAT_COLOR;
  doc["SSPEED"] = SCROLL_SPEED;
  doc["TIM"] = SHOW_TIME;
  doc["DAT"] = SHOW_DATE;
  doc["HUM"] = SHOW_HUM;
  doc["TEMP"] = SHOW_TEMP;
  doc["BAT"] = SHOW_BAT;
  doc["VOL"] = SOUND_VOLUME;
  doc["OVERLAY"] = getOverlayName();
  String jsonString;
  return serializeJson(doc, jsonString), jsonString;
}

void DisplayManager_::setNewSettings(const char *json)
{
  if (DEBUG_MODE)
    DEBUG_PRINTLN(F("Got new settings:"));
  if (DEBUG_MODE)
    DEBUG_PRINTLN(json);
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, json);
  if (error)
  {
    if (DEBUG_MODE)
      DEBUG_PRINTLN(F("Error while parsing json"));
    if (DEBUG_MODE)
      DEBUG_PRINTLN(error.c_str());
    return;
  }
  if (doc.containsKey("ATIME"))
  {
    long atime = doc["ATIME"].as<int>();
    TIME_PER_APP = atime * 1000;
  }

  if (doc.containsKey("OVERLAY"))
  {
    GLOBAL_OVERLAY = getOverlay(doc["OVERLAY"].as<String>());
    if (doc.size() == 1)
      return;
  }

  if (doc.containsKey("GAMEMODE"))
  {
    bool gamemode = doc["GAMEMODE"];
    GameManager.start(gamemode);
    return;
  }

  if (doc.containsKey("GAME"))
  {
    int game = doc["GAME"];
    GameManager.ChooseGame(game);
    return;
  }

  TIME_MODE = doc.containsKey("TMODE") ? doc["TMODE"].as<int>() : TIME_MODE;
  TRANS_EFFECT = doc.containsKey("TEFF") ? doc["TEFF"] : TRANS_EFFECT;
  TIME_PER_TRANSITION = doc.containsKey("TSPEED") ? doc["TSPEED"] : TIME_PER_TRANSITION;
  BRIGHTNESS = doc.containsKey("BRI") ? doc["BRI"] : BRIGHTNESS;
  SCROLL_SPEED = doc.containsKey("SSPEED") ? doc["SSPEED"] : SCROLL_SPEED;
  IS_CELSIUS = doc.containsKey("CEL") ? doc["CEL"] : IS_CELSIUS;
  START_ON_MONDAY = doc.containsKey("SOM") ? doc["SOM"].as<bool>() : START_ON_MONDAY;
  MATRIX_OFF = doc.containsKey("MATP") ? !doc["MATP"].as<bool>() : MATRIX_OFF;
  TIME_FORMAT = doc.containsKey("TFORMAT") ? doc["TFORMAT"].as<String>() : TIME_FORMAT;
  GAMMA = doc.containsKey("GAMMA") ? doc["GAMMA"].as<float>() : GAMMA;
  DATE_FORMAT = doc.containsKey("DFORMAT") ? doc["DFORMAT"].as<String>() : DATE_FORMAT;
  AUTO_BRIGHTNESS = doc.containsKey("ABRI") ? doc["ABRI"].as<bool>() : AUTO_BRIGHTNESS;
  AUTO_TRANSITION = doc.containsKey("ATRANS") ? doc["ATRANS"].as<bool>() : AUTO_TRANSITION;
  UPPERCASE_LETTERS = doc.containsKey("UPPERCASE") ? doc["UPPERCASE"].as<bool>() : UPPERCASE_LETTERS;
  SHOW_WEEKDAY = doc.containsKey("WD") ? doc["WD"].as<bool>() : SHOW_WEEKDAY;
  BLOCK_NAVIGATION = doc.containsKey("BLOCKN") ? doc["BLOCKN"].as<bool>() : BLOCK_NAVIGATION;
  SHOW_TIME = doc.containsKey("TIM") ? doc["TIM"].as<bool>() : SHOW_TIME;
  SHOW_DATE = doc.containsKey("DAT") ? doc["DAT"].as<bool>() : SHOW_DATE;
  SHOW_HUM = doc.containsKey("HUM") ? doc["HUM"].as<bool>() : SHOW_HUM;
  SHOW_TEMP = doc.containsKey("TEMP") ? doc["TEMP"].as<bool>() : SHOW_TEMP;
  SHOW_BAT = doc.containsKey("BAT") ? doc["BAT"].as<bool>() : SHOW_BAT;
  SOUND_ACTIVE = doc.containsKey("SOUND") ? doc["SOUND"].as<bool>() : SOUND_ACTIVE;

  if (doc.containsKey("VOL"))
  {
    SOUND_VOLUME = doc["VOL"];
    PeripheryManager.setVolume(SOUND_VOLUME);
  }

  auto parseCRGB = [&](const char *key, CRGB &target)
  {
    if (!doc.containsKey(key))
      return;
    auto colorValue = doc[key];
    if (colorValue.is<String>())
    {
      uint32_t rgbColor = strtoul(colorValue.as<String>().c_str(), NULL, 16);
      target.setRGB((rgbColor >> 16) & 0xFF, (rgbColor >> 8) & 0xFF, rgbColor & 0xFF);
    }
    else if (colorValue.is<JsonArray>() && colorValue.size() == 3)
    {
      target.setRGB(colorValue[0], colorValue[1], colorValue[2]);
    }
  };

  parseCRGB("CCORRECTION", COLOR_CORRECTION);
  if (COLOR_CORRECTION)
  {
    FastLED.setCorrection(COLOR_CORRECTION);
  }

  parseCRGB("CTEMP", COLOR_TEMPERATURE);
  if (COLOR_TEMPERATURE)
  {
    FastLED.setTemperature(COLOR_TEMPERATURE);
  }
  if (doc.containsKey("WDCA"))
  {
    auto WDCA = doc["WDCA"];
    WDC_ACTIVE = getColorFromJsonVariant(WDCA, 0xFFFFFF);
  }
  if (doc.containsKey("CHCOL"))
  {
    auto CHCOL = doc["CHCOL"];
    CALENDAR_HEADER_COLOR = getColorFromJsonVariant(CHCOL, 0xFF0000);
  }
  if (doc.containsKey("CTCOL"))
  {
    auto CTCOL = doc["CTCOL"];
    CALENDAR_TEXT_COLOR = getColorFromJsonVariant(CTCOL, 0x000000);
  }
  if (doc.containsKey("CBCOL"))
  {
    auto CBCOL = doc["CBCOL"];
    CALENDAR_BODY_COLOR = getColorFromJsonVariant(CBCOL, 0xFFFFFF);
  }
  if (doc.containsKey("WDCI"))
  {
    auto WDCI = doc["WDCI"];
    WDC_INACTIVE = getColorFromJsonVariant(WDCI, 0x666666);
  }
  if (doc.containsKey("TCOL"))
  {
    auto TCOL = doc["TCOL"];
    uint32_t TempColor = getColorFromJsonVariant(TCOL, 0xFFFFFF);
    setCustomAppColors(TempColor);
    TEXTCOLOR_888 = TempColor;
  }

  if (doc.containsKey("TIME_COL"))
  {
    auto TIME_COL = doc["TIME_COL"];
    TIME_COLOR = getColorFromJsonVariant(TIME_COL, TEXTCOLOR_888);
  }
  if (doc.containsKey("DATE_COL"))
  {
    auto DATE_COL = doc["DATE_COL"];
    DATE_COLOR = getColorFromJsonVariant(DATE_COL, TEXTCOLOR_888);
  }
  if (doc.containsKey("TEMP_COL"))
  {
    auto TEMP_COL = doc["TEMP_COL"];
    TEMP_COLOR = getColorFromJsonVariant(TEMP_COL, TEXTCOLOR_888);
  }
  if (doc.containsKey("HUM_COL"))
  {
    auto HUM_COL = doc["HUM_COL"];
    HUM_COLOR = getColorFromJsonVariant(HUM_COL, TEXTCOLOR_888);
  }
  if (doc.containsKey("BAT_COL"))
  {
    auto BAT_COL = doc["BAT_COL"];
    BAT_COLOR = getColorFromJsonVariant(BAT_COL, TEXTCOLOR_888);
  }
  doc.clear();
  applyAllSettings();
  saveSettings();
  if (DEBUG_MODE)
    DEBUG_PRINTLN("Settings loaded");
}
