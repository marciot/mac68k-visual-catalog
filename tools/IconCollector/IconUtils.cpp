#include "IconUtils.h"

#include <stdio.h>
#include <set.h>

/*inline uint32_t bfextu_byte(const uint8_t *base, uint32_t offset, uint32_t width) {
    uint8_t b = base[offset >> 3];
    uint32_t bit = offset & 7;
    return (b >> (8 - bit - width)) & ((1u << width) - 1);
}
*/

Boolean ColorMap::loadDefaultTables() {
  colors1bit = GetCTable(1);
  colors4bit = GetCTable(4);
  colors8bit = GetCTable(8);
  if (colors1bit == NULL) {
    return false;
  }
  if (colors4bit == NULL) {
    return false;
  }
  if (colors8bit == NULL) {
    return false;
  }
  return true;
}

void ColorMap::disposeDefaultTables() {
  DisposeCTable(colors1bit);
  DisposeCTable(colors4bit);
  DisposeCTable(colors8bit);
}

ColorMap::ColorMap(int depth, CTabPtr userCTab) {
  if (userCTab) {
    _ctabPtr = userCTab;
    ctabHndl = &_ctabPtr;
    makeMap();
  } else {
    switch (depth) {
      case 1: ctabHndl = colors1bit; break;
      case 4: ctabHndl = colors4bit; break;
      case 8: ctabHndl = colors8bit; break;
      default:
        printf("Invalid depth for colormap\n");
        return;
    }
    makeMap();
    _ctabPtr = 0;
  }
}

void ColorMap::makeMap() {
  Boolean needMapping = false;
  for (int i = 0; i <= (*ctabHndl)->ctSize; i++) {
    if ((*ctabHndl)->ctTable[i].value) {
      needMapping = true;
      break;
    }
  }
  if (needMapping) {
    for (int i = 0; i <= (*ctabHndl)->ctSize; i++) {
      colorMap[(*ctabHndl)->ctTable[i].value % 256] = i;
    }
  } else {
    for (int i = 0; i < 256; i++) {
      colorMap[i] = i;
    }
  }
}

CTabHandle ColorMap::colors1bit = 0;
CTabHandle ColorMap::colors4bit = 0;
CTabHandle ColorMap::colors8bit = 0;

Icon::Icon(Ptr _iconBase, Ptr _maskBase, int depth, CTabPtr clutPtr) :
      iconBase(_iconBase), maskBase(_maskBase), colorMap(depth, clutPtr),
      isMono((depth == 1) && (clutPtr == NULL))
 {
      transparentColor.red   = 0x1212;
      transparentColor.green = 0x3434;
      transparentColor.blue  = 0x5656;

      switch(depth) {
        case 1: log2depth = 0; log2pixelPerByte = 3; pixelIndexMask = 7; pixelValueMask =   1; break;
        case 2: log2depth = 1; log2pixelPerByte = 2; pixelIndexMask = 3; pixelValueMask =   3; break;
        case 4: log2depth = 2; log2pixelPerByte = 1; pixelIndexMask = 1; pixelValueMask =  15; break;
        case 8: log2depth = 3; log2pixelPerByte = 0; pixelIndexMask = 0; pixelValueMask = 255; break;
      }
    }

void Icon::getPixel(uint32_t pixel, RGBColor &rgb) {
  getColor(getIconValue(pixel), getMaskValue(pixel), rgb);
}

void Icon::getColor(uint8_t iconValue, uint8_t maskValue, RGBColor &rgb) {
 if(isMono) {
    // For B&W icons a pixel is transparent only if the
    // mask *and* icon bits are both clear.
    if ((maskValue == 0) && (iconValue == 0)) {
      rgb = transparentColor;
    } else {
      const unsigned short v = iconValue ? 0 : 0xFFFF;
      rgb.red   = v;
      rgb.blue  = v;
      rgb.green = v;
    }
  }
  else if (maskValue == 0) {
    // Put a transparent pixel
    rgb = transparentColor;
  } else {
    // Put a true color pixel
    rgb = colorMap.getColor(iconValue);
  }
}

IconIterator::IconIterator (Icon &_icon) :
        icon(_icon),
        iconPtr ((uint8_t*)_icon.iconBase),
        maskPtr ((uint8_t*)_icon.maskBase),
        iconBitsRemaining(0),
        maskBitsRemaining(0) {
}

void IconIterator::getNextPixel (RGBColor &rgb) {
  // Refill the buffers
  if (maskBitsRemaining == 0) {
    maskBits = maskPtr ? *maskPtr++ : 0xFF;
    maskBitsRemaining = 8;
  }

  if (iconBitsRemaining == 0) {
    iconBits = *iconPtr++;
    iconBitsRemaining = 8;
  }

  const iconDepth = 1u << icon.log2depth;
  const iconColor = (iconBits >> (8u - iconDepth)) & icon.pixelValueMask;
  const maskColor = (maskBits >>  7u             ) & 1u;

  icon.getColor(iconColor, maskColor, rgb);

  // Advance to next pixel
  iconBits <<= iconDepth;
  maskBits <<= 1u;
  iconBitsRemaining -= iconDepth;
  maskBitsRemaining -= 1u;
}