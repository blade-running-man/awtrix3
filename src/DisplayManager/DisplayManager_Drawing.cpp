#include <DisplayManager.h>
#include "DisplayManager_Internal.h"
#include "Globals.h"
#include "Functions.h"
#include <TJpg_Decoder.h>
#include <ArduinoJson.h>

// --- Free function (TJpgDec callback) ---

bool jpg_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
  uint16_t bitmapIndex = 0;
  for (uint16_t row = 0; row < h; row++)
  {
    for (uint16_t col = 0; col < w; col++)
    {
      matrix->drawPixel(x + col, y + row, bitmap[bitmapIndex++]);
    }
  }
  return 0;
}

// --- DisplayManager_ drawing methods ---

void DisplayManager_::drawJPG(uint16_t x, uint16_t y, fs::File jpgFile)
{
  TJpgDec.drawFsJpg(x, y, jpgFile);
}

void DisplayManager_::drawJPG(int32_t x, int32_t y, const uint8_t jpeg_data[], uint32_t data_size)
{
  TJpgDec.drawJpg(x, y, jpeg_data, data_size);
}

void DisplayManager_::drawBMP(int16_t x, int16_t y, const uint16_t bitmap[], int16_t w, int16_t h)
{
  matrix->drawRGBBitmap(y, x, bitmap, w, h);
}

void DisplayManager_::drawProgressBar(int16_t x, int16_t y, int progress, uint32_t pColor, uint32_t pbColor)
{
  int available_length = 32 - x;
  int leds_for_progress = (progress * available_length) / 100;
  drawLine(x, y, x + available_length - 1, y, pbColor);
  if (leds_for_progress > 0)
    drawLine(x, y, x + leds_for_progress - 1, y, pColor);
}

void DisplayManager_::drawMenuIndicator(int cur, int total, uint32_t color)
{
  int menuItemWidth = 1;
  int totalWidth = total * menuItemWidth + (total - 1);
  int leftMargin = (MATRIX_WIDTH - totalWidth) / 2;
  int pixelSpacing = 1;
  for (int i = 0; i < total; i++)
  {
    int x = leftMargin + i * (menuItemWidth + pixelSpacing);
    if (i == cur)
    {
      drawLine(x, 7, x + menuItemWidth - 1, 7, color);
    }
    else
    {
      drawLine(x, 7, x + menuItemWidth - 1, 7, 0x666666);
    }
  }
}

void DisplayManager_::drawBarChart(int16_t x, int16_t y, const int data[], byte dataSize, bool withIcon, uint32_t color, uint32_t barBG)
{
  int availableWidth = withIcon ? (32 - 9) : 32;
  int gap = 1;
  int totalGapsWidth = (dataSize - 1) * gap;
  int barWidth = (availableWidth - totalGapsWidth) / dataSize;
  int startX = withIcon ? 9 : 0;

  for (int i = 0; i < dataSize; i++)
  {
    int x1 = x + startX + i * (barWidth + gap);
    int barHeight = data[i];
    int y1 = (barHeight > 0) ? (8 - barHeight) : 8;

    if (barBG > 0)
    {
      // Draw background bar
      drawFilledRect(x1, y, barWidth, 8, barBG);
    }

    if (barHeight > 0)
    {
      drawFilledRect(x1, y1 + y, barWidth, barHeight, color);
    }
  }
}

void DisplayManager_::drawLineChart(int16_t x, int16_t y, const int data[], byte dataSize, bool withIcon, uint32_t color)
{
  int availableWidth = withIcon ? (32 - 9) : 32;
  int startX = withIcon ? 9 : 0;
  float xStep = static_cast<float>(availableWidth) / static_cast<float>(dataSize - 1);
  int lastX = x + startX;
  int lastY = y + 8 - data[0];
  for (int i = 1; i < dataSize; i++)
  {
    int x1 = x + startX + static_cast<int>(xStep * i);
    int y1 = y + 8 - data[i];
    drawLine(lastX, lastY, x1, y1, color);
    lastX = x1;
    lastY = y1;
  }
}

void DisplayManager_::processDrawInstructions(int16_t xOffset, int16_t yOffset, String &drawInstructions)
{
  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, drawInstructions);

  if (error)
  {
    Serial.println("Error parsing JSON draw instructions");
    return;
  }

  if (!doc.is<JsonArray>())
  {
    Serial.println("Invalid JSON draw instructions format");
    return;
  }

  JsonArray instructions = doc.as<JsonArray>();
  for (JsonObject instruction : instructions)
  {
    for (auto kvp : instruction)
    {
      String command = kvp.key().c_str();

      JsonArray params = kvp.value().as<JsonArray>();
      if (command == "dp")
      {
        int x = params[0].as<int>();
        int y = params[1].as<int>();
        auto color1 = params[2];
        uint32_t color = getColorFromJsonVariant(color1, TEXTCOLOR_888);
        matrix->drawPixel(x + xOffset, y + yOffset, color);
      }
      else if (command == "dl")
      {
        int x0 = params[0].as<int>();
        int y0 = params[1].as<int>();
        int x1 = params[2].as<int>();
        int y1 = params[3].as<int>();
        auto color2 = params[4];
        uint32_t color = getColorFromJsonVariant(color2, TEXTCOLOR_888);
        drawLine(x0 + xOffset, y0 + yOffset, x1 + xOffset, y1 + yOffset, color);
      }
      else if (command == "dr")
      {
        int x = params[0].as<int>();
        int y = params[1].as<int>();
        int w = params[2].as<int>();
        int h = params[3].as<int>();
        auto color3 = params[4];
        uint32_t color = getColorFromJsonVariant(color3, TEXTCOLOR_888);
        drawRect(x + xOffset, y + yOffset, w, h, color);
      }
      else if (command == "df")
      {
        int x = params[0].as<int>();
        int y = params[1].as<int>();
        int w = params[2].as<int>();
        int h = params[3].as<int>();
        auto color4 = params[4];
        uint32_t color = getColorFromJsonVariant(color4, TEXTCOLOR_888);
        drawFilledRect(x + xOffset, y + yOffset, w, h, color);
      }
      else if (command == "dc")
      {
        int x = params[0].as<int>();
        int y = params[1].as<int>();
        int r = params[2].as<int>();
        auto color5 = params[3];
        uint32_t color = getColorFromJsonVariant(color5, TEXTCOLOR_888);
        drawCircle(x + xOffset, y + yOffset, r, color);
      }
      else if (command == "dfc")
      {
        double x = params[0].as<double>();
        double y = params[1].as<double>();
        double r = params[2].as<double>();
        auto color6 = params[3];
        uint32_t color = getColorFromJsonVariant(color6, TEXTCOLOR_888);
        fillCircle(x + xOffset, y + yOffset, r, color);
      }
      else if (command == "dt")
      {
        int x = params[0].as<int>();
        int y = params[1].as<int>();
        String text = params[2].as<String>();
        auto color7 = params[3];
        uint32_t color = getColorFromJsonVariant(color7, TEXTCOLOR_888);
        setTextColor(color);
        setCursor(x + xOffset, y + yOffset + 5);
        matrixPrint(utf8ascii(text).c_str());
      }
      else if (command == "db")
      {
        int x = params[0].as<int>();
        int y = params[1].as<int>();
        int width = params[2].as<int>();
        int height = params[3].as<int>();
        std::vector<uint32_t> bitmap(width * height);
        JsonArray colorArray = params[4].as<JsonArray>();
        size_t i = 0;
        for (const auto &color : colorArray)
        {
          bitmap[i] = color.as<uint32_t>();
          i++;
        }
        size_t bitmapIndex = 0;
        for (int row = 0; row < height; ++row)
        {
          for (int col = 0; col < width; ++col)
          {
            matrix->drawPixel(x + col + xOffset, y + row + yOffset, bitmap[bitmapIndex++]);
          }
        }
      }
    }
  }
  doc.clear();
}

// ### DRAWING PRIMITIVES ###

void DisplayManager_::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color)
{
  for (int16_t i = x; i < x + w; i++)
  {
    matrix->drawPixel(i, y, color);
    matrix->drawPixel(i, y + h - 1, color);
  }
  for (int16_t i = y; i < y + h; i++)
  {
    matrix->drawPixel(x, i, color);
    matrix->drawPixel(x + w - 1, i, color);
  }
}

void DisplayManager_::drawFastVLine(int16_t x, int16_t y, int16_t h, uint32_t color)
{
  drawLine(x, y, x, y + h - 1, color);
}

void DisplayManager_::drawFilledRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color)
{
  for (int16_t i = x; i < x + w; i++)
  {
    drawFastVLine(i, y, h, color);
  }
}

void DisplayManager_::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint32_t color)
{
  int16_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int16_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int16_t err = dx + dy, e2;

  while (true)
  {
    matrix->drawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1)
      break;
    e2 = 2 * err;
    if (e2 >= dy)
    {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx)
    {
      err += dx;
      y0 += sy;
    }
  }
}

void DisplayManager_::drawRGBBitmap(int16_t x, int16_t y, const uint32_t *bitmap, int16_t w, int16_t h)
{
  for (int16_t i = 0; i < w; i++)
  {
    for (int16_t j = 0; j < h; j++)
    {
      uint32_t pixelColor = bitmap[j * w + i];
      matrix->drawPixel(x + i, y + j, pixelColor);
    }
  }
}

void DisplayManager_::drawPixel(int16_t x0, int16_t y0, uint32_t color)
{
  matrix->drawPixel(x0, y0, color);
}

void DisplayManager_::drawCircle(int16_t x0, int16_t y0, int16_t r, uint32_t color)
{
  int16_t x = r;
  int16_t y = 0;
  int16_t p = 1 - r;

  if (r == 0)
  {
    matrix->drawPixel(x0, y0, color);
    return;
  }

  matrix->drawPixel(x0 + r, y0, color);
  matrix->drawPixel(x0 - r, y0, color);
  matrix->drawPixel(x0, y0 + r, color);
  matrix->drawPixel(x0, y0 - r, color);
  matrix->drawPixel(x0 + x, y0 - y, color);
  while (x > y)
  {
    y++;

    if (p <= 0)
      p = p + 2 * y + 1;
    else
    {
      x--;
      p = p + 2 * y - 2 * x + 1;
    }

    if (x < y)
      break;

    matrix->drawPixel(x0 + x, y0 - y, color);
    matrix->drawPixel(x0 - x, y0 - y, color);
    matrix->drawPixel(x0 + x, y0 + y, color);
    matrix->drawPixel(x0 - x, y0 + y, color);

    if (x != y)
    {
      matrix->drawPixel(x0 + y, y0 - x, color);
      matrix->drawPixel(x0 - y, y0 - x, color);
      matrix->drawPixel(x0 + y, y0 + x, color);
      matrix->drawPixel(x0 - y, y0 + x, color);
    }
  }
}

void DisplayManager_::fillCircle(int16_t x0, int16_t y0, int16_t r, uint32_t color)
{
  matrix->drawPixel(x0, y0, color);
  drawLine(x0 - r, y0, x0 + r, y0, color);
  int16_t x = r;
  int16_t y = 0;
  int16_t p = 1 - r;
  while (x > y)
  {
    y++;

    if (p <= 0)
      p = p + 2 * y + 1;
    else
    {
      x--;
      p = p + 2 * y - 2 * x + 1;
    }

    if (x < y)
      break;

    drawLine(x0 - x, y0 - y, x0 + x, y0 - y, color);
    drawLine(x0 - x, y0 + y, x0 + x, y0 + y, color);

    if (x != y)
    {
      drawLine(x0 - y, y0 - x, x0 + y, y0 - x, color);
      drawLine(x0 - y, y0 + x, x0 + y, y0 + x, color);
    }
  }
}
