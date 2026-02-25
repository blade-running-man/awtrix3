#include <DisplayManager.h>
#include "DisplayManager_Internal.h"
#include "Globals.h"
#include "Functions.h"
#include <AwtrixFont.h>

// --- Free function ---

uint32_t interpolateColor(uint32_t color1, uint32_t color2, float t)
{
  if (t <= 0.0f)
    return color1;
  if (t >= 1.0f)
    return color2;

  uint8_t r1 = (color1 >> 16) & 0xFF; // R-Komponente aus Farbe 1
  uint8_t g1 = (color1 >> 8) & 0xFF;  // G-Komponente aus Farbe 1
  uint8_t b1 = color1 & 0xFF;         // B-Komponente aus Farbe 1

  uint8_t r2 = (color2 >> 16) & 0xFF; // R-Komponente aus Farbe 2
  uint8_t g2 = (color2 >> 8) & 0xFF;  // G-Komponente aus Farbe 2
  uint8_t b2 = color2 & 0xFF;         // B-Komponente aus Farbe 2

  // Interpolation für jede Farbkomponente
  uint8_t r_interp = r1 + (r2 - r1) * t;
  uint8_t g_interp = g1 + (g2 - g1) * t;
  uint8_t b_interp = b1 + (b2 - b1) * t;

  return (r_interp << 16) | (g_interp << 8) | b_interp;
}

// --- DisplayManager_ text methods ---

void DisplayManager_::resetTextColor()
{
  setTextColor(TEXTCOLOR_888);
}

void DisplayManager_::printText(int16_t x, int16_t y, const char *text, bool centered, byte textCase)
{

  if (centered)
  {
    uint16_t textWidth = getTextWidth(text, textCase);
    int16_t textX = ((32 - textWidth) / 2);
    setCursor(textX, y);
  }
  else
  {
    setCursor(x, y);
  }

  if ((UPPERCASE_LETTERS && textCase == 0) || textCase == 1)
  {
    size_t length = strlen(text);
    char upperText[length + 1]; // +1 for the null terminator

    for (size_t i = 0; i < length; ++i)
    {
      upperText[i] = toupper(text[i]);
    }

    upperText[length] = '\0'; // Null terminator
    matrixPrint(upperText);
  }
  else
  {
    matrixPrint(text);
  }
}

void DisplayManager_::HSVtext(int16_t x, int16_t y, const char *text, bool clear, byte textCase)
{
  if (clear)
    matrix->clear();
  static uint8_t hueOffset = 0;
  uint16_t xpos = 0;
  for (uint16_t i = 0; i < strlen(text); i++)
  {
    uint8_t hue = map(i, 0, strlen(text), 0, 360) + hueOffset;
    setTextColor(hsvToRgb(hue, 255, 255));
    const char *myChar = &text[i];

    setCursor(xpos + x, y);
    if ((UPPERCASE_LETTERS && textCase == 0) || textCase == 1)
    {
      matrixPrint((char)toupper(text[i]));
    }
    else
    {
      matrixPrint(text[i]);
    }
    char temp_str[2] = {'\0', '\0'};
    temp_str[0] = text[i];
    xpos += getTextWidth(temp_str, textCase);
  }
  hueOffset++;
  if (clear)
    matrix->show();
}

void DisplayManager_::GradientText(int16_t x, int16_t y, const char *text, int color1, int color2, bool clear, byte textCase)
{
  if (clear)
    matrix->clear();

  uint16_t xpos = 0;
  uint16_t textLength = strlen(text);

  for (uint16_t i = 0; i < textLength; i++)
  {
    // Bestimme den Interpolationswert basierend auf der aktuellen Position i im Text
    float t = (float)i / (textLength - 1);

    // Bestimme die Farbe für das aktuelle Zeichen basierend auf dem Farbverlauf
    uint32_t TC = interpolateColor(color1, color2, t);
    setTextColor(TC);

    setCursor(xpos + x, y);
    if ((UPPERCASE_LETTERS && textCase == 0) || textCase == 1)
    {
      matrixPrint((char)toupper(text[i]));
    }
    else
    {
      matrixPrint(text[i]);
    }

    char temp_str[2] = {'\0', '\0'};
    temp_str[0] = text[i];
    xpos += getTextWidth(temp_str, textCase);
  }

  if (clear)
    matrix->show();
}

void DisplayManager_::matrixPrint(char c)
{
  if (c == '\n')
  {
    cursor_y += AwtrixFont.yAdvance;
    cursor_x = 0;
    return;
  }
  else if (c == '\r')
  {
    // Handle carriage return, if needed
    return;
  }

  c -= (uint8_t)pgm_read_byte(&AwtrixFont.first);
  GFXglyph *glyph = &AwtrixFont.glyph[c];
  uint8_t *bitmap = AwtrixFont.bitmap;
  uint16_t bo = glyph->bitmapOffset;
  uint8_t w = glyph->width,
          h = glyph->height;
  int8_t xo = glyph->xOffset,
         yo = glyph->yOffset;

  uint8_t xx, yy, bits = 0, bit = 0;
  for (yy = 0; yy < h; yy++)
  {
    for (xx = 0; xx < w; xx++)
    {
      if (!(bit++ & 7))
      {
        bits = pgm_read_byte(&bitmap[bo++]);
      }
      if (bits & 0x80)
      {
        matrix->drawPixel(cursor_x + xo + xx, cursor_y + yo + yy, textColor);
      }
      bits <<= 1;
    }
  }

  cursor_x += glyph->xAdvance;
}

void DisplayManager_::matrixPrint(const char *str)
{
  while (*str)
  {
    char c = *str++;
    if (c == '\n')
    {
      cursor_y += AwtrixFont.yAdvance;
    }
    else if (c >= AwtrixFont.first && c <= AwtrixFont.last)
    {
      GFXglyph *glyph = &AwtrixFont.glyph[c - AwtrixFont.first];
      matrixPrint(c);
    }
  }
}

void DisplayManager_::matrixPrint(double number, uint8_t digits)
{
  size_t n = 0;
  String output;

  if (isnan(number))
  {
    output = "nan";
  }
  else if (isinf(number))
  {
    output = "inf";
  }
  else if (number > 4294967040.0)
  {
    output = "ovf";
  }
  else if (number < -4294967040.0)
  {
    output = "ovf";
  }
  else
  {

    if (number < 0.0)
    {
      output += '-';
      number = -number;
    }
    double rounding = 0.5;
    for (uint8_t i = 0; i < digits; ++i)
    {
      rounding /= 10.0;
    }
    number += rounding;

    unsigned long int_part = (unsigned long)number;
    output += String(int_part);

    if (digits > 0)
    {
      output += '.';
    }

    double remainder = number - (double)int_part;
    while (digits-- > 0)
    {
      remainder *= 10.0;
      int toPrint = int(remainder);
      output += String(toPrint);
      remainder -= toPrint;
    }
  }

  matrixPrint(output);
}

void DisplayManager_::matrixPrint(String str)
{
  matrixPrint(str.c_str());
}

void DisplayManager_::matrixPrint(char *str)
{
  matrixPrint(static_cast<const char *>(str));
}

void DisplayManager_::matrixPrint(char str[], size_t length)
{
  size_t Tlength = strlen(str);
  char temp[Tlength + 1]; // +1 für Nullterminator
  strncpy(temp, str, Tlength);
  temp[Tlength] = '\0'; // Nullterminator hinzufügen
  matrixPrint(temp);
}

void DisplayManager_::setCursor(int16_t x, int16_t y)
{
  cursor_x = x;
  cursor_y = y;
}

void DisplayManager_::setTextColor(uint32_t color)
{
  textColor = color;
}
