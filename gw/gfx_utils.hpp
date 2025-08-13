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


static bool show_fps = false;
static bool debug_buttons = false; // true;
static bool debug_gestures = false;

// for pushSpriteTransition reveal effect
static uint16_t *vCopyBuff = nullptr;
static uint16_t *hCopyBuff = nullptr;

LGFX_Sprite canvas(&tft); // jpeg game background holder
LGFX_Sprite dim(&tft);    // semi transparent overlay to dim background
LGFX_Sprite textBox(&tft);
LGFX_Sprite GWSprite(&tft);

static uint16_t *gw_fb; // Game&Watch framebuffer @16bpp
const uint32_t fbsize = GW_SCREEN_WIDTH * GW_SCREEN_HEIGHT * sizeof(uint16_t);

static double zoomx = 1.0f; // framebuffer to canvas zoom ratio
static double zoomy = 1.0f; // framebuffer to canvas zoom ratio

static std::vector<GWGame*> GWGames;

// TODO: store this in NVS
static int currentGame = 0;

static uint16_t volume = 8191; // 0..32767 (0x7fff)
static const uint16_t volume_increment = 2048; // ~16 levels

RGBColor heatMapColors[] = { {0, 0xff, 0}, {0xff, 0xff, 0}, {0xff, 0x80, 0}, {0xff, 0, 0} }; // green => yellow => orange => red


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
  //tft.endWrite();
  tft.clear();
  tft.setFont(&FreeSansBold24pt7b);
  tft.setTextSize(2);
  tft.setTextColor(TFT_RED);
  tft.setCursor(0, tft.height()/2);
  tft.printf(format, args);
  //tft.startWrite();
  Serial.printf(format, args);
  Serial.println();
  va_end (args);
}


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


void drawVolume(int32_t dstx, int32_t dsty, uint32_t width=200/*multiple of 32*/, uint32_t height=48)
{
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
    tft.fillRect(dstx+x, dsty+y, volume_bar_width, h, volumeColor);
  }
}


// Gets the JPEG size from the array of data passed to the function, file reference: http://www.obrador.com/essentialjpeg/headerinfo.htm
bool get_jpeg_size(unsigned char* data, unsigned int data_size, uint32_t *width, uint32_t *height)
{
  //int i=0;   // Keeps track of the position within the file
  // Check for valid JPEG file (SOI Header)
  if(data[0] != 0xFF || data[1] != 0xD8 || data[2] != 0xFF || data[3] != 0xE0) {
    ESP_LOGE(GFX_TAG, "Not a valid SOI header");
    return false;
  }

  //i += 4;

  // Check for valid JPEG header (null terminated JFIF)
  if(data[6] != 'J' || data[7] != 'F' || data[8] != 'I' || data[9] != 'F' || data[10] != 0x00) { // Not a valid JFIF string
    ESP_LOGE(GFX_TAG, "Not a valid JFIF string");
    return false;
  }

  int i = 4; // position within the file

  //if(data[i+2] == 'J' && data[i+3] == 'F' && data[i+4] == 'I' && data[i+5] == 'F' && data[i+6] == 0x00) {
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


void pushSpriteTransition( LGFX_Sprite* dst, int direction=0 )
{
  uint16_t breadth = 16; // copybuff breadth

  if(!vCopyBuff)
    vCopyBuff = (uint16_t *)ps_malloc(breadth*tft.height()*sizeof(uint16_t));
  if(!hCopyBuff)
    hCopyBuff = (uint16_t *)ps_malloc(breadth*tft.width()*sizeof(uint16_t));

  if(!vCopyBuff || !hCopyBuff || dst->width()%breadth!=0 || dst->height()%breadth!=0) {
    dst->pushSprite(0,0);
    return;
  }

  if( direction==0 ) { // top to bottom
    auto chunks = dst->height()/breadth;
    for (int chunk=0,y=0;chunk<chunks;chunk++,y=chunk*breadth) {
      dst->readRect(0, y, dst->width(), breadth, hCopyBuff);
      tft.pushImage(0, y, dst->width(), breadth, hCopyBuff);
    }
  } else if( direction > 0 ) { // right to left
    auto chunks = dst->width()/breadth;
    for(int chunk=chunks-1, x=(chunks-1)*breadth;chunk>=0;chunk--,x=chunk*breadth) {
      dst->readRect(x, 0, breadth, dst->height(), vCopyBuff);
      tft.pushImage(x, 0, breadth, dst->height(), vCopyBuff);
    }
  } else { // left to right
    auto chunks = dst->width()/breadth;
    for(int chunk=0,x=0;chunk<chunks;chunk++,x=chunk*breadth) {
      dst->readRect(x, 0, breadth, tft.height(), vCopyBuff);
      tft.pushImage(x, 0, breadth, tft.height(), vCopyBuff);
    }
  }
}


void dim_view(int sparsity = 2)
{
  auto game = GWGames[currentGame];

  dim.createSprite(game->view.innerbox.w, sparsity);
  dim.fillSprite(TFT_BLACK);
  for(int x=1;x<game->view.innerbox.w;x+=sparsity) {
    dim.drawPixel(x-1, 0, TFT_WHITE);
    dim.drawPixel(x,   sparsity/2, TFT_WHITE);
  }

  for(int y=game->view.innerbox.y;y<game->view.innerbox.y+game->view.innerbox.h;y+=sparsity) {
    dim.pushSprite(game->view.innerbox.x, y, TFT_BLACK);
  }
  dim.deleteSprite();
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

template <typename GFX>
void drawBounds(GFX* dst, GWTouchButton *btn)
{ // for debug
  dst->drawRect(btn->xs, btn->ys, btn->xe-btn->xs, btn->ye-btn->ys, TFT_RED);
}


template <typename GFX>
void textAt(GFX*dst, const char* str, int32_t x, int32_t y, float fontSize=1.0, lgfx::textdatum_t datum=TL_DATUM)
{
  int extrude_width = 4;
  int shadow_width = 10;
  int cbwidth = extrude_width+shadow_width;
  uint32_t mask_color    = 0x303030u;
  uint32_t text_color    = 0xfbb11eu;
  uint32_t extrude_color = 0x000000u;
  uint32_t shadow_color  = 0x404040u;
  int32_t posx;

  textBox.fillSprite(mask_color);
  textBox.setFont(&FreeSansBold24pt7b);
  textBox.setTextSize(fontSize);
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



void draw_menu()
{
  dim_view(4);

  auto gamesCount = GWGames.size();

  if( gamesCount == 0 ) {
    // TODO: filesystem picker
    return;
  }

  auto prevGame = currentGame==0 ? GWGames[gamesCount-1] : GWGames[currentGame-1];
  auto game     = GWGames[currentGame];
  auto nextGame = currentGame+1>=gamesCount ? GWGames[0] : GWGames[currentGame+1];

  uint32_t bw = tft.width()/20;
  uint32_t y0 = tft.height()-textBox.height(); //game->view.innerbox.y + bw/2;
  uint32_t x0 = bw; // game->view.innerbox.x + 20; // ML_DATUM

  uint32_t x1 = tft.width()-(textBox.width()+bw); // game->view.innerbox.x + game->view.innerbox.w - 20; // MR_DATUM
  uint32_t y1 = tft.height()-textBox.height(); // game->view.innerbox.y + bw + bw/2;

  uint32_t x2 = tft.width()/2 - textBox.width()/2;
  uint32_t y2 = game->view.innerbox.y + (game->view.innerbox.h/2 - textBox.height()/2);

  String menu = "< " + String(prevGame->name) + " | " + String(nextGame->name) + " > ";

  textAt(&tft, prevGame->name, x0, y0, 1.0, TL_DATUM);
  textAt(&tft, "<<", x0, y0+textBox.fontHeight()*2.5, 1.0, TL_DATUM);
  textAt(&tft, nextGame->name, x1, y1, 1.0, TR_DATUM);
  textAt(&tft, ">>", x1, y1+textBox.fontHeight()*2.5, 1.0, TR_DATUM);
  textAt(&tft, game->name, x2, y2, 1.1, TC_DATUM);

  String pgStr = String(currentGame+1)+"/"+String(gamesCount); // progress
  textAt(&tft, pgStr.c_str(), x2, y2+textBox.fontHeight()*2.5, 0.8, TC_DATUM);
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
