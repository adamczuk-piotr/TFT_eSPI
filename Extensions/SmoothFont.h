 // Coded by Bodmer 10/2/18, see license in root directory.
 // This is part of the TFT_eSPI class and is associated with anti-aliased font functions
#pragma once
#include <map>
class TFT_eSPI_SmoothFont {

 private:
  static std::map<String, TFT_eSPI_SmoothFont *  > Fonts;

  String    fontPath;
  bool      fontLoaded = false;
  bool      _metadataLoaded = false;
  unsigned long _lastLoadTime = 0L;
  void      loadMetrics(void);
  uint32_t  readInt32(void);

  
 public:
  TFT_eSPI_SmoothFont(String path) : fontPath(path){ };
  void   loadFont();

  static TFT_eSPI_SmoothFont *  const require(const String & path) {

    if (Fonts.find(path) == TFT_eSPI_SmoothFont::Fonts.end()){

      TFT_eSPI_SmoothFont::Fonts.insert({path, new TFT_eSPI_SmoothFont(path)});
    }
     auto font = TFT_eSPI_SmoothFont::Fonts.at(path);
    if (!font->metadataLoaded()) {
      font->loadFont();
    }    
    return font;
  };



  virtual ~TFT_eSPI_SmoothFont() {
    unloadFont();
  };
  // These are for the new antialiased fonts
  
  void     unloadFont( void );
  bool     getUnicodeIndex(uint16_t unicode, uint16_t *index);

  bool     loaded() {return fontLoaded;};
  bool     metadataLoaded() {return _metadataLoaded;};
  unsigned long lastLoadTime() {return _lastLoadTime;};

  size_t   metricsSize() {
      if (_metadataLoaded) {
        return gFont.gCount * 12; //Allocated size
      }
      else {
        return 0;
      }
  }

 // This is for the whole font
  typedef struct
  {
    uint16_t gCount;                 // Total number of characters
    uint16_t height;                 //from metadata
    uint16_t yAdvance;               // Line advance
    uint16_t spaceWidth;             // Width of a space character
    int16_t  ascent;                 // Height of top of 'd' above baseline, other characters may be taller
    int16_t  descent;                // Offset to bottom of 'p', other characters may have a larger descent
    uint16_t maxAscent;              // Maximum ascent found in font
    uint16_t maxDescent;             // Maximum descent found in font
  } fontMetrics;

fontMetrics gFont = { 0, 0, 0, 0, 0, 0, 0, 0 };

  // These are for the metrics for each individual glyph (so we don't need to seek this in file and waste time)
  uint16_t* gUnicode = NULL;  //UTF-16 code, the codes are searched so do not need to be sequential
  uint8_t*  gHeight = NULL;   //cheight
  uint8_t*  gWidth = NULL;    //cwidth
  uint8_t*  gxAdvance = NULL; //setWidth
  int16_t*  gdY = NULL;       //topExtent
  int8_t*   gdX = NULL;       //leftExtent
  uint32_t* gBitmap = NULL;   //file pointer to greyscale bitmap


  fs::File fontFile;
  fs::FS   &fontFS  = SPIFFS;


};
