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
#include "driver/jpeg_decode.h"

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

//GWImage IconPPA    {.file={nullptr,ppa_png      , ppa_png_len      }, .type=GWImage::PNG, .width=64, .height=64 };
GWImage IconSaveRTC{.file=new GWFile{nullptr,save_rtc_jpg , save_rtc_jpg_len }, .type=GWImage::JPG, .width=77, .height=48 };
GWImage IconDebug  {.file=new GWFile{nullptr,dgb_png      , dgb_png_len      }, .type=GWImage::PNG, .width=64, .height=64 };
GWImage IconFPS    {.file=new GWFile{nullptr,fps_png      , fps_png_len      }, .type=GWImage::PNG, .width=64, .height=64 };
GWImage IconGamepad{.file=new GWFile{nullptr,gamepad_png  , gamepad_png_len  }, .type=GWImage::PNG, .width=64, .height=64 };
GWImage IconMute   {.file=new GWFile{nullptr,mute_png     , mute_png_len     }, .type=GWImage::PNG, .width=64, .height=64 };
GWImage IconPrefs  {.file=new GWFile{nullptr,prefs_png    , prefs_png_len    }, .type=GWImage::PNG, .width=64, .height=64 };
GWImage IconSndM   {.file=new GWFile{nullptr,snd_minus_png, snd_minus_png_len}, .type=GWImage::PNG, .width=64, .height=64 };
GWImage IconSndP   {.file=new GWFile{nullptr,snd_plus_png , snd_plus_png_len }, .type=GWImage::PNG, .width=64, .height=64 };
GWImage IconLoading{.file=new GWFile{nullptr,hourglass_png, hourglass_png_len}, .type=GWImage::PNG, .width=64, .height=64 };

static fpsCounter_t fpsCounter;

// for pushSpriteTransition reveal effect
static uint16_t *vCopyBuff = nullptr;
static uint16_t *hCopyBuff = nullptr;

LGFX_Sprite GWSprite(&tft); // emulator shared framebuffer
LGFX_Sprite BlendSprite(&tft); // menu overlay for BGSprite
LGFX_Sprite BGSprite(&BlendSprite); // jpeg game background holder
LGFX_Sprite FGSprite(&BlendSprite); // menu overlay for BGSprite
LGFX_Sprite textBox(&FGSprite);

//LGFX_Sprite dim(&tft);    // semi transparent overlay to dim background
//LGFX_Sprite Blah(&FGSprite);
//LGFX_Sprite Bleh(&FGSprite);

static uint16_t *gw_fb = nullptr; // Game&Watch framebuffer @16bpp, will be attached to GWSprite buffer

// storage for playable games (when preloading is successful)
static std::vector<GWGame*> GWGames;

// will be overwritten by RTC saved value
static int currentGame = 0;
// will be overwritten by RTC saved value
static uint16_t volume = 8191; // 0..32767 (0x7fff)
static const uint16_t volume_increment = 2048; // ~16 levels

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


static std::vector<uint16_t*> ptrBucket; // pointers storage for allocSprite()
// same as sprite.createSprite() but with aligned memory for ppa operations
void allocSprite(const char* spritename, LGFX_Sprite* sprite, uint32_t width, uint32_t height, uint32_t align)
{
  assert(sprite);
  if( !ppa_enabled ) { // let M5GFX choose the alignment
    sprite->setPsram(true);
    sprite->setColorDepth(16);
    if(!sprite->createSprite(width, height)) {
      tft_error("Failed to create sprite '%s', halting", spritename);
      while(1);
    }
  } else { // create aligned buffer and attach to sprite
    uint32_t align_mask = align-1;
    uint32_t bufSize = width * height * sizeof(uint16_t);
    bufSize = (bufSize+align_mask) & ~align_mask;
    uint16_t *buf = (uint16_t *)heap_caps_aligned_alloc(align, bufSize, /*MALLOC_CAP_DMA | */MALLOC_CAP_SPIRAM);
    if( !buf) {
      tft_error("Failed to allocate sprite memory for %s, halting", spritename);
      while(1);
    }
    ptrBucket.push_back(buf);
    sprite->setBuffer(buf, tft.width(), tft.height(), 16);
  }
}

// render and cache jpg image
template <typename GFX>
void pushCacheJpg(GFX* dst, int32_t x, int32_t y, GWImage *jpgImage, float zoomx, float zoomy)
{
  assert(dst);
  assert(jpgImage);
  assert(jpgImage->file);
  assert(jpgImage->file->data);
  assert(jpgImage->file->len>0);

  if(!jpgImage->file->data || jpgImage->file->len==0)
    return;

  LGFX_Sprite* sprite;

  if( jpgImage->type == GWImage::JPG ) {
    sprite = new LGFX_Sprite(dst);
    sprite->setPsram(true);
    if(! sprite->createSprite(jpgImage->width, jpgImage->height) ) {
      ESP_LOGE(GFX_TAG, "Unable to create sprite for image");
      return;
    }
    sprite->drawJpg((uint8_t*)jpgImage->file->data, jpgImage->file->len);
    free( jpgImage->file->data );
    jpgImage->file->data = (uint8_t*)sprite;
    jpgImage->file->len = sprite->bufferLength();
    jpgImage->type = GWImage::RGB565;
  } else if( jpgImage->type == GWImage::RGB565 ) {
    sprite = (LGFX_Sprite*)jpgImage->file->data;
  } else {
    ESP_LOGE(GFX_TAG, "unsupported image type");
    return;
  }

  // // if(!ppa_srm)
  // //   ppa_srm = new LGFX_PPA_SRM(dst);
  // //
  // // ppa_srm->setup({x,y,pic_info.width, pic_info.height}, zoomx, zoomy, (uint16_t*)jpgSprite.getBuffer(), false);
  // // while(!ppa_srm->available())
  // //   vTaskDelay(1);
  // // ppa_srm->exec();
  // // while(!ppa_srm->available())
  // //   vTaskDelay(1);

  sprite->pushRotateZoom(dst, x+(jpgImage->width*zoomx)/2, y+(jpgImage->height*zoomy)/2, 0.0, zoomx, zoomy);
}


template <typename GFX>
void drawVolume(GFX* gfx, uint16_t* buf=nullptr )
{
  int32_t dstx = volumeBox.x, dsty = volumeBox.y;
  uint32_t width = volumeBox.w, height = volumeBox.h; // NOTE: width must be a multiple of 32

  if( gfx) {

    uint32_t steps = 0x8000/volume_increment; // 16 steps
    RGBColor volumeColor;
    uint32_t volume_bar_width   = ((2*width)/(3*steps)); // multiple of 4
    uint32_t volume_bar_marginx = volume_bar_width/2; // multiple of 2
    uint32_t volume_bar_height  = height; // multiple of 4
    uint32_t volume_bar_pady    = volume_bar_height/4;

    uint32_t real_width = (volume_bar_width*steps)+(volume_bar_marginx*(steps-1));
    int leftover = width-real_width;
    // Serial.printf("Drawing volume %d (leftover pixels=%d)\n", volume, leftover);
    for(int i=0;i<steps;i++) {
      uint32_t step_volume = i*volume_increment;
      if( volume!=0 && volume>=step_volume ) {
        volumeColor = getHeatMapColor(step_volume, 0, 0x7fff, heatMapColors);
      } else {
        volumeColor = RGBColor{0x80,0x80,0x80};
      }
      uint32_t x = (volume_bar_width+volume_bar_marginx)*i + leftover/2;
      uint32_t h = map(i, 0, steps, volume_bar_pady, volume_bar_height);
      uint32_t y = volume_bar_height-h;
      gfx->fillRect(dstx+x, dsty+y, volume_bar_width, h, volumeColor);
    }
  }

  if( buf ) {
    tft.setClipRect(dstx, dsty, width, height);
    FGSprite.pushSprite(&tft, 0, 0, TFT_BLACK);
    tft.clearClipRect();
  }
}




void handleFps()
{
  if( show_fps ) {
    GWSprite.setCursor(0,GWSprite.height()-1);
    GWSprite.setTextDatum(BL_DATUM);
    GWSprite.setFont(&FreeSansBold9pt7b);
    GWSprite.setTextColor(TFT_WHITE);
    GWSprite.setTextSize(1);
    GWSprite.printf("%.2f fps ", fpsCounter.getFps() );
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
  //textBox.setFont(&FreeSansBold24pt7b);
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
  if( x<tft.width()/2 ) {
    // SD_MMC
    gwFS = &SD_MMC;
  } else {
    // LittleFS
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
    case GWImage::RAW   : // default, TODO: implement zoom
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

  // extrudeShadow(&tft, name, x, y, text_color, extrude_width, extrude_color, shadow_width, shadow_color);
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
  uint16_t* buf = (uint16_t*)src->getBuffer();
  uint32_t buflen = src->bufferLength()/2;
  for(int x=box.x; x<box.x+box.w; x++) {
    for(int y=box.y; y<box.y+box.h; y++) {
      uint32_t idx = x + y*src->width();
      if( idx>buflen ) {
        ESP_LOGD(GFX_TAG, "idx(%d)>buflen(%d)", idx, buflen);
        continue;
      }

      if( buf[idx] != 0 && buf[idx] != 0xffff ) {
        buf[idx] = 0xffff-buf[idx];
      }
    }
  }
  dst->setClipRect( box.x, box.y, box.w, box.h );
  src->pushSprite(0,0,TFT_BLACK);
  dst->clearClipRect();
}




template <typename GFX>
void draw_menu(GFX* gfx, int transition_direction=0)
{
  assert(gfx);

 /*
  * Blending operations:
  * - Background ('BGSprite' sprite) composed from:
  *   - Game artwork (pic of the console)
  *   - Game framebuffer (from the emulatorm, 'GWSprite' sprite)
  * - Foreground ('FGSprite' sprite) composed from:
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

  uint32_t buflen = FGSprite.bufferLength()/2;
  uint16_t* buf = (uint16_t*)FGSprite.getBuffer();

  uint32_t now = millis();


  if( update_fg ) {
    for(int i=0;i<buflen;i++) {
      if(i%3<=1)
        buf[i] = 0;
      else
        buf[i] = 0xffff;
    }
    ESP_LOGD(GFX_TAG, "mask fill time: %d ms", millis()-now);
    now = millis();

    // // undo the dim effect over the Game&Watch button (background) as it is needed to exit the menu
    FGSprite.fillRect( GAWBox.x, GAWBox.y, GAWBox.w, GAWBox.h, TFT_BLACK );
  }


  // draw the volume button
  drawVolume(&FGSprite);

  if( update_fg ) {
    if( debug_buttons )
      for( int i=0;i<optionButtonsCount;i++)
        drawBounds(&FGSprite, &OptionButtons[i]);

    // draw always-visible icons
    drawButton(&FGSprite, &BtnOptionVolP);
    drawButton(&FGSprite, &BtnOptionVolM);
    drawButton(&FGSprite, &BtnOptionOpts);
  }

  uint16_t BtnOptionColor = BtnOption.enabled() ? TFT_GREEN : TFT_RED;
  FGSprite.fillCircle(BtnOption.btn->xs+10, BtnOption.btn->ys+10, 5, BtnOptionColor);

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
  }

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

  ppa_blend->exec();
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
