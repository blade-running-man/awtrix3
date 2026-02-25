#ifndef DisplayManager_Internal_h
#define DisplayManager_Internal_h

#include <Arduino.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
#include "MatrixDisplayUi.h"
#include "GifPlayer.h"
#include <ArtnetWifi.h>
#include <ArduinoJson.h>
#include "Apps.h"
#include "Functions.h"
#include "effects.h"

// Matrix dimensions
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8

// Core shared globals (defined in DisplayManager.cpp)
extern CRGB leds[MATRIX_WIDTH * MATRIX_HEIGHT];
extern CRGB ledsCopy[MATRIX_WIDTH * MATRIX_HEIGHT];
extern FastLED_NeoMatrix *matrix;
extern MatrixDisplayUi *ui;
extern float actualBri;
extern int16_t cursor_x, cursor_y;
extern uint32_t textColor;

// GIF globals (defined in DisplayManager.cpp)
extern fs::File gifFile;
extern GifPlayer gif;
extern uint16_t gifX, gifY;

// Artnet globals (defined in DisplayManager.cpp)
extern ArtnetWifi artnet;
extern unsigned long lastArtnetStatusTime;

// Free functions shared across split files

// Apps-related (defined in DisplayManager_Apps.cpp)
void pushCustomApp(String name, int position);
void removeCustomAppFromApps(const String &name, bool setApps);
bool deleteCustomAppFile(const String &name);
void ResetCustomApps();
void checkLifetime(uint8_t pos);
bool parseFragmentsText(const JsonArray &fragmentArray, std::vector<uint32_t> &colors, std::vector<String> &fragments, uint32_t standardColor);
void subscribeToPlaceholders(String text);
std::pair<String, AppCallback> getNativeAppByName(const String &appName);

// Text-related (defined in DisplayManager_Text.cpp)
uint32_t interpolateColor(uint32_t color1, uint32_t color2, float t);

// Drawing-related (defined in DisplayManager_Drawing.cpp)
bool jpg_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);

// Settings-related (defined in DisplayManager_Settings.cpp)
String CRGBtoHex(CRGB color);
String getOverlayName();
float logMap(float x, float in_min, float in_max, float out_min, float out_max, float mid_point_out);

// Artnet-related (defined in DisplayManager_Artnet.cpp)
void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t *data);

// Common JSON parsing for CustomApp and Notification structs.
// Both structs share the same field names for these properties.
template <typename T>
void parseCommonDisplayFields(T &target, JsonObject &doc)
{
  target.progress = doc.containsKey("progress") ? doc["progress"].as<int>() : -1;

  if (doc.containsKey("progressC"))
  {
    auto progressC = doc["progressC"];
    target.pColor = getColorFromJsonVariant(progressC, 0x00FF00);
  }
  else
  {
    target.pColor = 0x00FF00;
  }

  if (doc.containsKey("progressBC"))
  {
    auto progressBC = doc["progressBC"];
    target.pbColor = getColorFromJsonVariant(progressBC, 0xFFFFFF);
  }
  else
  {
    target.pbColor = 0xFFFFFF;
  }

  if (doc.containsKey("background"))
  {
    auto background = doc["background"];
    target.background = getColorFromJsonVariant(background, 0);
  }
  else
  {
    target.background = 0;
  }

  target.drawInstructions = doc.containsKey("draw") ? doc["draw"].as<String>() : "";

  if (doc.containsKey("effect"))
  {
    target.effect = getEffectIndex(doc["effect"].as<String>());
    if (doc.containsKey("effectSettings"))
    {
      updateEffectSettings(target.effect, doc["effectSettings"].as<String>());
    }
  }

  target.overlay = doc.containsKey("overlay") ? getOverlay(doc["overlay"].as<String>()) : NONE;
  target.rainbow = doc.containsKey("rainbow") ? doc["rainbow"].as<bool>() : false;
  target.pushIcon = doc.containsKey("pushIcon") ? doc["pushIcon"] : 0;
  target.textCase = doc.containsKey("textCase") ? doc["textCase"] : 0;
  target.iconOffset = doc.containsKey("iconOffset") ? doc["iconOffset"] : 0;
  target.textOffset = doc.containsKey("textOffset") ? doc["textOffset"] : 0;
  target.scrollSpeed = doc.containsKey("scrollSpeed") ? doc["scrollSpeed"].as<int>() : -1;
  target.topText = doc.containsKey("topText") ? doc["topText"].as<bool>() : false;
  target.fade = doc.containsKey("fadeText") ? doc["fadeText"].as<int>() : 0;
  target.blink = doc.containsKey("blinkText") ? doc["blinkText"].as<int>() : 0;
  target.center = doc.containsKey("center") ? doc["center"].as<bool>() : true;
  target.noScrolling = doc.containsKey("noScroll") ? doc["noScroll"] : false;
  target.repeat = doc.containsKey("repeat") ? doc["repeat"].as<int>() : -1;

  if (target.noScrolling)
  {
    target.repeat = -1;
  }

  target.gradient[0] = -1;
  target.gradient[1] = -1;
  if (doc.containsKey("gradient"))
  {
    JsonArray arr = doc["gradient"].as<JsonArray>();
    if (arr.size() == 2)
    {
      auto color1 = arr[0];
      auto color2 = arr[1];
      target.gradient[0] = getColorFromJsonVariant(color1, TEXTCOLOR_888);
      target.gradient[1] = getColorFromJsonVariant(color2, TEXTCOLOR_888);
    }
  }

  bool autoscale = doc.containsKey("autoscale") ? doc["autoscale"].as<bool>() : true;

  const char *dataKeys[] = {"bar", "line"};
  int *dataArrays[] = {target.barData, target.lineData};
  int *dataSizeArrays[] = {&target.barSize, &target.lineSize};

  for (int i = 0; i < 2; i++)
  {
    const char *key = dataKeys[i];
    int *dataArray = dataArrays[i];
    int *dataSize = dataSizeArrays[i];

    if (doc.containsKey(key))
    {
      if (doc.containsKey("barBC"))
      {
        auto color = doc["barBC"];
        target.barBG = getColorFromJsonVariant(color, 0);
      }
      else
      {
        target.barBG = 0;
      }
      JsonArray data = doc[key];
      int index = 0;
      int maximum = 0;
      for (JsonVariant v : data)
      {
        if (index >= 16)
          break;
        int d = v.as<int>();
        if (d > maximum)
          maximum = d;
        dataArray[index] = d;
        index++;
      }
      *dataSize = index;

      if (autoscale && maximum > 0)
      {
        for (int j = 0; j < *dataSize; j++)
        {
          dataArray[j] = map(dataArray[j], 0, maximum, 0, 8);
        }
      }
    }
    else
    {
      *dataSize = 0;
    }
  }

  if (doc.containsKey("color"))
  {
    auto color = doc["color"];
    target.color = getColorFromJsonVariant(color, TEXTCOLOR_888);
  }
  else
  {
    target.color = TEXTCOLOR_888;
  }

  target.colors.clear();
  target.fragments.clear();

  if (doc.containsKey("text") && doc["text"].is<JsonArray>())
  {
    JsonArray textArray = doc["text"].as<JsonArray>();
    parseFragmentsText(textArray, target.colors, target.fragments, target.color);
  }
  else if (doc.containsKey("text") && doc["text"].is<String>())
  {
    String text = doc["text"].as<String>();
    target.text = utf8ascii(text);
  }
  else
  {
    target.text = "";
  }
}

#endif
