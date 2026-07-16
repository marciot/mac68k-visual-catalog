#pragma once

#include "stdint.h"

class ColorMap {
  private:
    static CTabHandle colors1bit;
    static CTabHandle colors4bit;
    static CTabHandle colors8bit;

    CTabPtr    _ctabPtr; // Indirect pointer, for faking a handle
    CTabHandle ctabHndl;

    uint8_t colorMap[256];

    void makeMap();

  public:

    ColorMap (int depth, CTabPtr userCTab = NULL);

    static Boolean loadDefaultTables();
    static void disposeDefaultTables();

    RGBColor getColor(uint8_t color) {
      return (*ctabHndl)->ctTable[colorMap[color]].rgb;
    }
};

class Icon {
  private:
    Icon(const Icon&);

  public:
    Ptr iconBase;
    Ptr maskBase;

    ColorMap colorMap;
    RGBColor transparentColor;

    uint32_t log2depth;
    uint32_t log2pixelPerByte;
    uint32_t pixelIndexMask;
    uint32_t pixelValueMask;

    friend class IconIterator;

  public:
    const Boolean isMono;

    Icon(Ptr iconBase, Ptr maskBase, int depth, CTabPtr userCTab = NULL);

    uint32_t getIconValue(uint32_t pixel) {
      const b = iconBase[pixel >> log2pixelPerByte];
      const index = pixel & pixelIndexMask;
      const shift = (pixelIndexMask - index) << log2depth;
      return (b >> shift) & pixelValueMask;
    }

    uint8_t getMaskValue(uint32_t pixel) {
      if (maskBase) {
        const b = maskBase[pixel >> 3u];
        const index = pixel & 7u;
        const shift = (7u - index);
        return (b >> shift) & 1u;
      } else {
        return 1u;
      }
    }

    void getColor(uint8_t iconColor, uint8_t maskColor, RGBColor &rgb);
    void getPixel(uint32_t pixel, RGBColor &rgb);
};

class IconIterator {
  private:
    IconIterator(const IconIterator&);

    Icon &icon;
    uint8_t *maskPtr;
    uint8_t *iconPtr;
    uint8_t iconBits;
    uint8_t maskBits;
    uint8_t iconBitsRemaining;
    uint8_t maskBitsRemaining;

  public:
    IconIterator (Icon &icon);
    void getNextPixel (RGBColor &rgb);
};