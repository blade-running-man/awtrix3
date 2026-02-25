#include <DisplayManager.h>
#include "DisplayManager_Internal.h"
#include "Globals.h"
#include "Functions.h"
#include "effects.h"
#include "Dictionary.h"
#include <ArduinoJson.h>
#include <ArtnetWifi.h>

const int numberOfChannels = 256 * 3;
const int startUniverse = 0;
const int maxUniverses = numberOfChannels / 512 + ((numberOfChannels % 512) ? 1 : 0);
bool universesReceived[maxUniverses];
bool sendFrame = 1;
int previousDataLength = 0;
uint8_t received_packets = 0;
bool universe1_complete = false;
bool universe2_complete = false;

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t *data)
{
  sendFrame = 1;
  // set brightness of the whole matrix
  if (universe == 10)
  {
    matrix->setBrightness(data[0]);
    matrix->show();
  }

  // Store which universe has got in
  if ((universe - startUniverse) < maxUniverses)
    universesReceived[universe - startUniverse] = 1;

  for (int i = 0; i < maxUniverses; i++)
  {
    if (universesReceived[i] == 0)
    {
      // Serial.println("Broke");
      sendFrame = 0;
      break;
    }
  }

  // read universe and put into the right part of the display buffer
  for (int i = 0; i < length / 3; i++)
  {
    int led = i + (universe - startUniverse) * (previousDataLength / 3);
    if (led < 256)
      matrix->drawPixel(led % matrix->width(), led / matrix->width(), matrix->Color(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]));
  }
  previousDataLength = length;

  if (sendFrame)
  {
    matrix->show();
    // Reset universeReceived to 0
    memset(universesReceived, 0, maxUniverses);
  }
}

void DisplayManager_::startArtnet()
{
  artnet.begin();
  artnet.setArtDmxCallback(onDmxFrame);
}

bool DisplayManager_::moodlight(const char *json)
{
  if (strcmp(json, "") == 0)
  {
    MOODLIGHT_MODE = false;
    return true;
  }

  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, json);
  if (error)
    return false;

  int brightness = doc["brightness"] | BRIGHTNESS;
  matrix->setBrightness(brightness);

  if (doc.containsKey("kelvin"))
  {
    int kelvin = doc["kelvin"];
    CRGB color;
    color = kelvinToRGB(kelvin);
    color.nscale8(brightness);
    for (int i = 0; i < 256; i++)
    {
      leds[i] = color;
    }
  }
  else if (doc.containsKey("color"))
  {
    auto c = doc["color"];
    uint32_t color888 = getColorFromJsonVariant(c, TEXTCOLOR_888);
    drawFilledRect(0, 0, 32, 8, color888);
  }
  else
  {
    doc.clear();
    return true;
  }

  MOODLIGHT_MODE = true;
  doc.clear();
  matrix->show();
  return true;
}

String DisplayManager_::ledsAsJson()
{
  StaticJsonDocument<JSON_ARRAY_SIZE(MATRIX_WIDTH * MATRIX_HEIGHT)> jsonDoc;
  JsonArray jsonColors = jsonDoc.to<JsonArray>();
  for (int y = 0; y < MATRIX_HEIGHT; y++)
  {
    for (int x = 0; x < MATRIX_WIDTH; x++)
    {
      int index = matrix->XY(x, y);
      int color = (ledsCopy[index].r << 16) | (ledsCopy[index].g << 8) | ledsCopy[index].b;
      jsonColors.add(color);
    }
  }
  String jsonString;
  serializeJson(jsonColors, jsonString);
  return jsonString;
}

CRGB *DisplayManager_::getLeds()
{
  return leds;
}

String DisplayManager_::getEffectNames()
{
  StaticJsonDocument<1024> doc;
  JsonArray array = doc.to<JsonArray>();
  for (int i = 0; i < numOfEffects; i++)
  {
    array.add(effects[i].name);
  }
  String result;
  serializeJson(array, result);
  doc.clear();
  return result;
}

String DisplayManager_::getTransitionNames()
{
  char effectOptions[100];
  strcpy_P(effectOptions, HAeffectOptions);
  StaticJsonDocument<1024> doc;
  char *effect = strtok(effectOptions, ";");
  while (effect != NULL)
  {
    doc.add(effect);
    effect = strtok(NULL, ";");
  }
  String json;
  serializeJson(doc, json);
  doc.clear();
  return json;
}
