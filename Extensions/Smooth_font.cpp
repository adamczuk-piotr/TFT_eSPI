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
  sf->loadFont(fontName, flash);
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
  if (code < 0x20)
  {
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
  uint8_t xAdvance = (found) ?  sf->gxAdvance[gNum] : sf->gFont.spaceWidth + 4;

  uint16_t bitmapSize = sf->gFont.yAdvance * xAdvance;
  uint16_t  * bitmap = (uint16_t *) malloc(bitmapSize * 2);

  for (uint16_t b = 0; b < bitmapSize; b++) {
    bitmap[b] = bg;
  }


  if (textwrapX && (cursor_x + sf->gWidth[gNum] + sf->gdX[gNum] > _width))
  {
    cursor_y += sf->gFont.yAdvance;
    cursor_x = 0;
  }
  if (textwrapY && ((cursor_y + sf->gFont.yAdvance) >= _height)) cursor_y = 0;
  if (cursor_x == 0) cursor_x -= sf->gdX[gNum];


  uint16_t size = sf->gWidth[gNum] * sf->gHeight[gNum];
  uint8_t* gBuffer = nullptr;
  const uint8_t* gPtr = (const uint8_t*) sf->gFont.gArray;
  

#ifdef FONT_FS_AVAILABLE
  if (sf->fs_font)
  {
    sf->fontFile.seek(sf->gBitmap[gNum], fs::SeekSet); // This is taking >30ms for a significant position shift
    gBuffer =  (uint8_t*)malloc(size);
    sf->fontFile.read(gBuffer, size);
  }
#endif


  uint8_t pixel;

  int16_t cy = sf->gFont.maxAscent - sf->gdY[gNum];
  int16_t cx = sf->gdX[gNum];

  for (int y = 0; y < sf->gHeight[gNum]; y++)
  {
    for (int x = 0; x < sf->gWidth[gNum]; x++)
    {
#ifdef FONT_FS_AVAILABLE
      if (sf->fs_font) pixel = gBuffer[x + sf->gWidth[gNum] * y];
      else
#endif
      pixel = pgm_read_byte(gPtr + sf->gBitmap[gNum] + x + sf->gWidth[gNum] * y);

      if (pixel)
      {
        if (pixel == 0xFF)
        {
          bitmap[((y +cy) * xAdvance)  +x + cx ] = fg;
        }
        else
        {
           bitmap[((y +cy) * xAdvance)  +x + cx ] = alphaBlend( pixel, fg, bg);

        }
      }
    }
  }

  _swapBytes = true;
  pushImage(cursor_x, cursor_y, xAdvance, sf->gFont.yAdvance, bitmap);

  cursor_x += xAdvance;
  
  if (bitmap) {
    free(bitmap);
  }
  if (gBuffer) {
    free(gBuffer);
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
