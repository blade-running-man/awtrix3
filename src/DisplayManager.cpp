#include <DisplayManager.h>
#include "DisplayManager/DisplayManager_Internal.h"
#include "MatrixDisplayUi.h"
#include <TJpg_Decoder.h>
#include "icons.h"
#include "Globals.h"
#include "PeripheryManager.h"
#include "MQTTManager.h"
#include "GifPlayer.h"
#include <Ticker.h>
#include "timer.h"
#include "Functions.h"
#include "MenuManager.h"
#include "Apps.h"
#include "effects.h"
#include "Overlays.h"
#include <ArtnetWifi.h>
#include "Games/GameManager.h"

// --- Global variable definitions ---

unsigned long lastArtnetStatusTime = 0;
ArtnetWifi artnet;

#ifdef awtrix2_upgrade
#define MATRIX_PIN D2
#else
#define MATRIX_PIN 32
#endif

fs::File gifFile;
GifPlayer gif;

uint16_t gifX, gifY;
CRGB leds[MATRIX_WIDTH * MATRIX_HEIGHT];
CRGB ledsCopy[MATRIX_WIDTH * MATRIX_HEIGHT];
float actualBri;
int16_t cursor_x, cursor_y;
uint32_t textColor;

// NeoMatrix
FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(leds, 8, 8, 4, 1, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE);
MatrixDisplayUi *ui = new MatrixDisplayUi(matrix);

// --- Singleton ---

DisplayManager_ &DisplayManager_::getInstance()
{
  static DisplayManager_ instance;
  return instance;
}

DisplayManager_ &DisplayManager = DisplayManager.getInstance();

// --- Core methods ---

void DisplayManager_::setBrightness(int bri)
{
  bool wakeup = false;
  if (!notifications.empty())
  {
    wakeup = notifications[0].wakeup;
  }

  if (MATRIX_OFF && !wakeup)
  {
    matrix->setBrightness(0);
  }
  else
  {
    matrix->setBrightness(bri);
    actualBri = bri;
  }
}

bool DisplayManager_::setAutoTransition(bool active)
{
  if (ui->AppCount < 2)
  {
    ui->disablesetAutoTransition();
    return false;
  }
  if (active && AUTO_TRANSITION)
  {
    ui->enablesetAutoTransition();
    return true;
  }
  else
  {
    ui->disablesetAutoTransition();
    return false;
  }
}

void DisplayManager_::applyAllSettings()
{
  ui->setTargetFPS(MATRIX_FPS);
  ui->setTimePerApp(TIME_PER_APP);
  ui->setTimePerTransition(TIME_PER_TRANSITION);

  if (!AUTO_BRIGHTNESS)
    setBrightness(BRIGHTNESS);
  setTextColor(TEXTCOLOR_888);
  setAutoTransition(AUTO_TRANSITION);
}

void DisplayManager_::setAppTime(long duration)
{
  ui->setTimePerApp(duration);
}

void DisplayManager_::clearMatrix()
{
  matrix->clear();
  matrix->show();
}

void DisplayManager_::setup()
{
  TJpgDec.setCallback(jpg_output);
  TJpgDec.setJpgScale(1);
  random16_set_seed(millis());
  FastLED.addLeds<NEOPIXEL, MATRIX_PIN>(leds, MATRIX_WIDTH * MATRIX_HEIGHT);
  setMatrixLayout(MATRIX_LAYOUT);
  matrix->setRotation(ROTATE_SCREEN ? 90 : 0);
  GAMMA = 1.9;
  if (COLOR_CORRECTION)
  {
    FastLED.setCorrection(COLOR_CORRECTION);
  }
  if (COLOR_TEMPERATURE)
  {
    FastLED.setTemperature(COLOR_TEMPERATURE);
  }
  gif.setMatrix(matrix);
  ui->setAppAnimation(SLIDE_DOWN);

  ui->setTargetFPS(MATRIX_FPS);
  ui->setTimePerApp(TIME_PER_APP);
  ui->setTimePerTransition(TIME_PER_TRANSITION);
  ui->setOverlays(overlays, 3);
  ui->setBackgroundEffect(BACKGROUND_EFFECT);
  setAutoTransition(AUTO_TRANSITION);
  ui->init();
}

void DisplayManager_::tick()
{
  if (GAME_ACTIVE)
  {
    GameManager.tick();
    matrix->show();
    memcpy(ledsCopy, leds, sizeof(leds));
  }
  else if (AP_MODE)
  {
    HSVtext(2, 6, "AP MODE", true, 1);
  }
  else if (ARTNET_MODE)
  {
    // handled by the DMXFrame callback
  }
  else if (MOODLIGHT_MODE)
  {
    // handled by the moodlight function
  }
  else
  {
    ui->update();
    if (ui->getUiState()->appState == IN_TRANSITION && !appIsSwitching)
    {
      appIsSwitching = true;
    }
    else if (ui->getUiState()->appState == FIXED && appIsSwitching)
    {

      appIsSwitching = false;
      MQTTManager.setCurrentApp(CURRENT_APP);
      setAppTime(TIME_PER_APP);
      checkLifetime(ui->getnextAppNumber());
      ResetCustomApps();
    }
  }

  if (!AP_MODE)
  {
    uint16_t ArtnetStatus = artnet.read();
    if (ArtnetStatus > 0)
    {
      lastArtnetStatusTime = millis();
      ARTNET_MODE = true;
    }
    else if (millis() - lastArtnetStatusTime > 1000)
    {
      ARTNET_MODE = false;
    }
  }

  if (NEWYEAR)
    DisplayManager.checkNewYear();
}

bool newYearEventTriggered = false;

void DisplayManager_::checkNewYear()
{
  struct tm *timeInfo = timer_localtime();
  if (timeInfo->tm_mon == 0 && timeInfo->tm_mday == 1 && timeInfo->tm_hour == 0 && timeInfo->tm_min == 0 && timeInfo->tm_sec == 0)
  {
    if (!newYearEventTriggered)
    {
      int year = 1900 + timeInfo->tm_year;
      char message[300];
      sprintf(message, "{'stack':false,'text':'%d','duration':20,'effect':'Fireworks','rtttl':'Auld:d=4,o=6,b=125:a5,d.,8d,d,f#,e.,8d,e,8f#,8e,d.,8d,f#,a,2b.,b,a.,8f#,f#,d,e.,8d,e,8f#,8e,d.,8b5,b5,a5,2d,16p'}", year);
      DisplayManager.generateNotification(0, message);
      newYearEventTriggered = true;
    }
  }
  else
  {
    newYearEventTriggered = false;
  }
}

void DisplayManager_::clear()
{
  matrix->clear();
}

void DisplayManager_::show()
{
  matrix->show();
}

void DisplayManager_::leftButton()
{
  if (!MenuManager.inMenu)
    ui->previousApp();
}

void DisplayManager_::rightButton()
{
  if (!MenuManager.inMenu)
    ui->nextApp();
}

void DisplayManager_::nextApp()
{
  if (!MenuManager.inMenu)
  {
    if (DEBUG_MODE)
      DEBUG_PRINTLN(F("Switching to next app"));
    ui->nextApp();
  }
}

void DisplayManager_::forceNextApp()
{
  ui->switchToApp(ui->getUiState()->currentApp);
  setAppTime(TIME_PER_APP);
  MQTTManager.setCurrentApp(getAppNameAtIndex(ui->getUiState()->currentApp));
}

void DisplayManager_::previousApp()
{
  if (!MenuManager.inMenu)
  {
    if (DEBUG_MODE)
      DEBUG_PRINTLN(F("Switching to previous app"));
    ui->previousApp();
  }
}

void DisplayManager_::selectButton()
{
  if (!MenuManager.inMenu)
  {
    DisplayManager.getInstance().dismissNotify();
  }
}

void DisplayManager_::selectButtonLong()
{
}

void DisplayManager_::setPower(bool state)
{
  if (state)
  {
    MATRIX_OFF = false;
    setBrightness(BRIGHTNESS);
  }
  else
  {
    MATRIX_OFF = true;
    showSleepAnimation();
    setBrightness(0);
  }
}

void DisplayManager_::powerStateParse(const char *json)
{
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error)
  {
    setPower((strcmp(json, "true") == 0 || strcmp(json, "1") == 0) ? true : false);
    return;
  }

  if (doc.containsKey("power"))
  {
    bool power = doc["power"].as<bool>();
    setPower(power);
  }
}

void DisplayManager_::showSleepAnimation()
{
  setTextColor(0xFFFFFF);
  int steps[][2] = {{12, 8}, {13, 7}, {14, 6}, {15, 5}, {14, 4}, {13, 3}, {12, 2}, {13, 1}, {14, 0}, {15, -1}, {14, -2}, {13, -3}, {12, -4}, {13, -5}};
  int numSteps = sizeof(steps) / sizeof(steps[0]);
  for (int i = 0; i < numSteps; i++)
  {
    clear();
    printText(steps[i][0], steps[i][1], "Z", false, 1);
    show();
    delay(80);
  }
}

CRGB DisplayManager_::getPixelColor(int16_t x, int16_t y)
{
  int index = matrix->XY(x, y);
  return leds[index];
}
