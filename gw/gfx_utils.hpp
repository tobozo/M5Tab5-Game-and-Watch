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
} fpsCounter;

const char* GFX_TAG = "GW Utils";

GWBox GWFBBox(0,0,GW_SCREEN_WIDTH,GW_SCREEN_HEIGHT);

GWImage IconSaveRTC{.file={nullptr,save_rtc_jpg , save_rtc_jpg_len }, .type=GWImage::JPG, .width=77, .height=48 };
GWImage IconDebug  {.file={nullptr,dgb_png      , dgb_png_len      }, .type=GWImage::PNG, .width=64, .height=64 };
GWImage IconFPS    {.file={nullptr,fps_png      , fps_png_len      }, .type=GWImage::PNG, .width=64, .height=64 };
GWImage IconGamepad{.file={nullptr,gamepad_png  , gamepad_png_len  }, .type=GWImage::PNG, .width=64, .height=64 };
GWImage IconMute   {.file={nullptr,mute_png     , mute_png_len     }, .type=GWImage::PNG, .width=64, .height=64 };
GWImage IconPPA    {.file={nullptr,ppa_png      , ppa_png_len      }, .type=GWImage::PNG, .width=64, .height=64 };
GWImage IconPrefs  {.file={nullptr,prefs_png    , prefs_png_len    }, .type=GWImage::PNG, .width=64, .height=64 };
GWImage IconSndM   {.file={nullptr,snd_minus_png, snd_minus_png_len}, .type=GWImage::PNG, .width=64, .height=64 };
GWImage IconSndP   {.file={nullptr,snd_plus_png , snd_plus_png_len }, .type=GWImage::PNG, .width=64, .height=64 };
GWImage IconLoading{.file={nullptr,hourglass_png, hourglass_png_len}, .type=GWImage::PNG, .width=64, .height=64 };



// for pushSpriteTransition reveal effect
static uint16_t *vCopyBuff = nullptr;
static uint16_t *hCopyBuff = nullptr;

LGFX_Sprite canvas(&tft); // jpeg game background holder
LGFX_Sprite MenuSprite(&tft); // menu overlay for canvas
LGFX_Sprite BlendSprite(&tft); // menu overlay for canvas
LGFX_Sprite dim(&tft);    // semi transparent overlay to dim background
LGFX_Sprite textBox(&tft);
LGFX_Sprite GWSprite(&tft);
//LGFX_Sprite Hourglass(&MenuSprite); // hourglass pre-rendered png icon

static uint16_t *gw_fb; // Game&Watch framebuffer @16bpp
const uint32_t fbsize = GW_SCREEN_WIDTH * GW_SCREEN_HEIGHT * sizeof(uint16_t);

//static double zoomx = 1.0f; // framebuffer to canvas zoom ratio
//static double zoomy = 1.0f; // framebuffer to canvas zoom ratio

static std::vector<GWGame*> GWGames;

// TODO: store this in NVS
static int currentGame = 0;

static uint16_t volume = 8191; // 0..32767 (0x7fff)
static const uint16_t volume_increment = 2048; // ~16 levels

RGBColor heatMapColors[] = { {0, 0xff, 0}, {0xff, 0xff, 0}, {0xff, 0x80, 0}, {0xff, 0, 0} }; // green => yellow => orange => red

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

// forward declaration
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
    MenuSprite.pushSprite(0,0,TFT_BLACK);
    //tft.pushImage(0,0,tft.width(),tft.height(),(uint16_t*)buf, TFT_BLACK);
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




// uint8_t sp_to_bin_seq(uint8_t sp)
// {
//   // in binary, 0x11, 0x22, 0x33, etc have repeatable patterns
//   sp = (sp+15) &~15; // to nibble (0...f), will be doubled (1=>11, 2=>22, etc) to form a pattern
//   const char *fmt = "0x%x%x";
//   char s[8] = {0};
//   snprintf(s, 7, fmt, sp, sp);
//   uint x;
//   sscanf(s, "%x", &x);
//   return x&0xff;
// }
//
//
// template <typename GFX>
// void dim_screen(GFX* dst, int sparsity = 2)
// {
//   static LGFX_Sprite* dimPtr = nullptr;
//   static int last_sparsity = -1;
//
//   if(!dimPtr || dst->width()!=dim.width() || dst->height()!=dim.height() ) {
//     dim.setColorDepth(1);
//     dimPtr = (LGFX_Sprite*)dim.createSprite(dst->width(), dst->height());
//   }
//
//   if( sparsity != last_sparsity )
//   {
//     uint8_t* dimBuf = (uint8_t*)dim.getBuffer();
//     uint8_t pattern1 = sp_to_bin_seq(sparsity);
//     uint8_t pattern2 = 0xff-pattern1; // inverted pattern
//     uint32_t line_len = dim.width()/8;
//     for(int i=0;i<dim.bufferLength();i++) {
//       uint32_t y = i/line_len;
//       uint8_t bpos = y%8;
//       uint8_t bval = bitRead(pattern1, bpos);
//       if( bval == 0 )
//         dimBuf[i] = pattern1;
//       else
//         dimBuf[i] = pattern2;
//     }
//   }
//
//   dim.pushSprite(dst, 0, 0);
// }
//
//
// template <typename GFX>
// void dim_view(GFX* dst, int sparsity = 2)
// {
//   auto game = GWGames[currentGame];
//
//   dim.createSprite(game->view.innerbox.w, sparsity);
//   dim.fillSprite(TFT_BLACK);
//   for(int x=1;x<game->view.innerbox.w;x+=sparsity) {
//     dim.drawPixel(x-1, 0, TFT_WHITE);
//     dim.drawPixel(x,   sparsity/2, TFT_WHITE);
//   }
//
//   for(int y=game->view.innerbox.y;y<game->view.innerbox.y+game->view.innerbox.h;y+=sparsity) {
//     dim.pushSprite(dst ,game->view.innerbox.x, y, TFT_BLACK);
//   }
//   dim.deleteSprite();
// }


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
  switch(img->type)
  {
    case GWImage::JPG   : gfx->drawJpg(img->file.data, img->file.len, x, y, 0, 0, 0, 0, zoomx, zoomy); break;
    case GWImage::PNG   : gfx->drawPng(img->file.data, img->file.len, x, y, 0, 0, 0, 0, zoomx, zoomy); break;
    case GWImage::BMP   : gfx->drawBmp(img->file.data, img->file.len, x, y, 0, 0, 0, 0, zoomx, zoomy); break;
    case GWImage::RAW   : // default, TODO: implement zoom
    case GWImage::RGB565: gfx->pushImage(x, y, img->width, img->height, img->file.data, transparent_color); break;
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

  auto GAWButton = GWGames[currentGame]->getButtonByLabel("GAW");
  auto GAWBox = GAWButton->getBox();

  uint32_t buflen = MenuSprite.bufferLength()/2;
  uint16_t* buf = (uint16_t*)MenuSprite.getBuffer();

  for(int i=0;i<buflen;i++) {
    if(i%3<=1)
      buf[i] = 0;
    else
      buf[i] = 0xffff;
  }

  // // undo the dim effect over the Game&Watch button (background) as it is needed to exit the menu
  MenuSprite.fillRect( GAWBox.x, GAWBox.y, GAWBox.w, GAWBox.h, TFT_BLACK );

  // draw the volume button
  drawVolume(&MenuSprite);

  if( debug_buttons )
    for( int i=0;i<optionButtonsCount;i++)
      drawBounds(&MenuSprite, &OptionButtons[i]);

  // draw always-visible icons
  drawButton(&MenuSprite, &BtnOptionVolP);
  drawButton(&MenuSprite, &BtnOptionVolM);
  drawButton(&MenuSprite, &BtnOptionOpts);

  uint16_t BtnOptionColor = BtnOption.enabled() ? TFT_GREEN : TFT_RED;
  MenuSprite.fillCircle(BtnOption.btn->xs+10, BtnOption.btn->ys+10, 5, BtnOptionColor);

  if( in_options_menu ) {
    // draw icons from options menu
    for( int i=0;i<optionButtonsCount;i++) {
      if(strcmp(OptionButtons[i].name, "Options")==0)
        break; // don't render the gear icon or any icon listed after
      if(OptionButtons[i].icon)
        drawButton(&MenuSprite, &OptionButtons[i] );
    }
    // draw colored spot to indicate if the feature is enable or disabled
    for(int i=0;i<toggleSwitchesCount;i++) {
      uint16_t OptionColor = toggleSwitches[i].enabled() ? TFT_GREEN : TFT_RED;
      MenuSprite.fillCircle(toggleSwitches[i].btn->xs+10, toggleSwitches[i].btn->ys+10, 5, OptionColor);
    }
  }

  extern void preload_games();

  while( GWGames.size() == 0 ) {
    fsPicker();
    preload_games();
  }

  auto gamesCount = GWGames.size();
  auto prevGame = currentGame==0 ? GWGames[gamesCount-1] : GWGames[currentGame-1];
  auto game     = GWGames[currentGame];
  auto nextGame = currentGame+1>=gamesCount ? GWGames[0] : GWGames[currentGame+1];

  uint32_t bw = 10;
  textBox.setTextSize(1);
  uint32_t th = textBox.fontHeight()*2.5;

  uint32_t y00 = MenuSprite.height()-th;
  uint32_t y01 = MenuSprite.height()/2-th;

  uint32_t y10 = MenuSprite.height()-th;
  uint32_t y11 = MenuSprite.height()/2-th;

  uint32_t x0 = bw;
  uint32_t x1 = MenuSprite.width()-(textBox.width()+bw+1);

  String menu = "< " + String(prevGame->name) + " | " + String(nextGame->name) + " > ";

  // previous game
  textAt(&MenuSprite, prevGame->name, x0, y00, 1.0, 1.0, TL_DATUM);
  textAt(&MenuSprite, "<<",           x0, y01, 1.0, 4.0, TL_DATUM);

  // next game
  textAt(&MenuSprite, nextGame->name, x1, y10, 1.0, 1.0, TR_DATUM);
  textAt(&MenuSprite, ">>",           x1, y11, 1.0, 4.0, TR_DATUM);

  // current game title and list-progress overlaying the game framebuffer
  uint32_t x2 = MenuSprite.width()/2 - textBox.width()/2;
  uint32_t y2 = game->view.innerbox.y + (game->view.innerbox.h/2 - textBox.height()/2);
  textAt(&MenuSprite, game->name, x2, y2, 1.1, 1.1, TC_DATUM);
  String pgStr = String(currentGame+1)+"/"+String(gamesCount); // progress
  textAt(&MenuSprite, pgStr.c_str(), x2, y2+th, 0.8, 0.8, TC_DATUM);

  while(!ppa_blend->available())
    vTaskDelay(1);
  // blend canvas(bg) with MenuSprite(fg)
  ppa_blend->exec();
  while(!ppa_blend->available())
    vTaskDelay(1);
  pushSpriteTransition(&tft, &BlendSprite, transition_direction);
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
//     canvas.readRect(box.x, box.y, box.w, box.h, buffer16);
//     for(int j=0;j<box.w*box.h;j++) {
//       lgfx::rgb565_t color16 = buffer16[j];
//       lgfx::argb8888_t color32 { 0xff, color16.R8(), color16.G8(), color16.B8() };
//       buffer32[j] = color32;
//     }
//     // draw transparent stuff
//     alphaSprite.fillCircle((box.w/2)-1, (box.h/2)-1, box.w/4, (lgfx::argb8888_t)0x80808080u);
//     canvas.pushAlphaImage(box.x, box.y, box.w, box.h, buffer32 );
//     // alphaSprite.pushSprite(&canvas, box.x, box.y); // works too but slower
//     alphaSprite.deleteSprite();
//     free(buffer16);
//  }
