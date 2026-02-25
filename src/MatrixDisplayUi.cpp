/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 by Daniel Eichhorn
 * Copyright (c) 2016 by Fabrice Weinberg
 * Copyright (c) 2023 by Stephan Muehl (Blueforcer)
 * Note: This old lib for SSD1306 displays has been extremely
 * modified for AWTRIX 3 and has nothing to do with the original purposes.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#include "MatrixDisplayUi.h"
#include "effects.h"
#include "Globals.h"

MatrixDisplayUi::MatrixDisplayUi(FastLED_NeoMatrix *matrix)
{
  this->matrix = matrix;
}

MatrixDisplayUi::~MatrixDisplayUi()
{
  delete[] AppFunctions;
  AppFunctions = nullptr;
}

void MatrixDisplayUi::init()
{
  this->matrix->begin();
  this->matrix->setTextWrap(false);
  this->matrix->setBrightness(70);
  gif1_.setMatrix(this->matrix);
  gif2_.setMatrix(this->matrix);
}

void MatrixDisplayUi::setTargetFPS(uint8_t fps)
{
  float oldInterval = this->updateInterval;
  this->updateInterval = ((float)1.0 / (float)fps) * 1000;

  float changeRatio = oldInterval / (float)this->updateInterval;
  this->ticksPerTransition *= changeRatio;
}

void MatrixDisplayUi::setBackgroundEffect(int effect)
{
  this->BackgroundEffect = effect;
}

// -/------ Automatic control ------\-

void MatrixDisplayUi::enablesetAutoTransition()
{
  this->setAutoTransition = true;
}
void MatrixDisplayUi::disablesetAutoTransition()
{
  this->setAutoTransition = false;
}
void MatrixDisplayUi::setsetAutoTransitionForwards()
{
  this->state.appTransitionDirection = 1;
  this->lastTransitionDirection = 1;
}
void MatrixDisplayUi::setsetAutoTransitionBackwards()
{
  this->state.appTransitionDirection = -1;
  this->lastTransitionDirection = -1;
}
void MatrixDisplayUi::setTimePerApp(long time)
{
  this->ticksPerApp = time / updateInterval;
}
void MatrixDisplayUi::setTimePerTransition(uint16_t time)
{
  this->ticksPerTransition = (int)((float)time / (float)updateInterval);
}

// -/----- App settings -----\-
void MatrixDisplayUi::setAppAnimation(AnimationDirection dir)
{
  this->appAnimationDirection = dir;
}

void MatrixDisplayUi::setApps(const std::vector<std::pair<String, AppCallback>> &appPairs)
{
  delete[] AppFunctions;
  AppCount = appPairs.size();
  AppFunctions = new AppCallback[AppCount];
  for (size_t i = 0; i < AppCount; ++i)
  {
    AppFunctions[i] = appPairs[i].second;
  }
  this->resetState();
  DisplayManager.sendAppLoop();
  DisplayManager.setAutoTransition(true);
}

// -/----- Overlays ------\-
void MatrixDisplayUi::setOverlays(OverlayCallback *overlayFunctions, uint8_t overlayCount)
{
  this->overlayFunctions = overlayFunctions;
  this->overlayCount = overlayCount;
}

void MatrixDisplayUi::setBackground(BackgroundCallback backgroundFunction)
{
  this->backgroundFunction = backgroundFunction;
}

// -/----- Manual control -----\-
void MatrixDisplayUi::nextApp()
{
  if (this->state.appState != IN_TRANSITION)
  {
    this->state.manualControl = true;
    this->state.appState = IN_TRANSITION;
    this->state.ticksSinceLastStateSwitch = 0;
    this->lastTransitionDirection = this->state.appTransitionDirection;
    this->state.appTransitionDirection = 1;
  }
}
void MatrixDisplayUi::previousApp()
{
  if (this->state.appState != IN_TRANSITION)
  {
    this->state.manualControl = true;
    this->state.appState = IN_TRANSITION;
    this->state.ticksSinceLastStateSwitch = 0;
    this->lastTransitionDirection = this->state.appTransitionDirection;
    this->state.appTransitionDirection = -1;
  }
}

bool MatrixDisplayUi::switchToApp(uint8_t app)
{
  if (app >= this->AppCount)
    return false;
  this->state.ticksSinceLastStateSwitch = 0;
  if (app == this->state.currentApp)
    return false;
  this->state.appState = FIXED;
  this->state.currentApp = app;
  return true;
}

void MatrixDisplayUi::transitionToApp(uint8_t app)
{
  if (app >= this->AppCount)
    return;
  this->state.ticksSinceLastStateSwitch = 0;
  if (app == this->state.currentApp)
    return;
  this->nextAppNumber = app;
  this->lastTransitionDirection = this->state.appTransitionDirection;
  this->state.manualControl = true;
  this->state.appState = IN_TRANSITION;
  this->state.appTransitionDirection = app < this->state.currentApp ? -1 : 1;
}

// -/----- State information -----\-
MatrixDisplayUiState *MatrixDisplayUi::getUiState()
{
  return &this->state;
}

int16_t MatrixDisplayUi::update()
{
  unsigned long appStart = millis();
  int32_t timeBudget = this->updateInterval - (appStart - this->state.lastUpdate);
  if (timeBudget <= 0)
  {
    if (this->setAutoTransition && this->state.lastUpdate != 0)
      this->state.ticksSinceLastStateSwitch += ceil((float)(-timeBudget) / this->updateInterval);

    this->state.lastUpdate = appStart;
    this->tick();
  }

  return this->updateInterval - (millis() - appStart);
}

void MatrixDisplayUi::tick()
{
  this->state.ticksSinceLastStateSwitch++;

  if (this->AppCount > 0)
  {
    switch (this->state.appState)
    {
    case IN_TRANSITION:
      if (this->state.ticksSinceLastStateSwitch >= this->ticksPerTransition)
      {
        this->state.appState = FIXED;
        this->state.currentApp = getnextAppNumber();
        this->state.ticksSinceLastStateSwitch = 0;
        this->nextAppNumber = -1;
      }
      break;
    case FIXED:
      if (this->state.manualControl)
      {
        this->state.appTransitionDirection = 1;
        this->state.manualControl = false;
      }
      if (this->state.ticksSinceLastStateSwitch >= this->ticksPerApp)
      {
        if (this->setAutoTransition)
        {
          this->state.appState = IN_TRANSITION;
        }
        this->state.ticksSinceLastStateSwitch = 0;
      }
      break;
    }
  }

  this->matrix->clear();

  if (BackgroundEffect > -1)
  {
    callEffect(this->matrix, 0, 0, BackgroundEffect);
  }

  if (this->AppCount > 0)
    this->drawApp();
  this->drawOverlays();
  this->drawIndicators();
  if (GLOBAL_OVERLAY > 0)
  {
    EffectOverlay(matrix, 0, 0, GLOBAL_OVERLAY);
  }
  DisplayManager.gammaCorrection();
  this->matrix->show();
}

// -/----- Indicators -----\-

static const int8_t kIndicatorPixels[3][3][2] = {
    {{31, 0}, {30, 0}, {31, 1}},   // indicator 1: top-right corner
    {{31, 3}, {31, 4}, {-1, -1}},  // indicator 2: right-middle (2 pixels)
    {{31, 7}, {31, 6}, {30, 7}},   // indicator 3: bottom-right corner
};

void MatrixDisplayUi::drawIndicators()
{
  unsigned long now = millis();

  for (int i = 0; i < kNumIndicators; i++)
  {
    if (!indicators[i].active)
      continue;

    uint32_t drawColor;
    if (indicators[i].blinkMs > 0)
    {
      drawColor = (now % (2 * indicators[i].blinkMs) < (unsigned long)indicators[i].blinkMs)
                      ? indicators[i].color
                      : 0;
    }
    else if (indicators[i].fadeMs > 0)
    {
      drawColor = fadeColor(indicators[i].color, indicators[i].fadeMs);
    }
    else
    {
      drawColor = indicators[i].color;
    }

    for (int p = 0; p < 3; p++)
    {
      if (kIndicatorPixels[i][p][0] >= 0)
        matrix->drawPixel(kIndicatorPixels[i][p][0], kIndicatorPixels[i][p][1], drawColor);
    }
  }
}

uint32_t MatrixDisplayUi::fadeColor(uint32_t color, uint32_t interval)
{
  if (interval < 2)
    return color;

  // Triangle wave: ramps 0->255->0 over interval ms (no floating point)
  uint32_t phase = millis() % interval;
  uint32_t halfInterval = interval / 2;
  uint8_t brightness;
  if (phase < halfInterval)
    brightness = (phase * 255) / halfInterval;
  else
    brightness = ((interval - phase) * 255) / halfInterval;

  uint8_t r = (((color >> 16) & 0xFF) * brightness) >> 8;
  uint8_t g = (((color >> 8) & 0xFF) * brightness) >> 8;
  uint8_t b = ((color & 0xFF) * brightness) >> 8;
  return (r << 16) | (g << 8) | b;
}

void MatrixDisplayUi::setIndicatorColor(uint8_t index, uint32_t color)
{
  if (index < kNumIndicators)
    indicators[index].color = color;
}

void MatrixDisplayUi::setIndicatorState(uint8_t index, bool state)
{
  if (index < kNumIndicators)
    indicators[index].active = state;
}

void MatrixDisplayUi::setIndicatorBlink(uint8_t index, int blinkMs)
{
  if (index < kNumIndicators)
    indicators[index].blinkMs = blinkMs;
}

void MatrixDisplayUi::setIndicatorFade(uint8_t index, int fadeMs)
{
  if (index < kNumIndicators)
    indicators[index].fadeMs = fadeMs;
}

// -/----- Transition helpers -----\-

TransitionType MatrixDisplayUi::getRandomTransition()
{
  return static_cast<TransitionType>(random(1, CROSSFADE + 1));
}

void MatrixDisplayUi::copyLedsToBuffer()
{
  CRGB *leds = DisplayManager.getLeds();
  for (int x = 0; x < kMatrixWidth; x++)
  {
    for (int y = 0; y < kMatrixHeight; y++)
    {
      transitionBuffer_[x + y * kMatrixWidth] = leds[this->matrix->XY(x, y)];
    }
  }
}

void MatrixDisplayUi::drawApp()
{
  switch (this->state.appState)
  {
  case IN_TRANSITION:
  {
    swapped_ = false;
    gotNewTransition_ = false;
    switch (currentTransition_)
    {
    case SLIDE:
      slideTransition();
      break;
    case FADE:
      fadeTransition();
      break;
    case ZOOM:
      zoomTransition();
      break;
    case ROTATE:
      rotateTransition();
      break;
    case PIXELATE:
      pixelateTransition();
      break;
    case CURTAIN:
      curtainTransition();
      break;
    case RIPPLE:
      rippleTransition();
      break;
    case BLINK:
      blinkTransition();
      break;
    case RELOAD:
      reloadTransition();
      break;
    case CROSSFADE:
      crossfadeTransition();
      break;
    default:
      slideTransition();
      break;
    }
    break;
  }
  case FIXED:
    if (TRANS_EFFECT == RANDOM)
    {
      if (!gotNewTransition_)
      {
        currentTransition_ = getRandomTransition();
        gotNewTransition_ = true;
      }
    }
    else
    {
      currentTransition_ = TRANS_EFFECT;
    }

    (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1_);
    swapped_ = true;
    break;
  }
}

bool MatrixDisplayUi::isCurrentAppValid()
{
  return AppFunctions != nullptr && state.currentApp < AppCount;
}

void MatrixDisplayUi::resetState()
{
  if (!isCurrentAppValid())
  {
    this->state.lastUpdate = 0;
    this->state.ticksSinceLastStateSwitch = 0;
    this->state.appState = FIXED;
    this->state.currentApp = 0;
  }
}

void MatrixDisplayUi::forceResetState()
{
  this->state.lastUpdate = 0;
  this->state.ticksSinceLastStateSwitch = 0;
  this->state.appState = FIXED;
  this->state.currentApp = 0;
}

void MatrixDisplayUi::drawOverlays()
{
  for (uint8_t i = 0; i < this->overlayCount; i++)
  {
    (this->overlayFunctions[i])(this->matrix, &this->state, &gif2_);
  }
}

uint8_t MatrixDisplayUi::getnextAppNumber()
{
  if (this->nextAppNumber != -1)
    return this->nextAppNumber;
  return (this->state.currentApp + this->AppCount + this->state.appTransitionDirection) % this->AppCount;
}

// ------------------ TRANSITIONS -------------------

static inline void rotatePoint(int &x, int &y, float cosA, float sinA, int cx, int cy)
{
  x -= cx;
  y -= cy;
  int newX = x * cosA - y * sinA;
  int newY = x * sinA + y * cosA;
  x = newX + cx;
  y = newY + cy;
}

void MatrixDisplayUi::fadeTransition()
{
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;
  int fadeValue;
  if (progress < 0.5)
  {
    fadeValue = pow(progress * 2, 2) * 255;
  }
  else
  {
    fadeValue = pow((1.0 - progress) * 2, 2) * 255;
  }
  this->matrix->clear();
  if (progress < 0.5)
  {
    (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1_);
  }
  else
  {
    (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, 0, 0, &gif2_);
  }

  CRGB *leds = DisplayManager.getLeds();
  for (int i = 0; i < kMatrixWidth; i++)
  {
    for (int j = 0; j < kMatrixHeight; j++)
    {
      int idx = this->matrix->XY(i, j);
      leds[idx].fadeToBlackBy(fadeValue);
    }
  }
}

void MatrixDisplayUi::slideTransition()
{
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;
  int16_t x = 0, y = 0, x1 = 0, y1 = 0;
  switch (this->appAnimationDirection)
  {
  case SLIDE_UP:
    x = 0;
    y = -kMatrixHeight * progress;
    x1 = 0;
    y1 = y + kMatrixHeight;
    break;
  case SLIDE_DOWN:
    x = 0;
    y = kMatrixHeight * progress;
    x1 = 0;
    y1 = y - kMatrixHeight;
    break;
  }
  int8_t dir = this->state.appTransitionDirection >= 0 ? 1 : -1;
  x *= dir;
  y *= dir;
  x1 *= dir;
  y1 *= dir;
  (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, x, y, &gif1_);
  (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, x1, y1, &gif2_);
}

void MatrixDisplayUi::curtainTransition()
{
  CRGB *leds = DisplayManager.getLeds();
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;
  int curtainWidth = (int)(kCenterX * progress);

  if (this->state.ticksSinceLastStateSwitch == 1 || this->state.ticksSinceLastStateSwitch == 0)
  {
    (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1_);
    copyLedsToBuffer();
  }
  (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, 0, 0, &gif2_);

  for (int i = 0; i < kMatrixWidth; i++)
  {
    for (int j = 0; j < kMatrixHeight; j++)
    {
      if ((i < (kCenterX - curtainWidth)) || (i >= (kCenterX + curtainWidth)))
      {
        leds[this->matrix->XY(i, j)] = transitionBuffer_[i + j * kMatrixWidth];
      }
    }
  }
}

void MatrixDisplayUi::zoomTransition()
{
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;
  float scale = 1.0;
  if (progress < 0.5)
  {
    scale = 1 - progress * 2;
    (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1_);
  }
  else
  {
    scale = (progress - 0.5) * 2;
    (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, 0, 0, &gif2_);
  }

  copyLedsToBuffer();

  CRGB *leds = DisplayManager.getLeds();
  for (int i = 0; i < kMatrixWidth; i++)
  {
    for (int j = 0; j < kMatrixHeight; j++)
    {
      int iScaled = kCenterX + (i - kCenterX) * scale;
      int jScaled = kCenterY + (j - kCenterY) * scale;

      if (iScaled < 0)
        iScaled = 0;
      if (iScaled >= kMatrixWidth)
        iScaled = kMatrixWidth - 1;
      if (jScaled < 0)
        jScaled = 0;
      if (jScaled >= kMatrixHeight)
        jScaled = kMatrixHeight - 1;
      leds[this->matrix->XY(i, j)] = transitionBuffer_[iScaled + jScaled * kMatrixWidth];
    }
  }
}

void MatrixDisplayUi::rotateTransition()
{
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;
  float angle = progress * 2 * PI;
  float cosA = cos(angle);
  float sinA = sin(angle);

  if (progress < 0.5)
  {
    (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1_);
  }
  else
  {
    (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, 0, 0, &gif2_);
  }

  copyLedsToBuffer();

  CRGB *leds = DisplayManager.getLeds();
  for (int i = 0; i < kMatrixWidth; i++)
  {
    for (int j = 0; j < kMatrixHeight; j++)
    {
      int iRotated = i;
      int jRotated = j;
      rotatePoint(iRotated, jRotated, cosA, sinA, kCenterX, kCenterY);

      if (iRotated < 0)
        iRotated = 0;
      if (iRotated >= kMatrixWidth)
        iRotated = kMatrixWidth - 1;
      if (jRotated < 0)
        jRotated = 0;
      if (jRotated >= kMatrixHeight)
        jRotated = kMatrixHeight - 1;

      leds[this->matrix->XY(i, j)] = transitionBuffer_[iRotated + jRotated * kMatrixWidth];
    }
  }
}

void MatrixDisplayUi::pixelateTransition()
{
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;

  (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1_);
  copyLedsToBuffer();

  this->matrix->clear();
  (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, 0, 0, &gif2_);

  CRGB *leds = DisplayManager.getLeds();
  for (int i = 0; i < kMatrixWidth; i++)
  {
    for (int j = 0; j < kMatrixHeight; j++)
    {
      if (random(255) > progress * 255)
      {
        leds[this->matrix->XY(i, j)] = transitionBuffer_[i + j * kMatrixWidth];
      }
    }
  }
}

void MatrixDisplayUi::rippleTransition()
{
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;

  (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1_);
  copyLedsToBuffer();

  this->matrix->clear();
  (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, 0, 0, &gif2_);

  CRGB *leds = DisplayManager.getLeds();
  for (int i = 0; i < kMatrixWidth; i++)
  {
    for (int j = 0; j < kMatrixHeight; j++)
    {
      if ((i + j) % 2 == 0 && progress < 0.5)
      {
        leds[this->matrix->XY(i, j)] = transitionBuffer_[i + j * kMatrixWidth];
      }
      else if ((i + j) % 2 != 0 && progress >= 0.5)
      {
        leds[this->matrix->XY(i, j)] = transitionBuffer_[i + j * kMatrixWidth];
      }
    }
  }
}

void MatrixDisplayUi::blinkTransition()
{
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;

  int blinks = 3;
  bool blinkState = (int)(progress * blinks) % 2 == 0;

  if (blinkState)
  {
    if (progress < 0.5)
    {
      (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1_);
    }
    else
    {
      (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, 0, 0, &gif2_);
    }
  }
  else
  {
    this->matrix->clear();
  }
}

void MatrixDisplayUi::reloadTransition()
{
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;
  int visiblePixel;

  CRGB *leds = DisplayManager.getLeds();
  if (progress < 0.5)
  {
    (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1_);

    visiblePixel = kMatrixWidth * (1.0 - (progress * 2));
    if (visiblePixel < 0)
      visiblePixel = 0;

    for (int i = visiblePixel; i < kMatrixWidth; i++)
    {
      for (int j = 0; j < kMatrixHeight; j++)
      {
        leds[this->matrix->XY(i, j)] = CRGB::Black;
      }
    }
  }
  else
  {
    (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, 0, 0, &gif2_);

    visiblePixel = kMatrixWidth * ((progress - 0.5) * 2);
    if (visiblePixel > kMatrixWidth)
      visiblePixel = kMatrixWidth;

    for (int i = visiblePixel; i < kMatrixWidth; i++)
    {
      for (int j = 0; j < kMatrixHeight; j++)
      {
        leds[this->matrix->XY(i, j)] = CRGB::Black;
      }
    }
  }
}

void MatrixDisplayUi::crossfadeTransition()
{
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;

  (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1_);
  copyLedsToBuffer();

  this->matrix->fillScreen(0);

  (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, 0, 0, &gif2_);

  CRGB *leds = DisplayManager.getLeds();
  for (int i = 0; i < kMatrixWidth; i++)
  {
    for (int j = 0; j < kMatrixHeight; j++)
    {
      int idx = this->matrix->XY(i, j);
      CRGB pixelOld = transitionBuffer_[i + j * kMatrixWidth];
      leds[idx] = pixelOld.lerp8(leds[idx], progress * 255);
    }
  }
}
