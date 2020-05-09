 // Coded by Bodmer 10/2/18, see license in root directory.
 // This is part of the TFT_eSPI class and is associated with anti-aliased font functions
 

////////////////////////////////////////////////////////////////////////////////////////
// New anti-aliased (smoothed) font functions added below
////////////////////////////////////////////////////////////////////////////////////////



/***************************************************************************************
** Function name:           drawGlyph
** Description:             Write a character to the TFT cursor position
*************************************************************************************x*/
// Expects file to be open
bool TFT_eSPI::drawGlyph(uint16_t code)
{
  if (code < 0x20)
  {
    if (code == '\n') {
      cursor_x = 0;
      cursor_y += sf->gFont.height;
      if (cursor_y >= _pBottom) cursor_y = _pTop;
      return true;
    }
  }

  uint16_t gNum = 0;
  bool found = sf->getUnicodeIndex(code, &gNum);
  
  uint16_t fg = textcolor;
  uint16_t bg = textbgcolor;
  uint8_t xAdvance = (found) ?  sf->gxAdvance[gNum] : sf->gFont.spaceWidth + 4;



  if (textwrapX && (cursor_x + sf->gWidth[gNum] + sf->gdX[gNum] > _pRight))
  {
    cursor_y += sf->gFont.yAdvance;
    cursor_x = _pLeft;
  }
  
  if (textwrapY && ((cursor_y + sf->gFont.yAdvance) >= _pBottom))
  {
     cursor_y = _pTop;
  }

  if (cursor_x == _pLeft) 
  {
    cursor_x -= sf->gdX[gNum];
  }

  if (cursor_x + sf->gWidth[gNum] + sf->gdX[gNum] > _pRight) {
    return false;
  }

  uint16_t bitmapSize = sf->gFont.yAdvance * xAdvance;
  uint16_t  * bitmap = (uint16_t *) malloc(bitmapSize * 2);

  for (uint16_t b = 0; b < bitmapSize; b++) {
    bitmap[b] = bg;
  }

  uint16_t size = sf->gWidth[gNum] * sf->gHeight[gNum];
  uint8_t* gBuffer = nullptr;
  
    sf->fontFile.seek(sf->gBitmap[gNum], fs::SeekSet); // This is taking >30ms for a significant position shift
    gBuffer =  (uint8_t*)malloc(size);
    sf->fontFile.read(gBuffer, size);



  uint8_t pixel;

  int16_t cy = sf->gFont.maxAscent - sf->gdY[gNum];
  int16_t cx = (sf->gdX[gNum] < 0) ? 0 : sf->gdX[gNum];

  for (int y = 0; y < sf->gHeight[gNum]; y++)
  {
    for (int x = 0; x < sf->gWidth[gNum]; x++)
    {
       pixel = gBuffer[x + sf->gWidth[gNum] * y];

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

  _swapBytes = true;//dosn't work without this
  pushImage(cursor_x, cursor_y, xAdvance, sf->gFont.yAdvance, bitmap);

  cursor_x += xAdvance;
  
  if (bitmap) {
    free(bitmap);
  }
  if (gBuffer) {
    free(gBuffer);
  }
  return true;
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
