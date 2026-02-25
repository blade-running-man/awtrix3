#include <DisplayManager.h>
#include "DisplayManager_Internal.h"
#include "Globals.h"
#include "Apps.h"
#include "Functions.h"
#include "MQTTManager.h"
#include "effects.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "base64.hpp"
#include "Overlays.h"

void pushCustomApp(String name, int position)
{
  if (customApps.count(name) == 0)
  {
    int availableCallbackIndex = -1;

    for (int i = 0; i < 20; ++i)
    {
      bool callbackUsed = false;

      for (const auto &appPair : Apps)
      {
        if (appPair.second == customAppCallbacks[i])
        {
          callbackUsed = true;
          break;
        }
      }

      if (!callbackUsed)
      {
        availableCallbackIndex = i;
        break;
      }
    }

    if (availableCallbackIndex == -1)
    {
      if (DEBUG_MODE)
        DEBUG_PRINTLN(F("Error adding custom app -> Maximum number of custom apps reached"));
      return;
    }

    if (position < 0) // Insert at the end of the vector
    {
      Apps.push_back(std::make_pair(name, customAppCallbacks[availableCallbackIndex]));
    }
    else if (position < Apps.size()) // Insert at a specific position
    {
      Apps.insert(Apps.begin() + position, std::make_pair(name, customAppCallbacks[availableCallbackIndex]));
    }
    else // Invalid position, Insert at the end of the vector
    {
      Apps.push_back(std::make_pair(name, customAppCallbacks[availableCallbackIndex]));
    }

    ui->setApps(Apps); // Add Apps
    DisplayManager.getInstance().setAutoTransition(true);
  }
}

bool deleteCustomAppFile(const String &name)
{
  // Create the file name based on the app name
  String fileName = "/CUSTOMAPPS/" + name + ".json";

  // Check if the file exists
  if (!LittleFS.exists(fileName))
  {
    if (DEBUG_MODE)
      DEBUG_PRINTLN("File does not exist: " + fileName + ". No need to delete it");
    return false;
  }

  // Delete the file
  if (LittleFS.remove(fileName))
  {
    if (DEBUG_MODE)
      DEBUG_PRINTLN("File removed successfully: " + fileName);
    return true;
  }
  else
  {
    if (DEBUG_MODE)
      DEBUG_PRINTLN("Failed to remove file: " + fileName);
    return false;
  }
}

void removeCustomAppFromApps(const String &name, bool setApps)
{
  // Remove apps from Apps list
  auto it = Apps.begin();
  while (it != Apps.end())
  {
    if (it->first.startsWith(name))
    {
      it = Apps.erase(it);
    }
    else
    {
      ++it;
    }
  }

  // Remove apps from customApps map
  auto mapIt = customApps.begin();
  while (mapIt != customApps.end())
  {
    if (mapIt->first.startsWith(name))
    {
      mapIt = customApps.erase(mapIt);
    }
    else
    {
      ++mapIt;
    }
  }

  if (setApps)
    ui->setApps(Apps);
  DisplayManager.getInstance().setAutoTransition(true);
  deleteCustomAppFile(name);
  DisplayManager.setAppTime(TIME_PER_APP);
}

bool parseFragmentsText(const JsonArray &fragmentArray, std::vector<uint32_t> &colors, std::vector<String> &fragments, uint32_t standardColor)
{
  colors.clear();
  fragments.clear();

  for (JsonObject fragmentObj : fragmentArray)
  {
    String textFragment = fragmentObj["t"];
    uint32_t color;
    if (fragmentObj.containsKey("c"))
    {
      auto fragColor = fragmentObj["c"];
      color = getColorFromJsonVariant(fragColor, standardColor);
    }
    else
    {
      color = standardColor;
    }

    fragments.push_back(utf8ascii(textFragment));
    colors.push_back(color);
  }
  return true;
}

// Function to subscribe to MQTT topics based on placeholders in text
void subscribeToPlaceholders(String text)
{
  int start = 0;
  while ((start = text.indexOf("{{", start)) != -1)
  {
    int end = text.indexOf("}}", start);
    if (end == -1)
    {
      break;
    }
    String placeholder = text.substring(start + 2, end);
    String topic = placeholder;

    MQTTManager.subscribe(topic.c_str());

    start = end + 2;
  }
}

bool DisplayManager_::parseCustomPage(const String &name, const char *json, bool preventSave)
{
  if ((strcmp(json, "") == 0) || (strcmp(json, "{}") == 0))
  {
    removeCustomAppFromApps(name, true);
    return true;
  }

  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, json);
  if (error)
  {
    DEBUG_PRINTLN(error.c_str());
    doc.clear();
    return false;
  }

  if (doc.is<JsonObject>())
  {
    JsonObject rootObj = doc.as<JsonObject>();
    return generateCustomPage(name, rootObj, preventSave);
  }
  else if (doc.is<JsonArray>())
  {
    JsonArray customPagesArray = doc.as<JsonArray>();
    int cpIndex = 0;
    for (JsonObject customPageObject : customPagesArray)
    {
      generateCustomPage(name + String(cpIndex), customPageObject, preventSave);
      ++cpIndex;
    }
  }

  doc.clear();
  return true;
}

bool DisplayManager_::generateCustomPage(const String &name, JsonObject doc, bool preventSave)
{
  CustomApp customApp;

  if (customApps.find(name) != customApps.end())
  {
    customApp = customApps[name];
  }

  if (doc.containsKey("save") && preventSave == false)
  {
    bool saveApp = doc["save"].as<bool>();
    if (saveApp)
    {
      if (!LittleFS.exists("/CUSTOMAPPS"))
      {
        LittleFS.mkdir("/CUSTOMAPPS");
      }
      File file = LittleFS.open("/CUSTOMAPPS/" + name + ".json", "w");
      if (!file)
      {
        return false;
      }
      serializeJson(doc, file);
      file.close();
    }
  }

  // Parse all common fields shared with Notification
  parseCommonDisplayFields(customApp, doc);

  // CustomApp-specific fields
  customApp.hasCustomColor = doc.containsKey("color");
  customApp.duration = doc.containsKey("duration") ? doc["duration"].as<long>() * 1000 : 0;
  int pos = doc.containsKey("pos") ? doc["pos"].as<uint8_t>() : -1;
  customApp.name = name;
  customApp.bounce = doc.containsKey("bounce") ? doc["bounce"].as<bool>() : false;

  if (doc.containsKey("lifetime"))
  {
    customApp.lifetime = doc["lifetime"];
    customApp.lifetimeMode = doc.containsKey("lifetimeMode") ? doc["lifetimeMode"] : 0;
  }
  else
  {
    customApp.lifetime = 0;
  }

  // CustomApp subscribes to MQTT placeholders in text
  if (!customApp.text.isEmpty() && customApp.fragments.empty())
  {
    subscribeToPlaceholders(customApp.text);
  }

  // Icon handling (CustomApp tracks icon name changes)
  if (doc.containsKey("icon"))
  {
    String newIconName = doc["icon"].as<String>();

    if (newIconName.length() > 64)
    {
      customApp.jpegDataSize = decode_base64((const unsigned char *)newIconName.c_str(), customApp.jpegDataBuffer);
      customApp.isGif = false;
      customApp.icon.close();
      customApp.iconName = "";
      customApp.iconPosition = 0;
      customApp.currentFrame = 0;
    }
    else if (customApp.iconName != newIconName)
    {
      customApp.jpegDataSize = 0;
      customApp.iconName = newIconName;
      customApp.icon.close();
      customApp.iconPosition = 0;
      customApp.currentFrame = 0;
    }
  }
  else
  {
    customApp.jpegDataSize = 0;
    customApp.icon.close();
    customApp.iconName = "";
    customApp.iconPosition = 0;
    customApp.currentFrame = 0;
  }

  if (currentCustomApp != name)
  {
    customApp.scrollposition = 9 + customApp.textOffset;
  }

  customApp.lastUpdate = millis();
  customApp.lifeTimeEnd = false;
  doc.clear();
  pushCustomApp(name, pos - 1);
  customApps[name] = customApp;

  return true;
}

void DisplayManager_::loadCustomApps()
{
  File root = LittleFS.open("/CUSTOMAPPS");

  if (!root)
  {
    if (DEBUG_MODE)
      DEBUG_PRINTLN("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    if (DEBUG_MODE)
      DEBUG_PRINTLN("/CUSTOMAPPS is not a directory");
    return;
  }

  File file = root.openNextFile();

  while (file)
  {
    if (file.isDirectory())
    {
      file = root.openNextFile();
      continue;
    }

    String fileName = file.name();
    String json = file.readString();
    file.close();

    String name = fileName.substring(fileName.lastIndexOf('/') + 1, fileName.lastIndexOf('.')); // remove path and .json extension
    parseCustomPage(name, json.c_str(), true);

    file = root.openNextFile();
  }
}

void DisplayManager_::loadNativeApps()
{
  // Define a helper function to check and update an app
  auto updateApp = [&](const String &name, AppCallback callback, bool show, size_t position)
  {
    auto it = std::find_if(Apps.begin(), Apps.end(), [&](const std::pair<String, AppCallback> &app)
                           { return app.first == name; });
    if (it != Apps.end())
    {
      if (!show)
      {
        Apps.erase(it);
      }
    }
    else
    {
      if (show)
      {
        if (position >= Apps.size())
        {
          Apps.push_back(std::make_pair(name, callback));
        }
        else
        {
          Apps.insert(Apps.begin() + position, std::make_pair(name, callback));
        }
      }
    }
  };

  updateApp("Time", TimeApp, SHOW_TIME, 0);
  updateApp("Date", DateApp, SHOW_DATE, 1);

  if (SENSOR_READING)
  {
    updateApp("Temperature", TempApp, SHOW_TEMP, 2);
    updateApp("Humidity", HumApp, SHOW_HUM, 3);
  }
#ifdef ULANZI
  updateApp("Battery", BatApp, SHOW_BAT, 4);
#endif

  ui->setApps(Apps);
  setAutoTransition(true);
}

void ResetCustomApps()
{
  if (customApps.empty())
  {
    return;
  }

  for (auto it = customApps.begin(); it != customApps.end(); ++it)
  {
    CustomApp &app = it->second;
    if (app.name != currentCustomApp)
    {
      app.iconWasPushed = false;
      app.scrollposition = (app.icon ? 9 : 0) + app.textOffset;
      app.iconPosition = 0;
      app.scrollDelay = 0;
      app.currentRepeat = 0;
      app.icon.close();
      app.currentFrame = 0;
    }
  }
}

void checkLifetime(uint8_t pos)
{
  if (customApps.empty())
  {
    return;
  }

  if (pos >= Apps.size())
  {
    pos = 0;
  }

  String appName = Apps[pos].first;
  auto appIt = customApps.find(appName);

  if (appIt != customApps.end())
  {
    CustomApp &app = appIt->second;

    if (app.lifetime > 0 && (millis() - app.lastUpdate) / 1000 >= app.lifetime)
    {
      if (app.lifetimeMode == 0)
      {
        if (DEBUG_MODE)
          DEBUG_PRINTLN("Removing " + appName + " -> Lifetime over");
        removeCustomAppFromApps(appName, false);

        if (DEBUG_MODE)
          DEBUG_PRINTLN("Set new Apploop");
        ui->setApps(Apps);
      }
      else if (app.lifetimeMode == 1)
      {
        app.lifeTimeEnd = true;
      }
    }
  }
}

std::pair<String, AppCallback> getNativeAppByName(const String &appName)
{
  if (appName == "Time")
  {
    return std::make_pair("Time", TimeApp);
  }
  else if (appName == "Date")
  {
    return std::make_pair("Date", DateApp);
  }
  else if (appName == "Temperature")
  {
    return std::make_pair("Temperature", TempApp);
  }
  else if (appName == "Humidity")
  {
    return std::make_pair("Humidity", HumApp);
  }
#ifdef ULANZI
  else if (appName == "Battery")
  {
    return std::make_pair("Battery", BatApp);
  }
#endif
  return std::make_pair("", nullptr);
}

void DisplayManager_::updateAppVector(const char *json)
{
  if (DEBUG_MODE)
    DEBUG_PRINTLN(F("New apps vector received"));
  if (DEBUG_MODE)
    DEBUG_PRINTLN(json);
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, json);
  if (error)
  {
    doc.clear();
    if (DEBUG_MODE)
      DEBUG_PRINTLN(F("Failed to parse json"));
    return;
  }

  JsonArray appArray;
  DynamicJsonDocument wrapperDoc(2048);
  if (doc.is<JsonObject>())
  {
    JsonArray tempArray = wrapperDoc.to<JsonArray>();
    tempArray.add(doc.as<JsonObject>());
    appArray = tempArray;
  }
  else if (doc.is<JsonArray>())
  {
    appArray = doc.as<JsonArray>();
  }

  for (JsonObject appObj : appArray)
  {
    String appName = appObj["name"].as<String>();
    bool show = appObj["show"].as<bool>();
    int position = appObj.containsKey("pos") ? appObj["pos"].as<int>() : Apps.size();

    auto appIt = std::find_if(Apps.begin(), Apps.end(), [&appName](const std::pair<String, AppCallback> &app)
                              { return app.first == appName; });

    std::pair<String, AppCallback> nativeApp = getNativeAppByName(appName);

    if (!show)
    {
      if (appIt != Apps.end())
      {
        Apps.erase(appIt);
      }
    }
    else
    {
      if (nativeApp.second != nullptr)
      {
        if (appIt != Apps.end())
        {
          Apps.erase(appIt);
        }
        position = position < 0 ? 0 : position >= Apps.size() ? Apps.size()
                                                              : position;
        Apps.insert(Apps.begin() + position, nativeApp);
      }
      else
      {
        if (appIt != Apps.end() && appObj.containsKey("pos"))
        {
          std::pair<String, AppCallback> app = *appIt;
          Apps.erase(appIt);
          position = position < 0 ? 0 : position >= Apps.size() ? Apps.size()
                                                                : position;
          Apps.insert(Apps.begin() + position, app);
        }
      }
    }
  }

  // Set the updated apps vector in the UI and save settings
  ui->setApps(Apps);
  saveSettings();
  sendAppLoop();
  setAutoTransition(AUTO_TRANSITION);
  doc.clear();
}

bool DisplayManager_::switchToApp(const char *json)
{
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, json);
  if (error)
  {
    doc.clear();
    return false;
  }

  String name = doc["name"].as<String>();
  bool fast = doc["fast"] | false;
  doc.clear();
  int index = findAppIndexByName(name);
  if (index > -1)
  {
    if (fast)
    {
      ui->switchToApp(index);
      return true;
    }
    else
    {
      ui->transitionToApp(index);
      return true;
    }
  }
  else
  {
    return false;
  }
}

String DisplayManager_::getAppsAsJson()
{
  DynamicJsonDocument doc(1024);
  JsonObject appsObject = doc.to<JsonObject>();
  for (size_t i = 0; i < Apps.size(); i++)
  {
    appsObject[Apps[i].first] = i;
  }
  String json;
  serializeJson(appsObject, json);
  return json;
}

String DisplayManager_::getAppsWithIcon()
{
  DynamicJsonDocument jsonDocument(1024);
  JsonArray jsonArray = jsonDocument.to<JsonArray>();
  for (const auto &app : Apps)
  {
    JsonObject appObject = jsonArray.createNestedObject();
    appObject["name"] = app.first;

    CustomApp *customApp = getCustomAppByName(app.first);
    if (customApp != nullptr)
    {
      appObject["icon"] = customApp->iconName;
    }
  }
  String jsonString;
  serializeJson(jsonArray, jsonString);
  return jsonString;
}

void DisplayManager_::reorderApps(const String &jsonString)
{
  StaticJsonDocument<2048> jsonDocument;
  DeserializationError error = deserializeJson(jsonDocument, jsonString);
  if (error)
  {
    return;
  }

  JsonArray jsonArray = jsonDocument.as<JsonArray>();
  std::vector<std::pair<String, AppCallback>> reorderedApps;
  for (const String &appName : jsonArray)
  {
    for (const auto &app : Apps)
    {
      if (app.first == appName)
      {
        reorderedApps.push_back(app);
        break;
      }
    }
  }
  Apps = reorderedApps;
  ui->setApps(Apps);
  ui->forceResetState();
}

void DisplayManager_::setCustomAppColors(uint32_t color)
{
  for (auto it = customApps.begin(); it != customApps.end(); ++it)
  {
    CustomApp &app = it->second;
    if (!app.hasCustomColor)
    {
      app.color = color;
    }
  }
}
