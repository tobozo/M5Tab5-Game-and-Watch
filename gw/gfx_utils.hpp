/*\
 *
 * M5Stack Tab5 Game&Watch Emulator
 *
 * A speculation brought to you by tobozo, copyleft (c+) 2025
 *
\*/
#pragma once

#include "./gw_types.h"
#include "./usb_utils.hpp"
#include "./fs_utils.hpp"
#include "./gw_games.hpp"
#include "../assets/icons.h"


#define M5GFX_BOARD board_M5Tab5
#include <M5Unified.hpp>  // https://github.com/m5stack/M5Unified
#define tft M5.Display
#include <lgfx/v1/platforms/esp32p4/Panel_DSI.hpp>
#include "./time_utils.hpp"
#include "./ppa_utils.hpp"


struct fpsCounter_t
{
  uint32_t last_frame = 0;
  float fps = 0;
  float avgms = 0;
  float frames_count = 0;
  float ms_count = 0;
  void addFrame()
  {
    uint32_t now = millis();
    if( last_frame == 0 )
      last_frame = now;
    auto elapsed = now-last_frame;
    last_frame = millis();
    frames_count++;
    ms_count += elapsed;
    if( frames_count>0 && ms_count>=1000.0f ) {
      avgms = ms_count/frames_count;
      frames_count = 0;
      ms_count     = 0;
    }
  }
  float getFps() { return avgms!=0 ? 1000.f/avgms : 0; }
};

const char* GFX_TAG = "GW Utils";

GWBox GWFBBox(0,0,GW_SCREEN_WIDTH,GW_SCREEN_HEIGHT);

GWImage IconSaveRTC(new GWFile{nullptr,save_rtc_jpg , save_rtc_jpg_len }, GWImage::JPG, 77, 48 );
GWImage IconDebug  (new GWFile{nullptr,dgb_png      , dgb_png_len      }, GWImage::PNG, 64, 64 );
GWImage IconFPS    (new GWFile{nullptr,fps_png      , fps_png_len      }, GWImage::PNG, 64, 64 );
GWImage IconGamepad(new GWFile{nullptr,gamepad_png  , gamepad_png_len  }, GWImage::PNG, 64, 64 );
GWImage IconMute   (new GWFile{nullptr,mute_png     , mute_png_len     }, GWImage::PNG, 64, 64 );
GWImage IconPrefs  (new GWFile{nullptr,prefs_png    , prefs_png_len    }, GWImage::PNG, 64, 64 );
GWImage IconSndM   (new GWFile{nullptr,snd_minus_png, snd_minus_png_len}, GWImage::PNG, 64, 64 );
GWImage IconSndP   (new GWFile{nullptr,snd_plus_png , snd_plus_png_len }, GWImage::PNG, 64, 64 );
GWImage IconLoading(new GWFile{nullptr,hourglass_png, hourglass_png_len}, GWImage::PNG, 64, 64 );
GWImage IconPower  (new GWFile{nullptr,poweroff_png , poweroff_png_len }, GWImage::PNG, 64, 64 );
GWImage IconLightP (new GWFile{nullptr,bright_png   , bright_png_len   }, GWImage::PNG, 64, 64 );
GWImage IconLightM (new GWFile{nullptr,dim_png      , dim_png_len      }, GWImage::PNG, 64, 64 );


static fpsCounter_t fpsCounter;

// for pushSpriteTransition reveal effect
static uint16_t *vCopyBuff = nullptr;
static uint16_t *hCopyBuff = nullptr;

PPA_Sprite GWSprite(&tft);         // emulator shared framebuffer
PPA_Sprite BlendSprite(&tft);      // menu overlay, where BGSprite+FGSprite blending happens
PPA_Sprite BGSprite(&BlendSprite); // game background image, artwork + inserted frame from the game
PPA_Sprite FGSprite(&BlendSprite); // text/icons + semi opaque overlay for menu

LGFX_Sprite FPSSprite(&tft);            // text rendering for "FPS: xx"
LGFX_Sprite MenuOptionsMask(&FGSprite); // Backup mask to capture background behind optional buttons in menu overlay
LGFX_Sprite textBox(&FGSprite);         // for drop-shadow/outline text effects

static uint16_t *gw_fb = nullptr; // Game&Watch framebuffer @16bpp, will be attached to GWSprite buffer

// storage for playable games (when preloading is successful)
static std::vector<GWGame*> GWGames;

// will be overwritten by RTC saved value
static int currentGame = 0;
// will be overwritten by RTC saved value
static uint16_t volume = 8191; // 0..32767 (0x7fff)
static const uint16_t volume_increment = 2048; // ~16 levels
static const uint16_t volume_max = 0x7fff; // INT16_MAX

static uint8_t brightness = 0x80; // 0..255
static const uint8_t brightness_increment = 0x8; // ~32 levels
static const uint8_t brightness_max = 0xff; // INT8_MAX

static PPABlend* ppa_blend = nullptr;
static PPASrm*   ppa_srm   = nullptr;

// gradient colors for the volume control
RGBColor heatMapColors[] = { {0, 0xff, 0}, {0xff, 0xff, 0}, {0xff, 0x80, 0}, {0xff, 0, 0} }; // green => yellow => orange => red
// gradient helper
template <size_t palette_size>
RGBColor getHeatMapColor( int value, int minimum, int maximum, RGBColor (&colors)[palette_size]/*RGBColor *colors*//*, size_t palette_size*/ )
{
  float indexFloat = float(value-minimum) / float(maximum-minimum) * float(palette_size-1);
  int paletteIndex = int(indexFloat/1);
  float distance = indexFloat - float(paletteIndex);
  if( distance < std::numeric_limits<float>::epsilon() ) {
    return { colors[paletteIndex].r, colors[paletteIndex].g, colors[paletteIndex].b };
  } else {
    uint8_t r1 = colors[paletteIndex].r;
    uint8_t g1 = colors[paletteIndex].g;
    uint8_t b1 = colors[paletteIndex].b;
    uint8_t r2 = colors[paletteIndex+1].r;
    uint8_t g2 = colors[paletteIndex+1].g;
    uint8_t b2 = colors[paletteIndex+1].b;
    return { uint8_t(r1 + distance*float(r2-r1)), uint8_t(g1 + distance*float(g2-g1)), uint8_t(b1 + distance*float(b2-b1)) };
  }
}



// wrapper for PPA_Sprite::createSprite() with halting on fail
void allocSprite(const char* spritename, PPA_Sprite* sprite, uint32_t width, uint32_t height)
{
  assert(sprite);
  if(!sprite->createSprite(width, height)) {
    tft_error("Failed to create sprite '%s', halting", spritename);
    while(1);
  }
}


template <typename GFX>
void borderBox(GFX* dstGfx, const GWBox srcBox, uint32_t color, uint32_t border_w=1, uint32_t border_padding=0 )
{
  assert(border_w>0);
  do
  {
    int32_t xs = srcBox.x - (border_w+border_padding);
    int32_t xe = srcBox.x + srcBox.w + border_w + border_padding;

    int32_t ys = srcBox.y - (border_w+border_padding);
    int32_t ye = srcBox.y + srcBox.h + border_w + border_padding;

    dstGfx->drawRect(xs, ys, (xe-xs)+1, (ye-ys)+1, color );

    border_w--;
  } while(border_w>0);

}


// render and cache jpg image for the background
template <typename GFX>
void composeArtwork(GWGame *gamePtr, GFX* dst=nullptr, uint32_t dst_x=0, uint32_t dst_y=0, float dst_zoom=0)
{
  assert(gamePtr);
  GWImage *jpgImage = gamePtr->bgImage;
  assert(jpgImage);
  assert(jpgImage->file);
  assert(jpgImage->file->data);
  assert(jpgImage->file->len>0);
  assert(jpgImage->type == GWImage::JPG);

  if(!jpgImage->file->data || jpgImage->file->len==0)
    return;

  auto imgscale = gamePtr->jpg_zoom;

  uint32_t aligned_width  = tft.width()/imgscale;
  uint32_t aligned_height = tft.height()/imgscale;

  if( jpgImage->width>aligned_width || jpgImage->height>aligned_height ) {
    tft_error("BAD IMAGE dimensions [%d*%d]*.%2f, should not exceed [%d*%d]", jpgImage->width, jpgImage->height, imgscale, aligned_width, aligned_height);
    while(1);
  }
  [[maybe_unused]] uint32_t now = millis();
  LGFX_Sprite* sprite = new LGFX_Sprite(&tft);
  sprite->setPsram(true); // render jpg in psram
  sprite->setColorDepth(16); //
  if(! sprite->createSprite(aligned_width, aligned_height)) {
    ESP_LOGE(GFX_TAG, "Unable to create %dx%d sprite for image", aligned_width, aligned_height);
    return;
  }
  { // expand jpg and prepare file data buffer to store expanded image data as rgb565
    ESP_LOGV(GFX_TAG, "jpg sprite created [%d x %d]", sprite->width(), sprite->height());
    // draw the jpg
    auto box = gamePtr->view.outerbox;
    uint32_t offsetx = box.x/imgscale, offsety = box.y/imgscale;
    sprite->drawJpg((uint8_t*)jpgImage->file->data, jpgImage->file->len, offsetx, offsety);
    // jpg data not needed anymore, will be replaced by rgb565 data
    free( jpgImage->file->data );
    // align the memory for later use by ppa_srm
    uint32_t align = 0x40;
    uint32_t align_mask = align-1;
    uint32_t bufSize = sprite->bufferLength();
    bufSize = (bufSize+align_mask) & ~align_mask;
    jpgImage->file->data = (uint8_t*)ps_calloc(1, bufSize);//heap_caps_aligned_calloc(align, 1, bufSize, /*MALLOC_CAP_DMA | */MALLOC_CAP_SPIRAM);
    if( !jpgImage->file->data) {
      tft_error("Failed to allocate sprite memory (%d bytes) for jpgImage->file->data, halting", bufSize);
      while(1);
    }
  }

  { // compose artwork + game framebuffer
    auto box = gamePtr->view.innerbox;
    sprite->pushImageRotateZoom(box.x/imgscale, box.y/imgscale, 0, 0, 0.0, gamePtr->zoomx/imgscale, gamePtr->zoomy/imgscale, GW_SCREEN_WIDTH, GW_SCREEN_HEIGHT, gw_fb);
    // sprite->fillRect(box.x/2.0f, box.y/2.0f, GW_SCREEN_WIDTH*gamePtr->zoomx*.5f, GW_SCREEN_HEIGHT*gamePtr->zoomy*.5f, TFT_ORANGE);
  }

  { // update jpgImage->file with expanded image data, from unaligned sprite buffer to aligned image buffer

    uint16_t* dst16 = (uint16_t*)jpgImage->file->data;
    uint16_t* src16 = (uint16_t*)sprite->getBuffer();
    for(int i=0;i<sprite->bufferLength()/2;i++) {
      dst16[i] = src16[i];
    }
    // memcpy(jpgImage->file->data, sprite->getBuffer(), sprite->bufferLength());

    jpgImage->file->len = sprite->bufferLength();
    jpgImage->type = GWImage::RGB565;
    gamePtr->jpg_width  = aligned_width;
    gamePtr->jpg_height = aligned_height;
  }

  if(dst) // a destination slot was also provided, push the sprite before destruction
    sprite->pushRotateZoom(dst, dst_x, dst_y, 0.0, dst_zoom, dst_zoom);

  delete sprite;
  ESP_LOGV(GFX_TAG, "Spent %d ms rendering+zooming the background (pushRotateZoom)", millis()-now);
}


template <typename GFX>
void drawBackground(GWGame* gamePtr, GFX* dst)
{
  assert(gamePtr);
  assert(gamePtr->jpg_zoom>0);
  GWImage *jpgImage = gamePtr->bgImage;
  assert(jpgImage);
  assert(jpgImage->file);
  assert(jpgImage->file->data);
  assert(jpgImage->file->len>0);
  assert(jpgImage->type == GWImage::RGB565);
  auto src_w = gamePtr->jpg_width;
  auto src_h = gamePtr->jpg_height;
  {
    uint32_t now = millis();
    dst->pushImageRotateZoom(0, 0, 0, 0, 0.0, gamePtr->jpg_zoom, gamePtr->jpg_zoom, src_w, src_h, (uint16_t*)jpgImage->file->data);
    ESP_LOGD(GFX_TAG, "Spent %d ms zooming the background with pushImageRotateZoom(%.2f)", millis()-now, gamePtr->jpg_zoom);
  }
  // { // this breaks the blending, looks like the output of ppa srm is not a valid input for ppa blend?
  //   uint32_t now = millis();
  //   uint32_t dst_x=0, dst_y=0, src_x=0, src_y=0;
  //   float angle = 0;
  //   while(!gamePtr->ppa_fb->available());
  //   while(!gamePtr->ppa_tft->available());
  //   //while(!ppa_blend->available());
  //   //PPA_RESET_CACHE(jpgImage->file->data, jpgImage->file->len);
  //   ppa_srm->pushImageSRM(dst_x, dst_y, src_x, src_y, angle, gamePtr->jpg_zoom, gamePtr->jpg_zoom, src_w, src_h, (uint16_t*)jpgImage->file->data, true);
  //   while(!ppa_srm->available());
  //   ESP_LOGD(GFX_TAG, "Spent %d ms zooming the background with ppa_srm", millis()-now);
  //
  // }
}



template <typename GFX>
void drawLevel(GFX* dst, GWBox box, uint32_t level, uint32_t size, uint32_t increment )
{
  assert(increment>0);
  assert(size>0xff);
  int32_t dstx = box.x, dsty = box.y;
  uint32_t width = box.w, height = box.h; // NOTE: width must be a multiple of 32

  if( dst) {

    uint32_t steps = size/increment;
    RGBColor levelColor;
    uint32_t level_bar_width   = ((2*width)/(3*steps)); // multiple of 4
    uint32_t level_bar_marginx = level_bar_width/2; // multiple of 2
    uint32_t level_bar_height  = height; // multiple of 4
    uint32_t level_bar_pad_y   = level_bar_height/4;

    uint32_t real_width = (level_bar_width*steps)+(level_bar_marginx*(steps-1));
    int leftover = width-real_width;
    // Serial.printf("Drawing volume %d (leftover pixels=%d)\n", volume, leftover);
    for(int i=0;i<steps;i++) {
      uint32_t step = i*increment;
      if( level!=0 && level>=step ) {
        levelColor = getHeatMapColor(step, 0, 0x7fff, heatMapColors);
      } else {
        levelColor = RGBColor{0x80,0x80,0x80};
      }
      uint32_t x = (level_bar_width+level_bar_marginx)*i + leftover/2;
      uint32_t h = map(i, 0, steps, level_bar_pad_y, level_bar_height);
      uint32_t y = level_bar_height-h;
      dst->fillRect(dstx+x, dsty+y, level_bar_width, h, levelColor);
    }
  }
}



void drawVolumeBox()
{
  drawLevel(&FGSprite, volumeBox, volume, 0x8000, volume_increment);
  tft.setClipRect(volumeBox.x, volumeBox.y, volumeBox.w, volumeBox.h);
  FGSprite.pushSprite(&tft, 0, 0, TFT_BLACK);
  tft.clearClipRect();
}


void drawBrightnessBox()
{
  drawLevel(&FGSprite, brightnessBox, brightness, 0x100, brightness_increment);
  tft.setClipRect(brightnessBox.x, brightnessBox.y, brightnessBox.w, brightnessBox.h);
  FGSprite.pushSprite(&tft, 0, 0, TFT_BLACK);
  tft.clearClipRect();
}



void handleFps()
{
  if( show_fps ) {
    static int last_fps_int = -1;
    auto fps = fpsCounter.getFps();
    int fps_int = fps;
    if( fps_int != last_fps_int ) {
      last_fps_int = fps_int;
      BGSprite.pushSprite(&FPSSprite,0,0);
      FPSSprite.setCursor(4,4);
      FPSSprite.setTextColor(TFT_BLACK);
      FPSSprite.printf("%3d fps", fps_int );
      FPSSprite.setCursor(0,0);
      FPSSprite.setTextColor(TFT_YELLOW);
      FPSSprite.printf("%3d fps", fps_int );
      FPSSprite.pushSprite(&tft,0,0);
    }
  }
}


void debugTouchState(m5::touch_state_t state)
{
  static constexpr const char* state_name[16] =
    { "none"
    , "touch"
    , "touch_end"
    , "touch_begin"
    , "___"
    , "hold"
    , "hold_end"
    , "hold_begin"
    , "___"
    , "flick"
    , "flick_end"
    , "flick_begin"
    , "___"
    , "drag"
    , "drag_end"
    , "drag_begin"
  };
  ESP_LOGD(GFX_TAG, "TouchState: %s", state_name[state]);
}



std::string textWrap(std::string str, int location)
{
  int n = str.rfind(' ', location);
  if (n != std::string::npos) {
    str.at(n) = '\n';
  }
  return str;
}


void tft_error(const char * format, ...)
{
  va_list args;
  va_start (args, format);
  tft.clear();
  tft.setFont(&FreeSansBold24pt7b);
  tft.setTextSize(2);
  tft.setTextColor(TFT_RED);
  tft.setCursor(0, tft.height()/2);
  tft.printf(format, args);
  Serial.printf(format, args);
  Serial.println();
  va_end (args);
}




// Gets the JPEG size from the array of data passed to the function, file reference: http://www.obrador.com/essentialjpeg/headerinfo.htm
bool get_jpeg_size(unsigned char* data, unsigned int data_size, uint32_t *width, uint32_t *height)
{
  // Check for valid JPEG file (SOI Header)
  if(data[0] != 0xFF || data[1] != 0xD8 || data[2] != 0xFF || data[3] != 0xE0) {
    ESP_LOGE(GFX_TAG, "Not a valid SOI header");
    return false;
  }

  // Check for valid JPEG header (null terminated JFIF)
  if(data[6] != 'J' || data[7] != 'F' || data[8] != 'I' || data[9] != 'F' || data[10] != 0x00) { // Not a valid JFIF string
    ESP_LOGE(GFX_TAG, "Not a valid JFIF string");
    return false;
  }

  int i = 4; // position within the file

  // Retrieve the block length of the first block since the first block will not contain the size of file
  unsigned short block_length = data[i] * 256 + data[i+1];
  while(i<data_size) {
    i+=block_length;                  // Increase the file index to get to the next block
    if(i >= data_size) return false;  // Check to protect against segmentation faults
    if(data[i] != 0xFF) return false; // Check that we are truly at the start of another block
    if(data[i+1] == 0xC0) {           // 0xFFC0 is the "Start of frame" marker which contains the file size
      //The structure of the 0xFFC0 block is quite simple [0xFFC0][ushort length][uchar precision][ushort x][ushort y]
      *height = data[i+5]*256 + data[i+6];
      *width = data[i+7]*256 + data[i+8];
      return true;
    } else {
      i+=2;                              //Skip the block marker
      block_length = data[i] * 256 + data[i+1];   //Go to the next block
    }
  }
  ESP_LOGE(GFX_TAG, "Size not found");
  return false;
}


void gzProgressCb(uint8_t progress)
{
  static uint8_t last_progress = 0xff;
  if( last_progress != progress ) {
    auto color = progress==0 ? TFT_BLACK : TFT_WHITE;
    uint32_t width = progress==0 ? 140 : float(progress)*1.4f;
    tft.fillRect(570, 462, width, 10, color);
    last_progress = progress;
  }
}


template <typename GFXIn, typename GFXout>
void pushSpriteTransition( GFXout* dst, GFXIn* src, int direction=0)
{
  uint16_t breadth = 16; // copybuff breadth

  if(!vCopyBuff)
    vCopyBuff = (uint16_t *)ps_malloc(breadth*dst->height()*sizeof(uint16_t));
  if(!hCopyBuff)
    hCopyBuff = (uint16_t *)ps_malloc(breadth*dst->width()*sizeof(uint16_t));

  if(!vCopyBuff || !hCopyBuff || src->width()%breadth!=0 || src->height()%breadth!=0) {
    src->pushSprite(dst, 0,0);
    return;
  }

  if( direction==0 ) { // top to bottom

    auto chunks = src->height()/breadth;
    for (int chunk=0,y=0;chunk<chunks;chunk++,y=chunk*breadth) {
      src->readRect(0, y, src->width(), breadth, hCopyBuff);
      dst->pushImage(0, y, src->width(), breadth, hCopyBuff);
    }

  } else if( direction > 0 ) { // right to left
    auto chunks = src->width()/breadth;
    for(int chunk=chunks-1, x=(chunks-1)*breadth;chunk>=0;chunk--,x=chunk*breadth) {
      src->readRect(x, 0, breadth, src->height(), vCopyBuff);
      dst->pushImage(x, 0, breadth, src->height(), vCopyBuff);
    }
  } else { // left to right
    auto chunks = src->width()/breadth;
    for(int chunk=0,x=0;chunk<chunks;chunk++,x=chunk*breadth) {
      src->readRect(x, 0, breadth, dst->height(), vCopyBuff);
      dst->pushImage(x, 0, breadth, dst->height(), vCopyBuff);
    }
  }

}




template <typename GFX>
void printStringAt(GFX* dst, String text, int32_t x, int32_t y)
{
  dst->setCursor(x, y);
  dst->print(text.c_str());
}



template <typename GFX>
void extrudeShadow(GFX* dst, String text, uint32_t x, uint32_t y, uint32_t text_color, uint8_t extrude_width, uint32_t extrude_color, uint8_t shadow_width, uint32_t shadow_color)
{
  if( shadow_width > 0 ) {
    // cast shadow
    dst->setTextColor(shadow_color);
    if( extrude_width == 0 ) {
      dst->drawString(text, x+shadow_width, y+shadow_width);
    } else {
      dst->drawString(text, x+shadow_width-extrude_width, y+shadow_width);
      dst->drawString(text, x+shadow_width+extrude_width, y+shadow_width);
      dst->drawString(text, x+shadow_width, y+shadow_width-extrude_width);
      dst->drawString(text, x+shadow_width, y+shadow_width+extrude_width);
    }
  }

  if( extrude_width > 0 ) {
    // draw extrusion
    dst->setTextColor(extrude_color);
    dst->drawString(text, x-extrude_width, y);
    dst->drawString(text, x+extrude_width, y);
    dst->drawString(text, x, y+extrude_width);
    dst->drawString(text, x, y-extrude_width);
  }
  // draw text
  dst->setTextColor(text_color);
  dst->drawString(text, x, y);
}


void debugPrintButton(GWTouchButton* btn, const char*msg="Drawing")
{
  if( btn )
    ESP_LOGD(GFX_TAG, "%s button %s [%d:%d] [%dx%d] -> %d (keycode=%d)", msg, btn->name, int(btn->xs), int(btn->ys), int(btn->xe-btn->xs), int(btn->ye-btn->ys), btn->hw_val, btn->keyCode);
}


template <typename GFX>
void drawBounds(GFX* dst, GWTouchButton *btn)
{ // for debug
  if(debug_buttons) {
    debugPrintButton(btn);
  }
  dst->drawRect(btn->xs, btn->ys, btn->xe-btn->xs, btn->ye-btn->ys, TFT_RED);
}


template <typename GFX>
void textAt(GFX*dst, const char* str, int32_t x, int32_t y, float fontSizeX=1.0, float fontSizeY=1.0, lgfx::textdatum_t datum=TL_DATUM)
{
  int extrude_width = 4;
  int shadow_width = 10;
  int cbwidth = extrude_width+shadow_width;
  uint32_t mask_color    = 0x303030u;
  uint32_t text_color    = 0xfbb11eu;
  uint32_t extrude_color = 0x101010u;
  uint32_t shadow_color  = 0x404040u;
  int32_t posx;

  textBox.fillSprite(mask_color);
  textBox.setTextSize(fontSizeX, fontSizeY);
  textBox.setTextDatum(datum);

  switch(datum) {
    case TL_DATUM:
      posx = cbwidth;
    break;
    case TC_DATUM:
      posx = textBox.width()/2;
    break;
    case TR_DATUM:
      posx = textBox.width()-cbwidth;
    break;

    default:
      // unsupported
      return;
  }

  if(textBox.textWidth(str)+cbwidth>textBox.width()) {
    std::string wrapblah = std::string(str); // textWrap(str,15);
    int n = wrapblah.rfind(' ', 15);
    if (n != std::string::npos) {
      std::string str1 = wrapblah.substr(0, n);
      std::string str2 = wrapblah.substr(n+1, wrapblah.size()-1);
      extrudeShadow(&textBox, str1.c_str(), posx, 0,                    text_color, extrude_width, extrude_color, shadow_width, shadow_color);
      extrudeShadow(&textBox, str2.c_str(), posx, textBox.fontHeight(), text_color, extrude_width, extrude_color, shadow_width, shadow_color);
    } else {
      extrudeShadow(&textBox, str, posx, 0, text_color, extrude_width, extrude_color, shadow_width, shadow_color);
    }
  } else {
    extrudeShadow(&textBox, str, posx, 0, text_color, extrude_width, extrude_color, shadow_width, shadow_color);
  }
  textBox.pushSprite(dst, x,y, mask_color);
}



// helper for bootAnimation()
void createCircle(LGFX_Sprite*dst, uint32_t w, uint32_t h, int32_t c, int32_t c1, int32_t c2, int32_t r1, int32_t r2, uint32_t color)
{
  dst->setPsram(true);
  dst->setColorDepth(1);
  if(!dst->createSprite(w,h)) return;
  dst->setPaletteColor(0, 0x000000U);
  dst->setPaletteColor(1, color);
  dst->fillArc(c, c, c1, c2, r1, r2, 1);
}



void bootAnimation()
{
  for(int i=0;i<64;i++) {
    tft.fillCircle(tft.width()/2, tft.height()/2, i, TFT_WHITE);
    lgfx::delay(2);
  }
  for(int i=0;i<63;i++) {
    tft.fillCircle(tft.width()/2, tft.height()/2, i, TFT_BLACK);
    lgfx::delay(2);
  }

  uint32_t w=200, h=200;
  uint32_t c = (w/2);//-1;

  LGFX_Sprite circles(&tft);

  LGFX_Sprite circle5(&circles);
  LGFX_Sprite circle6(&circles);

  createCircle(&circle5, w, h, c, c-50, c-49,  0, 300, 0xbbbbbbU);
  createCircle(&circle6, w, h, c, c-60, c-59, 0, 270, 0x999999U);

  circles.setPsram(true);
  circles.setColorDepth(16);
  if(!circles.createSprite(w,h)) return;
  circles.fillSprite(TFT_BLUE);
  circles.fillCircle(c, c, 32, TFT_WHITE );

  uint32_t max = 720;
  uint32_t step = 10;
  float arel = 360.0/float(max);

  for( int i=0;i<max;i+=step ) {

    int iinv = (max-1)-i;

    if( i>0 ) {
      for(int b=0;b<circles.bufferLength()/2;b++) {
        auto buf = (lgfx::swap565_t*)circles.getBuffer();
        if( buf[b] !=TFT_BLUE )
          buf[b] = TFT_BLACK;
      }
    }

    int a01 = i*arel;
    int a02 = 0;
    int a03 = iinv*arel;
    int a04 = (iinv+step)*arel;
    circles.fillArc(c, c, 12, 20, a01, a02, TFT_ORANGE );
    circles.fillArc(c, c, 24, 32, a03, a04, TFT_YELLOW );

    int a5 = (i*2)%360;
    int a6 = (i*3)%360;
    circle5.pushRotateZoom(&circles, c, c, a5, 1.0, 1.0, TFT_BLACK);
    circle6.pushRotateZoom(&circles, c, c, a6, 1.0, 1.0, TFT_BLACK);
    circles.pushSprite(&tft, tft.width()/2-c, tft.height()/2-c, TFT_BLUE);
  }

  for(float i=1.0;i>0;i-=.05) {
    circles.pushRotateZoom(&tft, tft.width()/2, tft.height()/2, 0.0, i, i);
    lgfx::delay(2);
  }

  tft.fillCircle(tft.width()/2, tft.height()/2, 64, TFT_BLACK);
}


void fsPicker()
{
  tft.clear();
  tft.fillRect(0,0,tft.width()/2,tft.height(),TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setFont(&FreeSansBold24pt7b);
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLACK);
  tft.drawString("SD Card", tft.width()*.25, tft.height()/2);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("LittleFS", tft.width()*.75, tft.height()/2);

  int32_t x, y;
  while(!tft.getTouch(&x, &y)) {

    auto rotation = tft.getRotation();
    if (M5.Imu.isEnabled()) {
      M5.delay(100); // wait for IMU to collect some data
      float val[3];
      // not sure why Imu.getAccel() is needed to get the gyro data but why not :)
      M5.Imu.getAccel(&val[0], &val[1], &val[2]);
      if( val[0]<0 && rotation==3 ) { // battery on the left hand side
        tft.setRotation(1);
        return;
      } else if( val[0]>0 && rotation==1 ) { // battery on the right hand side
        tft.setRotation(3);
        return;
      }
    }

    vTaskDelay(1);
  }
  if( x<tft.width()/2 ) { // SD_MMC
    gwFS = &SD_MMC;
  } else { // LittleFS
    gwFS = &LittleFS;
  }
  tft.clear();
  while(tft.getTouch(&x, &y))
    vTaskDelay(1);// wait for release
}



template <typename GFX>
void drawImageAt(GFX* gfx, GWImage* img, int32_t x, int32_t y, uint32_t transparent_color=0, float zoomx=1.0, float zoomy=1.0)
{
  assert(gfx);
  assert(img);
  assert(img->file);
  assert(img->file->data);
  switch(img->type)
  {
    case GWImage::JPG   : gfx->drawJpg((uint8_t*)img->file->data, img->file->len, x, y, 0, 0, 0, 0, zoomx, zoomy); break;
    case GWImage::PNG   : gfx->drawPng((uint8_t*)img->file->data, img->file->len, x, y, 0, 0, 0, 0, zoomx, zoomy); break;
    case GWImage::BMP   : gfx->drawBmp((uint8_t*)img->file->data, img->file->len, x, y, 0, 0, 0, 0, zoomx, zoomy); break;
    case GWImage::RAW   :
    case GWImage::RGB565:
      gfx->pushImageRotateZoom(x+(img->width*zoomx)/2, y+(img->height*zoomy)/2, 0, 0, 0.0, zoomx, zoomy, img->width, img->height, (uint16_t*)img->file->data, transparent_color);
    break;
  }
}


template <typename GFX>
void drawButton(GFX* gfx, GWTouchButton* btn, uint32_t w=100, uint32_t h=100, uint32_t bgcolor=0x123456u, uint32_t fgcolor=0x000000u, uint32_t transcolor=0x654321u)
{
  if( btn->xs==btn->xe || btn->ys== btn->ye )
    return; // unrenderable
  int32_t x=btn->xs, y=btn->ys;
  auto img = btn->icon;
  assert(img);
  assert(w>=img->width && h>=img->height);
  gfx->fillRoundRect(x, y, w, h, 8, bgcolor);
  uint32_t cx = (w/2-img->width/2)-1;
  uint32_t cy = (h/2-img->height/2)-6;
  drawImageAt( gfx, img, cx+x, cy+y, transcolor );

  gfx->setFont(&FreeMono9pt7b);
  gfx->setTextSize(1);
  gfx->setTextDatum(BC_DATUM);

  uint32_t text_color    = 0xfbb11eu;
  uint32_t extrude_color = 0x101010u;
  uint32_t shadow_color  = 0x404040u;

  extrudeShadow(gfx, btn->name, x-1+w/2, y+h-6, text_color, 1, extrude_color, 2, shadow_color);
}


template <typename GFX>
void highlight_box(GFX* gfx, uint32_t stroke_width = 16, uint32_t stroke_height = 8, lgfx::argb8888_t stroke_color = 0xffffff80u )
{
  gfx->fillRect(0, 0, stroke_width, gfx->height(), stroke_color);
  gfx->fillRect(stroke_width, 0, gfx->width()-stroke_width*2, stroke_height, stroke_color);
  gfx->fillRect(stroke_width, gfx->height()-stroke_height-1, gfx->width()-stroke_width*2, stroke_height, stroke_color);
  gfx->fillRect(gfx->width()-stroke_width-1, 0, stroke_width, gfx->height(), stroke_color);
}


template <typename GFX>
void invert_box(GFX* dst, LGFX_Sprite* src, GWBox box)
{
  if( dst->width()!=src->width() || dst->height()!=src->height() ) {
    ESP_LOGE(GFX_TAG, "src and dst dimensions don't match!");
    return;
  }
  uint16_t* srcBuf = (uint16_t*)src->getBuffer();
  uint32_t buflen = src->bufferLength()/2;
  for(int x=box.x; x<box.x+box.w; x++) {
    for(int y=box.y; y<box.y+box.h; y++) {
      uint32_t idx = x + y*src->width();
      if( idx>buflen ) {
        ESP_LOGD(GFX_TAG, "idx(%d)>buflen(%d)", idx, buflen);
        continue;
      }

      if( srcBuf[idx] != 0 && srcBuf[idx] != 0xffff ) {
        srcBuf[idx] = 0xffff-srcBuf[idx];
      }
    }
  }
  dst->setClipRect( box.x, box.y, box.w, box.h );
  dst->pushImage( 0, 0, src->width(), src->height(), srcBuf, TFT_BLACK);
  dst->clearClipRect();
}




template <typename GFX>
void draw_menu(GFX* gfx, int transition_direction=0)
{
  assert(gfx);

 /*
  * Blending operations:
  * - Background ('BGSprite' sprite) composed in load_game() from:
  *   - Game artwork (pic of the console)
  *   - Game framebuffer (from the emulatorm, 'GWSprite' sprite)
  * - Foreground ('FGSprite' sprite) composed here from:
  *   - Semi opaque mask
  *   - Menu items (buttons, text, volume, etc)
  * - Output:
  *   - 'BlendSprite' sprite
  */

  static int lastGame = -1;
  bool update_fg = false;
  if( currentGame!=lastGame ) {
    update_fg = true;
    lastGame = currentGame;
  }

  auto GAWButton = GWGames[currentGame]->getButtonByLabel("GAW");
  auto GAWBox = GAWButton->getBox();

  uint32_t fglen16 = FGSprite.bufferLength()/2;
  uint16_t* fgbuf = (uint16_t*)FGSprite.getBuffer();

  uint32_t mmlen8  = MenuOptionsMask.bufferLength();
  uint32_t mmlen16 = mmlen8/2;
  uint16_t* mmbuf = (uint16_t*)MenuOptionsMask.getBuffer();

  uint32_t now = millis();


  if( update_fg ) {
    for(int i=0;i<fglen16;i++) {
      if(i%3<=1)
        fgbuf[i] = 0;
      else
        fgbuf[i] = 0xffff;
    }
    // undo the dim effect over the Game&Watch button (background) as it is needed to exit the menu
    FGSprite.fillRect( GAWBox.x, GAWBox.y, GAWBox.w, GAWBox.h, TFT_BLACK );
    borderBox(&FGSprite, GAWBox, 0xfbb11eu, 8, 2 );

    // refill option buttons background mask
    memcpy(mmbuf, fgbuf, mmlen8);
    // if the MenuOptions overlaps the GAW button area, undo the dim effect for that partial area
    if( GAWBox.y<MenuOptionsMask.height() ) {
      MenuOptionsMask.fillRect( GAWBox.x, GAWBox.y, GAWBox.w, GAWBox.h, TFT_BLACK );
    }
    ESP_LOGD(GFX_TAG, "mask fill time: %d ms", millis()-now);
    now = millis();
  }

  // draw volume and brightness levels
  drawLevel(&FGSprite, volumeBox, volume, 0x8000, volume_increment);
  drawLevel(&FGSprite, brightnessBox, brightness, 0x100, brightness_increment);

  if( update_fg ) {
    if( debug_buttons )
      for( int i=0;i<optionButtonsCount;i++)
        drawBounds(&FGSprite, &OptionButtons[i]);

    // draw always-visible icons
    drawButton(&FGSprite, &BtnOptionVolDec);
    drawButton(&FGSprite, &BtnOptionVolInc);
    drawButton(&FGSprite, &BtnOptionDim);
    drawButton(&FGSprite, &BtnOptionLight);
  }

  if( in_options_menu ) {
    // draw icons from options menu
    for( int i=0;i<optionButtonsCount;i++) {
      if(strcmp(OptionButtons[i].name, "Options")==0)
        break; // don't render the gear icon or any icon listed after
      if(OptionButtons[i].icon)
        drawButton(&FGSprite, &OptionButtons[i] );
    }
    // draw colored spot to indicate if the feature is enable or disabled
    for(int i=0;i<toggleSwitchesCount;i++) {
      uint16_t OptionColor = toggleSwitches[i].enabled() ? TFT_GREEN : TFT_RED;
      FGSprite.fillCircle(toggleSwitches[i].btn->xs+10, toggleSwitches[i].btn->ys+10, 5, OptionColor);
    }

  } else {
    // clear icons area
    for( int i=0;i<mmlen16;i++ ) {
      fgbuf[i] = mmbuf[i];
    }
  }

  drawButton(&FGSprite, &BtnOptionOpts);

  uint16_t BtnOptionColor = BtnOption.enabled() ? TFT_GREEN : TFT_RED;
  FGSprite.fillCircle(BtnOption.btn->xs+10, BtnOption.btn->ys+10, 5, BtnOptionColor);

  if( update_fg ) {

    auto gamesCount = GWGames.size();
    auto prevGame = currentGame==0 ? GWGames[gamesCount-1] : GWGames[currentGame-1];
    auto game     = GWGames[currentGame];
    auto nextGame = currentGame+1>=gamesCount ? GWGames[0] : GWGames[currentGame+1];

    uint32_t bw = 10;
    textBox.setTextSize(1);
    uint32_t th = textBox.fontHeight()*2.5;

    uint32_t y00 = FGSprite.height()-th;
    uint32_t y01 = FGSprite.height()/2-th;

    uint32_t y10 = FGSprite.height()-th;
    uint32_t y11 = FGSprite.height()/2-th;

    uint32_t x0 = bw;
    uint32_t x1 = FGSprite.width()-(textBox.width()+bw+1);

    String menu = "< " + String(prevGame->name) + " | " + String(nextGame->name) + " > ";

    // previous game
    textAt(&FGSprite, prevGame->name, x0, y00, 1.0, 1.0, TL_DATUM);
    textAt(&FGSprite, "<<",           x0, y01, 1.0, 4.0, TL_DATUM);

    // next game
    textAt(&FGSprite, nextGame->name, x1, y10, 1.0, 1.0, TR_DATUM);
    textAt(&FGSprite, ">>",           x1, y11, 1.0, 4.0, TR_DATUM);

    // current game title and list-progress overlaying the game framebuffer
    uint32_t x2 = FGSprite.width()/2 - textBox.width()/2;
    uint32_t y2 = game->view.innerbox.y + (game->view.innerbox.h/2 - textBox.height()/2);
    textAt(&FGSprite, game->name, x2, y2, 1.1, 1.1, TC_DATUM);
    String pgStr = String(currentGame+1)+"/"+String(gamesCount); // progress
    textAt(&FGSprite, pgStr.c_str(), x2, y2+th, 0.8, 0.8, TC_DATUM);

    ESP_LOGD(GFX_TAG, "menu draw time: %d ms", millis()-now);
    now = millis();
  }

  ppa_blend->pushImageBlend(&FGSprite, &BGSprite, TFT_BLACK);
  while(!ppa_blend->available())
    vTaskDelay(1);

  ESP_LOGD(GFX_TAG, "blend time: %d ms", millis()-now);
  now = millis();

  pushSpriteTransition(&tft, &BlendSprite, transition_direction);

  ESP_LOGD(GFX_TAG, "transition time: %d ms", millis()-now);
}



void drawBuffer(uint8_t* data, uint32_t len)
{
  uint32_t sqsize = 20;
  tft.setFont(&FreeMonoBold12pt7b);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  for(int i=0;i<len;i++) {
    tft.setCursor(sqsize/2, i*sqsize);
    tft.printf("%d", i );
    for(int b=0;b<8;b++) {
      tft.fillRect((b+2)*sqsize, i*sqsize, sqsize, sqsize, bitRead(data[i], b)==0 ? TFT_WHITE : TFT_BLACK);
      tft.drawRect((b+2)*sqsize, i*sqsize, sqsize, sqsize, bitRead(data[i], b)==0 ? TFT_BLACK : TFT_WHITE); // inverted border
    }
  }
}


//  transparent buttons
//  {
//     LGFX_Sprite alphaSprite(&tft);
//     alphaSprite.setColorDepth(32);
//     auto box = {x, y, w, h};
//     if( !alphaSprite.createSprite(box.w,box.h) )
//       return;
//     int buffer_size = box.w*box.h*2;
//     auto buffer16 = (lgfx::rgb565_t*)ps_malloc(buffer_size);
//     auto buffer32 = (lgfx::argb8888_t*)alphaSprite.getBuffer();
//     BGSprite.readRect(box.x, box.y, box.w, box.h, buffer16);
//     for(int j=0;j<box.w*box.h;j++) {
//       lgfx::rgb565_t color16 = buffer16[j];
//       lgfx::argb8888_t color32 { 0xff, color16.R8(), color16.G8(), color16.B8() };
//       buffer32[j] = color32;
//     }
//     // draw transparent stuff
//     alphaSprite.fillCircle((box.w/2)-1, (box.h/2)-1, box.w/4, (lgfx::argb8888_t)0x80808080u);
//     BGSprite.pushAlphaImage(box.x, box.y, box.w, box.h, buffer32 );
//     // alphaSprite.pushSprite(&BGSprite, box.x, box.y); // works too but slower
//     alphaSprite.deleteSprite();
//     free(buffer16);
//  }
