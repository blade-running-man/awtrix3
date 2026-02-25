#include <DisplayManager.h>
#include "DisplayManager_Internal.h"
#include "Globals.h"
#include "Overlays.h"
#include "Apps.h"
#include "Functions.h"
#include "MQTTManager.h"
#include "PeripheryManager.h"
#include "effects.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include "base64.hpp"

bool DisplayManager_::generateNotification(uint8_t source, const char *json)
{
  // source: 0=MQTT, 1=HTTP
  DynamicJsonDocument doc(6144);
  DeserializationError error = deserializeJson(doc, json);
  if (error)
  {
    DEBUG_PRINTLN(error.c_str());
    doc.clear();
    return false;
  }

  Notification newNotification;
  JsonObject docObj = doc.as<JsonObject>();

  // Parse all common fields shared with CustomApp
  parseCommonDisplayFields(newNotification, docObj);

  // Notification-specific fields
  newNotification.duration = doc.containsKey("duration") ? doc["duration"].as<long>() * 1000 : TIME_PER_APP;
  newNotification.loopSound = doc.containsKey("loopSound") ? doc["loopSound"].as<bool>() : false;
  newNotification.sound = doc.containsKey("sound") ? doc["sound"].as<String>() : "";
  newNotification.rtttl = doc.containsKey("rtttl") ? doc["rtttl"].as<String>() : "";
  newNotification.hold = doc.containsKey("hold") ? doc["hold"].as<bool>() : false;
  newNotification.wakeup = doc.containsKey("wakeup") ? doc["wakeup"].as<bool>() : false;
  newNotification.scrollposition = 9 + newNotification.textOffset;
  newNotification.iconWasPushed = false;
  newNotification.iconPosition = 0;
  newNotification.scrollDelay = 0;

  // Icon handling (Notification opens files directly)
  if (doc.containsKey("icon"))
  {
    String iconValue = doc["icon"].as<String>();

    if (iconValue.length() > 64)
    {
      newNotification.jpegDataSize = decode_base64((const unsigned char *)iconValue.c_str(), newNotification.jpegDataBuffer);
      newNotification.isGif = false;
    }
    else
    {
      newNotification.jpegDataSize = 0;
      if (LittleFS.exists("/ICONS/" + iconValue + ".jpg"))
      {
        newNotification.isGif = false;
        newNotification.icon = LittleFS.open("/ICONS/" + iconValue + ".jpg");
      }
      else if (LittleFS.exists("/ICONS/" + iconValue + ".gif"))
      {
        newNotification.isGif = true;
        newNotification.icon = LittleFS.open("/ICONS/" + iconValue + ".gif");
      }
      else
      {
        fs::File nullPointer;
        newNotification.icon = nullPointer;
        newNotification.isGif = false;
      }
    }
  }
  else
  {
    fs::File nullPointer;
    newNotification.icon = nullPointer;
    newNotification.jpegDataSize = 0;
    newNotification.isGif = false;
  }

  if (doc.containsKey("clients"))
  {
    JsonArray ClientNames = doc["clients"];
    doc.remove("clients");
    String modifiedJson;
    serializeJson(doc, modifiedJson);
    for (JsonVariant c : ClientNames)
    {
      String client = c.as<String>();
      if (source == 0)
      {
        MQTTManager.rawPublish(client.c_str(), "notify", modifiedJson.c_str());
      }
      else
      {
        HTTPClient http;
        http.begin("http://" + client + "/api/notify");
        http.POST(modifiedJson);
        http.end();
      }
    }
  }
  newNotification.startime = millis();
  CURRENT_APP = "Notification";
  MQTTManager.setCurrentApp(CURRENT_APP);

  bool stack = doc.containsKey("stack") ? doc["stack"] : true;

  if (stack)
  {
    notifications.push_back(newNotification);
  }
  else
  {
    if (notifications.empty())
    {
      notifications.push_back(newNotification);
    }
    else
    {
      notifications[0] = newNotification;
    }
  }

  doc.clear();
  return true;
}

void DisplayManager_::dismissNotify()
{
  bool wakeup = false;
  if (!notifications.empty())
  {
    if (notifications.size() >= 2)
    {
      notifications[1].startime = millis();
    }
    wakeup = notifications[0].wakeup;
    notifications[0].icon.close();
    notifications.erase(notifications.begin());
    PeripheryManager.stopSound();
  }
  if (notifications.empty())
  {
    if (wakeup && MATRIX_OFF)
    {
      DisplayManager.setBrightness(0);
    }
  }
}
