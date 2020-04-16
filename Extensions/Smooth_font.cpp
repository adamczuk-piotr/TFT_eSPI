 // Coded by Bodmer 10/2/18, see license in root directory.
 // This is part of the TFT_eSPI class and is associated with anti-aliased font functions
 

////////////////////////////////////////////////////////////////////////////////////////
// New anti-aliased (smoothed) font functions added below
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           loadFont
** Description:             loads parameters from a font vlw array in memory
*************************************************************************************x*/
void TFT_eSPI::loadFont(const uint8_t array[])
{
  loadFont("", false);
}

#ifdef FONT_FS_AVAILABLE
/***************************************************************************************
** Function name:           loadFont
** Description:             loads parameters from a font vlw file
*************************************************************************************x*/
void TFT_eSPI::loadFont(String fontName, fs::FS &ffs)
{
  loadFont(fontName, false);
}
#endif

/***************************************************************************************
** Function name:           loadFont
** Description:             loads parameters from a font vlw file
*************************************************************************************x*/
void TFT_eSPI::loadFont(String fontName, bool flash)
{
<<<<<<< HEAD
  sf->loadFont(fontName, flash);
=======
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

  if (fontLoaded) unloadFont();

#ifdef FONT_FS_AVAILABLE
  if (fontName == "") fs_font = false;
  else { fontPtr = nullptr; fs_font = true; }

  if (fs_font) {
    spiffs = flash; // true if font is in SPIFFS

    if(spiffs) fontFS = SPIFFS;

    // Avoid a crash on the ESP32 if the file does not exist
    if (fontFS.exists("/" + fontName + ".vlw") == false) {
      Serial.println("Font file " + fontName + " not found!");
      return;
    }

    fontFile = fontFS.open( "/" + fontName + ".vlw", "r");

    if(!fontFile) return;

    fontFile.seek(0, fs::SeekSet);
  }
#else
  // Avoid unused varaible warning
  fontName = fontName;
  flash = flash;
#endif

  gFont.gArray   = (const uint8_t*)fontPtr;

  gFont.gCount   = (uint16_t)readInt32(); // glyph count in file
                             readInt32(); // vlw encoder version - discard
  gFont.yAdvance = (uint16_t)readInt32(); // Font size in points, not pixels
                             readInt32(); // discard
  gFont.ascent   = (uint16_t)readInt32(); // top of "d"
  gFont.descent  = (uint16_t)readInt32(); // bottom of "p"

  // These next gFont values might be updated when the Metrics are fetched
  gFont.maxAscent  = gFont.ascent;   // Determined from metrics
  gFont.maxDescent = gFont.descent;  // Determined from metrics
  gFont.yAdvance   = gFont.ascent + gFont.descent;
  gFont.spaceWidth = gFont.yAdvance / 4;  // Guess at space width

>>>>>>> Diagnostic sketch update + others
  fontLoaded = true;
}


/***************************************************************************************
** Function name:           deleteMetrics
** Description:             Delete the old glyph metrics and free up the memory
*************************************************************************************x*/
void TFT_eSPI::unloadFont( void )
{
 fontLoaded = false;
}

/***************************************************************************************
** Function name:           drawGlyph
** Description:             Write a character to the TFT cursor position
*************************************************************************************x*/
// Expects file to be open
void TFT_eSPI::drawGlyph(uint16_t code)
{
  if (code < 0x21)
  {
    if (code == 0x20) {
      cursor_x += sf->gFont.spaceWidth;
      return;
    }

    if (code == '\n') {
      cursor_x = 0;
      cursor_y += sf->gFont.yAdvance;
      if (cursor_y >= _height) cursor_y = 0;
      return;
    }
  }

  uint16_t gNum = 0;
  bool found = sf->getUnicodeIndex(code, &gNum);
  
  uint16_t fg = textcolor;
  uint16_t bg = textbgcolor;

  if (found)
  {

    if (textwrapX && (cursor_x + sf->gWidth[gNum] + sf->gdX[gNum] > _width))
    {
      cursor_y += sf->gFont.yAdvance;
      cursor_x = 0;
    }
    if (textwrapY && ((cursor_y + sf->gFont.yAdvance) >= _height)) cursor_y = 0;
    if (cursor_x == 0) cursor_x -= sf->gdX[gNum];

    uint8_t* pbuffer = nullptr;
    const uint8_t* gPtr = (const uint8_t*) sf->gFont.gArray;

#ifdef FONT_FS_AVAILABLE
    if (sf->fs_font)
    {
      sf->fontFile.seek(sf->gBitmap[gNum], fs::SeekSet); // This is taking >30ms for a significant position shift
      pbuffer =  (uint8_t*)malloc(sf->gWidth[gNum]);
    }
#endif

    int16_t  xs = 0;
    uint32_t dl = 0;
    uint8_t pixel;

    int16_t cy = cursor_y + sf->gFont.maxAscent - sf->gdY[gNum];
    int16_t cx = cursor_x + sf->gdX[gNum];

    startWrite(); // Avoid slow ESP32 transaction overhead for every pixel

    for (int y = 0; y < sf->gHeight[gNum]; y++)
    {
#ifdef FONT_FS_AVAILABLE
      if (sf->fs_font) {
        if (sf->spiffs)
        {
          sf->fontFile.read(pbuffer, sf->gWidth[gNum]);
          //Serial.println("SPIFFS");
        }
        else
        {
          endWrite();    // Release SPI for SD card transaction
          sf->fontFile.read(pbuffer,sf-> gWidth[gNum]);
          startWrite();  // Re-start SPI for TFT transaction
          //Serial.println("Not SPIFFS");
        }
      }
#endif
      for (int x = 0; x < sf->gWidth[gNum]; x++)
      {
#ifdef FONT_FS_AVAILABLE
        if (sf->fs_font) pixel = pbuffer[x];
        else
#endif
        pixel = pgm_read_byte(gPtr + sf->gBitmap[gNum] + x + sf->gWidth[gNum] * y);

        if (pixel)
        {
          if (pixel != 0xFF)
          {
            if (dl) {
              if (dl==1) drawPixel(xs, y + cy, fg);
              else drawFastHLine( xs, y + cy, dl, fg);
              dl = 0;
            }
            if (getColor) bg = getColor(x + cx, y + cy);
            drawPixel(x + cx, y + cy, alphaBlend(pixel, fg, bg));
          }
          else
          {
            if (dl==0) xs = x + cx;
            dl++;
          }
        }
        else
        {
          if (dl) { drawFastHLine( xs, y + cy, dl, fg); dl = 0; }
        }
      }
      if (dl) { drawFastHLine( xs, y + cy, dl, fg); dl = 0; }
    }

    if (pbuffer) free(pbuffer);
    cursor_x += sf->gxAdvance[gNum];
    endWrite();
  }
  else
  {
    // Not a Unicode in font so draw a rectangle and move on cursor
    drawRect(cursor_x, cursor_y + sf->gFont.maxAscent - sf->gFont.ascent, sf->gFont.spaceWidth, sf->gFont.ascent, fg);
    cursor_x += sf->gFont.spaceWidth + 1;
  }
}

/***************************************************************************************
** Function name:           showFont
** Description:             Page through all characters in font, td ms between screens
*************************************************************************************x*/
void TFT_eSPI::showFont(uint32_t td)
{
  if( !fontLoaded || sf == nullptr) return;

  int16_t cursorX = width(); // Force start of new page to initialise cursor
  int16_t cursorY = height();// for the first character
  uint32_t timeDelay = 0;    // No delay before first page

  fillScreen(textbgcolor);
  
  for (uint16_t i = 0; i < sf->gFont.gCount; i++)
  {
    // Check if this will need a new screen
    if (cursorX + sf->gdX[i] + sf->gWidth[i] >= width())  {
      cursorX = -sf->gdX[i];

      cursorY += sf->gFont.yAdvance;
      if (cursorY + sf->gFont.maxAscent + sf->gFont.descent >= height()) {
        cursorX = -sf->gdX[i];
        cursorY = 0;
        delay(timeDelay);
        timeDelay = td;
        fillScreen(textbgcolor);
      }
    }

    setCursor(cursorX, cursorY);
    drawGlyph(sf->gUnicode[i]);
    cursorX += sf->gxAdvance[i];
    //cursorX +=  printToSprite( cursorX, cursorY, i );
    yield();
  }

  delay(timeDelay);
  fillScreen(textbgcolor);
  //fontFile.close();
}
