
#include "SmoothFont.h"
#include <FS.h>



 std::map<String, TFT_eSPI_SmoothFont *  >  TFT_eSPI_SmoothFont::Fonts;

/***************************************************************************************
** Function name:           loadFont
** Description:             loads parameters from a font vlw file
*************************************************************************************x*/
void TFT_eSPI_SmoothFont::loadFont()
{
  /*
    The vlw font format does not appear to be documented anywhere, so some reverse
    engineering has been applied!

    Header of vlw file comprises 6 uint32_t parameters (24 bytes total):
      1. The gCount (number of character glyphs)
      2. A version number (0xB = 11 for the one I am using)
      3. The font size (in points, not pixels)
      4. Deprecated mboxY parameter (typically set to 0)
      5. Ascent in pixels from baseline to top of "d"
      6. Descent in pixels from baseline to bottom of "p"

    Next are gCount sets of values for each glyph, each set comprises 7 int32t parameters (28 bytes):
      1. Glyph Unicode stored as a 32 bit value
      2. Height of bitmap bounding box
      3. Width of bitmap bounding box
      4. gxAdvance for cursor (setWidth in Processing)
      5. dY = distance from cursor baseline to top of glyph bitmap (signed value +ve = up)
      6. dX = distance from cursor to left side of glyph bitmap (signed value -ve = left)
      7. padding value, typically 0

    The bitmaps start next at 24 + (28 * gCount) bytes from the start of the file.
    Each pixel is 1 byte, an 8 bit Alpha value which represents the transparency from
    0xFF foreground colour, 0x00 background. The sketch uses a linear interpolation
    between the foreground and background RGB component colours. e.g.
        pixelRed = ((fgRed * alpha) + (bgRed * (255 - alpha))/255
    To gain a performance advantage fixed point arithmetic is used with rounding and
    division by 256 (shift right 8 bits is faster).

    After the bitmaps is:
       1 byte for font name string length (excludes null)
       a zero terminated character string giving the font name
       1 byte for Postscript name string length
       a zero/one terminated character string giving the font name
       last byte is 0 for non-anti-aliased and 1 for anti-aliased (smoothed)


    Glyph bitmap example is:
    // Cursor coordinate positions for this and next character are marked by 'C'
    // C<------- gxAdvance ------->C  gxAdvance is how far to move cursor for next glyph cursor position
    // |                           |
    // |                           |   ascent is top of "d", descent is bottom of "p"
    // +-- gdX --+             ascent
    // |         +-- gWidth--+     |   gdX is offset to left edge of glyph bitmap
    // |   +     x@.........@x  +  |   gdX may be negative e.g. italic "y" tail extending to left of
    // |   |     @@.........@@  |  |   cursor position, plot top left corner of bitmap at (cursorX + gdX)
    // |   |     @@.........@@ gdY |   gWidth and gHeight are glyph bitmap dimensions
    // |   |     .@@@.....@@@@  |  |
    // | gHeight ....@@@@@..@@  +  +    <-- baseline
    // |   |     ...........@@     |
    // |   |     ...........@@     |   gdY is the offset to the top edge of the bitmap
    // |   |     .@@.......@@. descent plot top edge of bitmap at (cursorY + yAdvance - gdY)
    // |   +     x..@@@@@@@..x     |   x marks the corner pixels of the bitmap
    // |                           |
    // +---------------------------+   yAdvance is y delta for the next line, font size or (ascent + descent)
    //                                 some fonts can overlay in y direction so may need a user adjust value

  */

  _lastLoadTime = millis();//set access time

  if (fontLoaded) return;


    String oldestFont;
    unsigned long acessTime = 0-1;
    uint8_t loadedFonts = 0;
    for (auto & f : Fonts) {
        if (f.second->loaded()) {
          loadedFonts++;
          if (f.second->lastLoadTime() < acessTime) {
            acessTime = f.second->lastLoadTime();
            oldestFont = f.first;
          }
        }
       // Serial.printf("Font: %s, loaded: %d, size: %zu, access time: %lu \n", f.first.c_str(), (f.second->loaded()) ? 1 : 0, f.second->metricsSize(), _lastLoadTime);
    }
    if (loadedFonts > 3) {
      Fonts.at(oldestFont)->unloadFont();
    }

    // Avoid a crash on the ESP32 if the file does not exist
    if (fontFS.exists(fontPath) == false) {
      Serial.println("Font file " + fontPath + " not found!");
      return;
    }

    fontFile = fontFS.open( fontPath , "r");

    if(!fontFile) return;

    fontFile.seek(0, fs::SeekSet);
  
    //Load metadata once
    if (!_metadataLoaded) {
      gFont.gCount   = (uint16_t)readInt32(); // glyph count in file
                                readInt32(); // vlw encoder version - discard
      gFont.height = (uint16_t)readInt32(); // Font size in points, not pixels
                                readInt32(); // discard
      gFont.ascent   = (uint16_t)readInt32(); // top of "d"
      gFont.descent  = (uint16_t)readInt32(); // bottom of "p"

      // These next gFont values might be updated when the Metrics are fetched
      gFont.maxAscent  = gFont.ascent;   // Determined from metrics
      gFont.maxDescent = gFont.descent;  // Determined from metrics
      gFont.yAdvance   = gFont.ascent + gFont.descent;
      gFont.spaceWidth = gFont.yAdvance / 4;  // Guess at space width

    }
  // Fetch the metrics for each glyph
  loadMetrics();
}


/***************************************************************************************
** Function name:           loadMetrics
** Description:             Get the metrics for each glyph and store in RAM
*************************************************************************************x*/
//#define SHOW_ASCENT_DESCENT
void TFT_eSPI_SmoothFont::loadMetrics(void)
{
  uint32_t headerPtr = 24;
  uint32_t bitmapPtr = headerPtr + gFont.gCount * 28;

  gUnicode  = (uint16_t*)malloc( gFont.gCount * 2); // Unicode 16 bit Basic Multilingual Plane (0-FFFF)
  gHeight   =  (uint8_t*)malloc( gFont.gCount );    // Height of glyph
  gWidth    =  (uint8_t*)malloc( gFont.gCount );    // Width of glyph
  gxAdvance =  (uint8_t*)malloc( gFont.gCount );    // xAdvance - to move x cursor
  gdY       =  (int16_t*)malloc( gFont.gCount * 2); // offset from bitmap top edge from lowest point in any character
  gdX       =   (int8_t*)malloc( gFont.gCount );    // offset for bitmap left edge relative to cursor X
  gBitmap   = (uint32_t*)malloc( gFont.gCount * 4); // seek pointer to glyph bitmap in the file


#ifdef SHOW_ASCENT_DESCENT
  Serial.print("ascent  = "); Serial.println(gFont.ascent);
  Serial.print("descent = "); Serial.println(gFont.descent);
#endif

 fontFile.seek(headerPtr, fs::SeekSet);

  uint16_t gNum = 0;

  while (gNum < gFont.gCount)
  {
    gUnicode[gNum]  = (uint16_t)readInt32(); // Unicode code point value
    gHeight[gNum]   =  (uint8_t)readInt32(); // Height of glyph
    gWidth[gNum]    =  (uint8_t)readInt32(); // Width of glyph
    gxAdvance[gNum] =  (uint8_t)readInt32(); // xAdvance - to move x cursor
    gdY[gNum]       =  (int16_t)readInt32(); // y delta from baseline
    gdX[gNum]       =   (int8_t)readInt32(); // x delta from cursor
    readInt32(); // ignored

    if(!_metadataLoaded) {
      if (gUnicode[gNum] == 0x20) {
        gFont.spaceWidth = gxAdvance[gNum];
      }
      //Serial.print("Unicode = 0x"); Serial.print(gUnicode[gNum], HEX); Serial.print(", gHeight  = "); Serial.println(gHeight[gNum]);
      //Serial.print("Unicode = 0x"); Serial.print(gUnicode[gNum], HEX); Serial.print(", gWidth  = "); Serial.println(gWidth[gNum]);
      //Serial.print("Unicode = 0x"); Serial.print(gUnicode[gNum], HEX); Serial.print(", gxAdvance  = "); Serial.println(gxAdvance[gNum]);
      //Serial.print("Unicode = 0x"); Serial.print(gUnicode[gNum], HEX); Serial.print(", gdY  = "); Serial.println(gdY[gNum]);

      // Different glyph sets have different ascent values not always based on "d", so we could get
      // the maximum glyph ascent by checking all characters. BUT this method can generate bad values
      // for non-existant glyphs, so we will reply on processing for the value and disable this code for now...
      
      if (gdY[gNum] > gFont.maxAscent)
      {
        // Try to avoid UTF coding values and characters that tend to give duff values
        if ((gUnicode[gNum] > 0x20) && (gUnicode[gNum] != 0xA0))
        {
          gFont.maxAscent   = gdY[gNum];
          #ifdef SHOW_ASCENT_DESCENT
                  Serial.print("Unicode = 0x"); Serial.print(gUnicode[gNum], HEX); Serial.print(", maxAscent  = "); Serial.println(gFont.maxAscent);
          #endif
        }
      }
      if (gdY[gNum] > gFont.height) {
        gdY[gNum] = gFont.height;
      }

      // Different glyph sets have different descent values not always based on "p", so get maximum glyph descent
      if (((int16_t)gHeight[gNum] - (int16_t)gdY[gNum]) > gFont.maxDescent)
      {
        // Avoid UTF coding values and characters that tend to give duff values
        if ((gUnicode[gNum] > 0x20) && (gUnicode[gNum] != 0xA0))
        {
          gFont.maxDescent   = gHeight[gNum] - gdY[gNum];
          #ifdef SHOW_ASCENT_DESCENT
                  Serial.print("Unicode = 0x"); Serial.print(gUnicode[gNum], HEX); Serial.print(", maxDescent = "); Serial.println(gHeight[gNum] - gdY[gNum]);
          #endif
        }
      }

      gFont.yAdvance = gFont.maxAscent + gFont.maxDescent;
    }

    gBitmap[gNum] = bitmapPtr;

    bitmapPtr += gWidth[gNum] * gHeight[gNum];

    gNum++;
    yield();
  }



  fontLoaded = true;
  _metadataLoaded = true;

}


/***************************************************************************************
** Function name:           deleteMetrics
** Description:             Delete the old glyph metrics and free up the memory
*************************************************************************************x*/
void TFT_eSPI_SmoothFont::unloadFont( void )
{
  if (gUnicode)
  {
    free(gUnicode);
    gUnicode = NULL;
  }

  if (gHeight)
  {
    free(gHeight);
    gHeight = NULL;
  }

  if (gWidth)
  {
    free(gWidth);
    gWidth = NULL;
  }

  if (gxAdvance)
  {
    free(gxAdvance);
    gxAdvance = NULL;
  }

  if (gdY)
  {
    free(gdY);
    gdY = NULL;
  }

  if (gdX)
  {
    free(gdX);
    gdX = NULL;
  }

  if (gBitmap)
  {
    free(gBitmap);
    gBitmap = NULL;
  }

  fontFile.close();
  fontLoaded = false;
}


/***************************************************************************************
** Function name:           readInt32
** Description:             Get a 32 bit integer from the font file
*************************************************************************************x*/
uint32_t TFT_eSPI_SmoothFont::readInt32(void)
{
  uint32_t val = 0;

  val |= fontFile.read() << 24;
  val |= fontFile.read() << 16;
  val |= fontFile.read() << 8;
  val |= fontFile.read();
  return val;
}


/***************************************************************************************
** Function name:           getUnicodeIndex
** Description:             Get the font file index of a Unicode character
*************************************************************************************x*/
bool TFT_eSPI_SmoothFont::getUnicodeIndex(uint16_t unicode, uint16_t *index)
{
  for (uint16_t i = 0; i < gFont.gCount; i++)
  {
    if (gUnicode[i] == unicode)
    {
      *index = i;
      return true;
    }
  }
  return false;
}
