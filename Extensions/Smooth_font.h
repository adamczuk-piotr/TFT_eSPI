 // Coded by Bodmer 10/2/18, see license in root directory.
 // This is part of the TFT_eSPI class and is associated with anti-aliased font function
protected:
  TFT_eSPI_SmoothFont * sf = nullptr;
  bool     fontLoaded = false; // Flags when a anti-aliased font is loaded



public:
  void     setFont( TFT_eSPI_SmoothFont * font) {
               sf = font; 
               if (sf != nullptr ){
                 sf->loadFont();
                 fontLoaded = sf->loaded();
               }
                  
            };


  virtual bool drawGlyph(uint16_t code);

  void     showFont(uint32_t td);




