/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 by Daniel Eichhorn
 * Copyright (c) 2016 by Fabrice Weinberg
 * Highly modified 2023 for AWTRIX 3 by Stephan Muehl (Blueforcer)
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

#ifndef MatrixDisplayUi_h
#define MatrixDisplayUi_h

#include <Arduino.h>
#include "FastLED_NeoMatrix.h"
#include "GifPlayer.h"
#include "DisplayManager.h"

#ifndef DEBUG_MatrixDisplayUi
#define DEBUG_MatrixDisplayUi(...)
#endif

enum AnimationDirection
{
  SLIDE_UP,
  SLIDE_DOWN
};

enum TransitionType
{
  RANDOM,
  SLIDE,
  FADE,
  ZOOM,
  ROTATE,
  PIXELATE,
  CURTAIN,
  RIPPLE,
  BLINK,
  RELOAD,
  CROSSFADE
};

enum AppState
{
  IN_TRANSITION,
  FIXED
};

// Structure of the UiState
struct MatrixDisplayUiState
{
  u_int64_t lastUpdate = 0;
  long ticksSinceLastStateSwitch = 0;

  AppState appState = FIXED;
  uint8_t currentApp = 0;

  // Normal = 1, Inverse = -1;
  int8_t appTransitionDirection = 1;
  bool lastFrameShown = false;
  bool manualControl = false;

  // Custom data that can be used by the user
  void *userData = NULL;
};

typedef void (*AppCallback)(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, int16_t x, int16_t y, GifPlayer *gifPlayer);
typedef void (*OverlayCallback)(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state, GifPlayer *gifPlayer);
typedef void (*BackgroundCallback)(FastLED_NeoMatrix *matrix);

struct IndicatorConfig
{
  uint32_t color;
  bool active;
  int blinkMs;
  int fadeMs;
};

class MatrixDisplayUi
{
private:
  static constexpr int kMatrixWidth = 32;
  static constexpr int kMatrixHeight = 8;
  static constexpr int kNumPixels = kMatrixWidth * kMatrixHeight;
  static constexpr int kCenterX = kMatrixWidth / 2;
  static constexpr int kCenterY = kMatrixHeight / 2;

  FastLED_NeoMatrix *matrix;
  CRGB transitionBuffer_[kNumPixels];

  // Values for the Apps
  AnimationDirection appAnimationDirection = SLIDE_DOWN;
  int8_t lastTransitionDirection = 1;

  long ticksPerApp = 151;           // ~ 5000ms at 30 FPS
  uint16_t ticksPerTransition = 15; // ~  500ms at 30 FPS

  bool setAutoTransition = true;
  AppCallback *AppFunctions = nullptr;

  // Internally used to transition to a specific app
  int8_t nextAppNumber = -1;

  // Values for Overlays
  OverlayCallback *overlayFunctions = nullptr;
  BackgroundCallback backgroundFunction = nullptr;
  uint8_t overlayCount = 0;
  int BackgroundEffect = -1;

  // UI State
  MatrixDisplayUiState state;

  // Bookkeeping for update
  long updateInterval = 33;

  // GIF players for current and next app during transitions
  GifPlayer gif1_;
  GifPlayer gif2_;

  // Transition state
  uint8_t currentTransition_ = SLIDE;
  bool gotNewTransition_ = true;
  bool swapped_ = false;

  void drawApp();
  void drawOverlays();
  void tick();
  void resetState();
  bool isCurrentAppValid();
  void copyLedsToBuffer();
  TransitionType getRandomTransition();

  // Transitions
  void curtainTransition();
  void slideTransition();
  void fadeTransition();
  void zoomTransition();
  void rotateTransition();
  void pixelateTransition();
  void rippleTransition();
  void blinkTransition();
  void reloadTransition();
  void crossfadeTransition();

public:
  MatrixDisplayUi(FastLED_NeoMatrix *matrix);
  ~MatrixDisplayUi();

  uint32_t fadeColor(uint32_t color, uint32_t interval);
  uint8_t AppCount = 0;

  void init();

  uint8_t getnextAppNumber();
  void setTargetFPS(uint8_t fps);
  void setBackgroundEffect(int effect);

  // Automatic Control
  void enablesetAutoTransition();
  void disablesetAutoTransition();
  void setsetAutoTransitionForwards();
  void setsetAutoTransitionBackwards();
  void setTimePerApp(long time);
  void setTimePerTransition(uint16_t time);

  // Indicators (indexed)
  static constexpr int kNumIndicators = 3;
  IndicatorConfig indicators[kNumIndicators] = {
      {0xFF0000, false, 0, 0},
      {0x00FF00, false, 0, 0},
      {0x0000FF, false, 0, 0},
  };

  void setIndicatorColor(uint8_t index, uint32_t color);
  void setIndicatorState(uint8_t index, bool state);
  void setIndicatorBlink(uint8_t index, int blinkMs);
  void setIndicatorFade(uint8_t index, int fadeMs);

  // Backward-compatible wrappers
  void setIndicator1Color(uint32_t color) { setIndicatorColor(0, color); }
  void setIndicator1State(bool state) { setIndicatorState(0, state); }
  void setIndicator1Blink(int blink) { setIndicatorBlink(0, blink); }
  void setIndicator1Fade(int fade) { setIndicatorFade(0, fade); }
  void setIndicator2Color(uint32_t color) { setIndicatorColor(1, color); }
  void setIndicator2State(bool state) { setIndicatorState(1, state); }
  void setIndicator2Blink(int blink) { setIndicatorBlink(1, blink); }
  void setIndicator2Fade(int fade) { setIndicatorFade(1, fade); }
  void setIndicator3Color(uint32_t color) { setIndicatorColor(2, color); }
  void setIndicator3State(bool state) { setIndicatorState(2, state); }
  void setIndicator3Blink(int blink) { setIndicatorBlink(2, blink); }
  void setIndicator3Fade(int fade) { setIndicatorFade(2, fade); }

  void drawIndicators();

  // App settings
  void setAppAnimation(AnimationDirection dir);
  void setApps(const std::vector<std::pair<String, AppCallback>> &appPairs);

  // Overlay
  void forceResetState();
  void setOverlays(OverlayCallback *overlayFunctions, uint8_t overlayCount);
  void setBackground(BackgroundCallback backgroundfunction);

  // Manual Control
  void nextApp();
  void previousApp();
  bool switchToApp(uint8_t app);
  void transitionToApp(uint8_t app);

  // State Info
  MatrixDisplayUiState *getUiState();
  int16_t update();
};
#endif
