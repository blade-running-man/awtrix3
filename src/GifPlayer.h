#ifndef GifPlayer_H
#define GifPlayer_H
#include <LittleFS.h>

class GifPlayer
{
  // ---- PUBLIC API ----
public:
  uint8_t currentFrame = 0;

  void setMatrix(FastLED_NeoMatrix *matrix)
  {
    mtx = matrix;
  }

  uint8_t getFrame()
  {
    return currentFrame;
  }

  int playGif(int x, int y, File *imageFile, uint32_t frame = 0)
  {
    offsetX = x;
    offsetY = y;

    if (imageFile->name() == file.name())
    {
      drawFrame();
      return lsdWidth;
    }

    currentFrame = 0;
    file = *imageFile;

    memset(FrameBuffer, 0, sizeof(FrameBuffer));
    memset(gifPalette, 0, sizeof(gifPalette));
    memset(lzwImageData, 0, sizeof(lzwImageData));
    memset(imageData, 0, sizeof(imageData));
    memset(imageDataBU, 0, sizeof(imageDataBU));
    memset(stack, 0, sizeof(stack));
    memset(suffix, 0, sizeof(suffix));
    memset(prefix, 0, sizeof(prefix));

    initGifFromFile();
    if (frame != 0)
    {
      do
      {
        drawFrame(true);
      } while (currentFrame < frame);
    }
    else
    {
      drawFrame();
    }

    return lsdWidth;
  }

  // ---- PRIVATE IMPLEMENTATION ----
private:
  // --- Constants ---
  static constexpr int kWidth = 32;
  static constexpr int kHeight = 8;
  static constexpr int kMaxPixels = kWidth * kHeight;

  static constexpr int kErrorNone = 0;
  static constexpr int kErrorBadGifFormat = 3;
  static constexpr int kErrorUnknownControlExt = 4;
  static constexpr int kErrorFinished = 5;

  static constexpr int kGifHdrSize = 6;
  static constexpr uint8_t kColorTableFlag = 0x80;
  static constexpr uint8_t kInterlaceFlag = 0x40;
  static constexpr uint8_t kTransparentFlag = 0x01;
  static constexpr int kNoTransparentIndex = -1;

  static constexpr int kDisposalNone = 0;
  static constexpr int kDisposalLeave = 1;
  static constexpr int kDisposalBackground = 2;
  static constexpr int kDisposalRestore = 3;

  static constexpr int kLzwMaxBits = 10;
  static constexpr int kLzwTableSize = (1 << kLzwMaxBits);

  const uint16_t kMask[17] = {
      0x0000, 0x0001, 0x0003, 0x0007,
      0x000F, 0x001F, 0x003F, 0x007F,
      0x00FF, 0x01FF, 0x03FF, 0x07FF,
      0x0FFF, 0x1FFF, 0x3FFF, 0x7FFF,
      0xFFFF};

  // --- Types ---
  struct RGB
  {
    byte Red;
    byte Green;
    byte Blue;
  };

  // --- Display state ---
  FastLED_NeoMatrix *mtx = nullptr;
  int offsetX = 0;
  int offsetY = 0;

  // --- GIF header state ---
  int lsdWidth = 0;
  int lsdHeight = 0;
  int lsdPackedField = 0;
  int lsdBackgroundIndex = 0;

  // --- Frame timing ---
  unsigned long lastFrameTime = 0;
  int newframeDelay = 0;
  int frameDelay = 0;

  // --- Frame decode state ---
  CRGB FrameBuffer[kHeight][kWidth];
  RGB gifPalette[256];
  byte imageData[kMaxPixels];
  byte imageDataBU[kMaxPixels];
  int transparentColorIndex = kNoTransparentIndex;
  int prevBackgroundIndex = 0;
  int prevDisposalMethod = kDisposalNone;
  int disposalMethod = kDisposalNone;
  boolean keyFrame = true;
  int colorCount = 0;
  int rectX = 0;
  int rectY = 0;
  int rectWidth = 0;
  int rectHeight = 0;

  // --- Table-based image state ---
  int tbiImageX = 0;
  int tbiImageY = 0;
  int tbiWidth = 0;
  int tbiHeight = 0;
  int tbiPackedBits = 0;
  boolean tbiInterlaced = false;

  // --- LZW decoder state ---
  byte lzwImageData[1280];
  int lzwCodeSize = 0;
  byte *pbuf = nullptr;
  int bbits = 0;
  int bbuf = 0;
  int cursize = 0;
  int curmask = 0;
  int codesize = 0;
  int clear_code = 0;
  int end_code = 0;
  int newcodes = 0;
  int top_slot = 0;
  int slot = 0;
  int fc = 0;
  int oc = 0;
  int bs = 0;
  byte *sp = nullptr;
  byte stack[kLzwTableSize];
  byte suffix[kLzwTableSize];
  uint16_t prefix[kLzwTableSize];

  // --- File state ---
  File file;

  // --- File I/O ---
  void backUpStream(int n)
  {
    file.seek(file.position() - n, SeekSet);
  }

  int readByte()
  {
    return file.read();
  }

  int readWord()
  {
    int b0 = readByte();
    int b1 = readByte();
    return (b1 << 8) | b0;
  }

  int readIntoBuffer(void *buffer, int numberOfBytes)
  {
    return file.read(static_cast<uint8_t *>(buffer), numberOfBytes);
  }

  // --- Image data ops (with bounds checks) ---
  void fillImageDataRect(byte colorIndex, int x, int y, int width, int height)
  {
    int yEnd = min(height + y, kHeight);
    int xEnd = min(width + x, kWidth);
    for (int yy = max(y, 0); yy < yEnd; yy++)
    {
      int yOffset = yy * kWidth;
      for (int xx = max(x, 0); xx < xEnd; xx++)
      {
        imageData[yOffset + xx] = colorIndex;
      }
    }
  }

  void fillImageData(byte colorIndex)
  {
    memset(imageData, colorIndex, sizeof(imageData));
  }

  void copyImageDataRect(byte *src, byte *dst, int x, int y, int width, int height)
  {
    int yEnd = min(height + y, kHeight);
    int xEnd = min(width + x, kWidth);
    for (int yy = max(y, 0); yy < yEnd; yy++)
    {
      int yOffset = yy * kWidth;
      for (int xx = max(x, 0); xx < xEnd; xx++)
      {
        int offset = yOffset + xx;
        dst[offset] = src[offset];
      }
    }
  }

  // --- GIF structure parsing ---
  bool initGifFromFile()
  {
    if (!parseGifHeader())
      return false;
    parseLogicalScreenDescriptor();
    parseGlobalColorTable();
    return true;
  }

  bool parseGifHeader()
  {
    char buffer[10];
    readIntoBuffer(buffer, kGifHdrSize);
    return (strncmp(buffer, "GIF87a", kGifHdrSize) == 0) ||
           (strncmp(buffer, "GIF89a", kGifHdrSize) == 0);
  }

  void parseLogicalScreenDescriptor()
  {
    lsdWidth = readWord();
    lsdHeight = readWord();
    if (lsdWidth > kWidth)
      lsdWidth = kWidth;
    if (lsdHeight > kHeight)
      lsdHeight = kHeight;
    lsdPackedField = readByte();
    lsdBackgroundIndex = readByte();
    readByte(); // aspect ratio (unused)
  }

  void parseGlobalColorTable()
  {
    if (lsdPackedField & kColorTableFlag)
    {
      colorCount = 1 << ((lsdPackedField & 7) + 1);
      int colorTableBytes = sizeof(RGB) * colorCount;
      readIntoBuffer(gifPalette, colorTableBytes);
    }
  }

  void parseGraphicControlExtension()
  {
    readByte();
    int packedBits = readByte();
    frameDelay = readWord();
    transparentColorIndex = readByte();

    if ((packedBits & kTransparentFlag) == 0)
    {
      transparentColorIndex = kNoTransparentIndex;
    }
    disposalMethod = (packedBits >> 2) & 7;
    if (disposalMethod > 3)
    {
      disposalMethod = 0;
    }

    readByte(); // block end
  }

  void parsePlainTextExtension()
  {
    char tempBuffer[260];
    byte len = readByte();
    readIntoBuffer(tempBuffer, len);
    len = readByte();
    while (len != 0)
    {
      readIntoBuffer(tempBuffer, len);
      len = readByte();
    }
  }

  void parseApplicationExtension()
  {
    char tempBuffer[260];
    memset(tempBuffer, 0, sizeof(tempBuffer));
    byte len = readByte();
    readIntoBuffer(tempBuffer, len);
    len = readByte();
    while (len != 0)
    {
      readIntoBuffer(tempBuffer, len);
      len = readByte();
    }
  }

  void parseCommentExtension()
  {
    char tempBuffer[260];
    byte len = readByte();
    while (len != 0)
    {
      memset(tempBuffer, 0, sizeof(tempBuffer));
      readIntoBuffer(tempBuffer, len);
      len = readByte();
    }
  }

  // --- Frame decode and render ---
  unsigned long parseTableBasedImage()
  {
    tbiImageX = readWord();
    tbiImageY = readWord();
    tbiWidth = readWord();
    tbiHeight = readWord();
    tbiPackedBits = readByte();
    tbiInterlaced = ((tbiPackedBits & kInterlaceFlag) != 0);

    // Bounds check: clamp sub-image to matrix dimensions
    if (tbiImageX < 0)
      tbiImageX = 0;
    if (tbiImageY < 0)
      tbiImageY = 0;
    if (tbiImageX >= kWidth)
      tbiImageX = kWidth - 1;
    if (tbiImageY >= kHeight)
      tbiImageY = kHeight - 1;
    if (tbiImageX + tbiWidth > kWidth)
      tbiWidth = kWidth - tbiImageX;
    if (tbiImageY + tbiHeight > kHeight)
      tbiHeight = kHeight - tbiImageY;

    boolean localColorTable = ((tbiPackedBits & kColorTableFlag) != 0);
    if (localColorTable)
    {
      int colorBits = ((tbiPackedBits & 7) + 1);
      colorCount = 1 << colorBits;
      int colorTableBytes = sizeof(RGB) * colorCount;
      readIntoBuffer(gifPalette, colorTableBytes);
    }

    if (keyFrame)
    {
      if (transparentColorIndex == kNoTransparentIndex)
      {
        fillImageData(lsdBackgroundIndex);
      }
      else
      {
        fillImageData(transparentColorIndex);
      }
      keyFrame = false;

      rectX = 0;
      rectY = 0;
      rectWidth = kWidth;
      rectHeight = kHeight;
    }

    if ((prevDisposalMethod != kDisposalNone) && (prevDisposalMethod != kDisposalLeave))
    {
      memset(FrameBuffer, 0, sizeof(FrameBuffer));
    }

    if (prevDisposalMethod == kDisposalBackground)
    {
      fillImageDataRect(prevBackgroundIndex, rectX, rectY, rectWidth, rectHeight);
    }
    else if (prevDisposalMethod == kDisposalRestore)
    {
      copyImageDataRect(imageDataBU, imageData, rectX, rectY, rectWidth, rectHeight);
    }
    prevDisposalMethod = disposalMethod;
    if (disposalMethod != kDisposalNone)
    {
      rectX = tbiImageX;
      rectY = tbiImageY;
      rectWidth = tbiWidth;
      rectHeight = tbiHeight;
      if (disposalMethod == kDisposalBackground)
      {
        if (transparentColorIndex != kNoTransparentIndex)
        {
          prevBackgroundIndex = transparentColorIndex;
        }
        else
        {
          prevBackgroundIndex = lsdBackgroundIndex;
        }
      }
      else if (disposalMethod == kDisposalRestore)
      {
        copyImageDataRect(imageData, imageDataBU, rectX, rectY, rectWidth, rectHeight);
      }
    }
    lzwCodeSize = readByte();
    int offset = 0;
    int dataBlockSize = readByte();
    while (dataBlockSize != 0)
    {
      backUpStream(1);
      dataBlockSize++;
      if (offset + dataBlockSize <= (int)sizeof(lzwImageData))
      {
        readIntoBuffer(lzwImageData + offset, dataBlockSize);
      }
      else
      {
        for (int i = 0; i < dataBlockSize; i++)
          file.read();
      }

      offset += dataBlockSize;
      dataBlockSize = readByte();
    }
    lzw_decode_init(lzwCodeSize, lzwImageData);
    decompressAndDisplayFrame();
    redrawLastFrame();
    transparentColorIndex = kNoTransparentIndex;
    disposalMethod = kDisposalNone;
    if (frameDelay < 1)
    {
      frameDelay = 1;
    }
    newframeDelay = frameDelay * 10;
    return frameDelay * 10;
  }

  // --- LZW decoder ---
  void lzw_decode_init(int csize, byte *buf)
  {
    pbuf = buf;
    bbuf = 0;
    bbits = 0;
    bs = 0;
    codesize = csize;
    cursize = codesize + 1;
    curmask = kMask[cursize];
    top_slot = 1 << cursize;
    clear_code = 1 << codesize;
    end_code = clear_code + 1;
    slot = newcodes = clear_code + 2;
    oc = fc = -1;
    sp = stack;
  }

  int lzw_get_code()
  {
    while (bbits < cursize)
    {
      if (!bs)
      {
        bs = *pbuf++;
      }
      bbuf |= (*pbuf++) << bbits;
      bbits += 8;
      bs--;
    }
    int c = bbuf;
    bbuf >>= cursize;
    bbits -= cursize;
    return c & curmask;
  }

  int lzw_decode(byte *buf, int len)
  {
    int l, c, code;

    if (end_code < 0)
    {
      return 0;
    }
    l = len;

    for (;;)
    {
      while (sp > stack)
      {
        *buf++ = *(--sp);
        if ((--l) == 0)
        {
          return len - l;
        }
      }
      c = lzw_get_code();
      if (c == end_code)
      {
        break;
      }
      else if (c == clear_code)
      {
        cursize = codesize + 1;
        curmask = kMask[cursize];
        slot = newcodes;
        top_slot = 1 << cursize;
        fc = oc = -1;
      }
      else
      {
        code = c;
        if ((code == slot) && (fc >= 0))
        {
          if (sp >= stack + kLzwTableSize)
            return len - l;
          *sp++ = fc;
          code = oc;
        }
        else if (code >= slot)
        {
          break;
        }
        while (code >= newcodes)
        {
          if (sp >= stack + kLzwTableSize)
            return len - l;
          if (code < 0 || code >= kLzwTableSize)
            return len - l;
          *sp++ = suffix[code];
          code = prefix[code];
        }
        if (sp >= stack + kLzwTableSize)
          return len - l;
        *sp++ = code;
        if ((slot < top_slot) && (oc >= 0))
        {
          suffix[slot] = code;
          prefix[slot++] = oc;
        }
        fc = code;
        oc = c;
        if (slot >= top_slot)
        {
          if (cursize < kLzwMaxBits)
          {
            top_slot <<= 1;
            curmask = kMask[++cursize];
          }
        }
      }
    }
    end_code = -1;
    return len - l;
  }

  void redrawLastFrame()
  {
    for (int y = 0; y < lsdHeight && y < kHeight; y++)
    {
      for (int x = 0; x < lsdWidth && x < kWidth; x++)
      {
        mtx->drawPixel(x + offsetX, y + offsetY, FrameBuffer[y][x]);
      }
    }
  }

  void decompressAndDisplayFrame()
  {
    if (tbiInterlaced)
    {
      for (int line = tbiImageY; line < tbiHeight + tbiImageY && line < kHeight; line += 8)
      {
        lzw_decode(imageData + (line * kWidth) + tbiImageX, tbiWidth);
      }
      for (int line = tbiImageY + 4; line < tbiHeight + tbiImageY && line < kHeight; line += 8)
      {
        lzw_decode(imageData + (line * kWidth) + tbiImageX, tbiWidth);
      }
      for (int line = tbiImageY + 2; line < tbiHeight + tbiImageY && line < kHeight; line += 4)
      {
        lzw_decode(imageData + (line * kWidth) + tbiImageX, tbiWidth);
      }
      for (int line = tbiImageY + 1; line < tbiHeight + tbiImageY && line < kHeight; line += 2)
      {
        lzw_decode(imageData + (line * kWidth) + tbiImageX, tbiWidth);
      }
    }
    else
    {
      for (int line = tbiImageY; line < tbiHeight + tbiImageY && line < kHeight; line++)
      {
        lzw_decode(imageData + (line * kWidth) + tbiImageX, tbiWidth);
      }
    }

    for (int y = tbiImageY; y < tbiHeight + tbiImageY && y < kHeight; y++)
    {
      int yOffset = y * kWidth;
      for (int x = tbiImageX; x < tbiWidth + tbiImageX && x < kWidth; x++)
      {
        int pixel = imageData[yOffset + x];
        if (pixel != transparentColorIndex)
        {
          CRGB color;
          color.r = gifPalette[pixel].Red;
          color.g = gifPalette[pixel].Green;
          color.b = gifPalette[pixel].Blue;
          FrameBuffer[y][x] = color;
        }
        else
        {
          if (disposalMethod == kDisposalBackground)
          {
            FrameBuffer[y][x] = CRGB::Black;
          }
        }
      }
    }
    ++currentFrame;
    lastFrameTime = millis();
  }

  // Iterative (not recursive) — max 1 rewind per call
  unsigned long drawFrame(bool force = false)
  {
    if (!force)
    {
      if (millis() - lastFrameTime < (unsigned long)newframeDelay)
      {
        redrawLastFrame();
        return 0;
      }
    }

    for (int rewindAttempts = 0; rewindAttempts < 2; rewindAttempts++)
    {
      bool rewound = false;
      while (!rewound)
      {
        int b = readByte();
        if (b < 0)
        {
          return kErrorBadGifFormat;
        }
        if (b == 0x2c)
        {
          parseTableBasedImage();
          return 0;
        }
        else if (b == 0x21)
        {
          b = readByte();
          switch (b)
          {
          case 0x01:
            parsePlainTextExtension();
            break;
          case 0xf9:
            parseGraphicControlExtension();
            break;
          case 0xfe:
            parseCommentExtension();
            break;
          case 0xff:
            parseApplicationExtension();
            break;
          default:
            return kErrorUnknownControlExt;
          }
        }
        else
        {
          // End of GIF data — rewind and try once more
          rewound = true;
          file.seek(0);
          currentFrame = 0;
          initGifFromFile();
        }
      }
    }

    return kErrorBadGifFormat;
  }
};

#endif
