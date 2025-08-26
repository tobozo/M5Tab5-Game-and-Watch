/*\
 *
 * M5Stack Tab5 Game&Watch Emulator
 *
 * A speculation brought to you by tobozo, copyleft (c+) 2025
 *
\*/
#pragma once

#include <FS.h>


// Ram file holder
struct GWFile
{
public:
  char *path{nullptr}; // file path (optional)
  void *data{nullptr}; // data (bytes array or struct/class pointer)
  uint32_t len{0};     // bytes count

  GWFile() { }
  template <typename T>
  GWFile(char*path, T*_data, uint32_t len) : path(path), data((unsigned char*)_data), len(len) { }
  void setPath(const char*name)
  {
    assert(name);
    path = (char*)ps_calloc(1, strlen(name)+1);
    strcpy(path, name);
    path[strlen(name)] = '\0'; // superstitious terminator: strcpy supposedly did it, but terminate anyway
  }
};


// Wrapper for GWFile containing image data
struct GWImage
{
public:
  enum file_type {
    RAW,
    JPG,
    PNG,
    BMP,
    RGB565,
  };
  GWImage() { }
  GWImage(GWFile *file, file_type type, uint32_t width, uint32_t height) : file(file), type(type), width(width), height(height) { }
  GWFile *file{nullptr};
  file_type type{RAW};
  uint32_t width{0};
  uint32_t height{0};
};


// GWBox: unshifted/unscaled coords/dimensions (until normalized)
struct GWBox
{
  double x{0}, y{0}; // 0..1 until normalized
  double w{0}, h{0}; // 0..1 until normalized
  bool normalized{false};
  void normalize(GWBox box)
  {
    x = x * box.w + box.x;
    y = y * box.h + box.y;
    w = w * box.w;
    h = h * box.h;
    normalized = true;
  }
};


// GWTouchButton: Translate touch coords to Game&Watch hardware button signal
struct GWTouchButton
{
public:
  const char* name{nullptr};
  // touch area
  double xs{0}, ys{0}; // 0..1 until normalized
  double xe{0}, ye{0}; // 0..1 until normalized
  // button signal for GW emulator
  const uint32_t hw_val{0}; // button code for the emulator
  bool normalized{false};
  // USB keycode for touch<=>HID binding
  const uint8_t keyCode{0};
  GWImage* icon{nullptr}; // optional button image

  GWTouchButton(const char* name, double x, double y, double w, double h, uint32_t hw_val,  uint8_t keyCode, bool normalized=false, GWImage* icon=nullptr)
  : name(name), xs(x), ys(y), xe(w+x), ye(h+y), hw_val(hw_val), normalized(normalized), keyCode(keyCode), icon(icon) { }


  void normalize(const GWBox box)
  {
    xs = xs*box.w + box.x;
    xe = xe*box.w + box.x;
    ys = ys*box.h + box.y;
    ye = ye*box.h + box.y;
    normalized = true;
  }

  GWBox getBox()
  {
    return {xs, ys, (xe-xs)+1, (ye-ys)+1};
  }

  uint32_t getHWState(int32_t x, int32_t y)
  { // TODO: handle multi-touch
    if(xe==xs || ye==ys)
      return 0; // null button
    if( x >= xs && x < xe && y >= ys && y < ye )
      return hw_val; // coords are in the button zone !
    return 0; // coords are outside the button zone
  }

};


// Buttons with icon and on/off indicator
struct GWToggleSwitch
{
public:
  GWTouchButton *btn;
  GWToggleSwitch( GWTouchButton* btn, bool*state ) : btn(btn), state(state) { assert(btn); assert(state); last_state=!*state; }
  bool enabled() const { return *state; }
  bool changed() { bool ret = *state!=last_state; last_state = *state; return ret; }
private:
  bool* state;
  bool last_state;
};


// Emulator layout (inner and outer) sizes and positions
struct GWLayout
{
  const char* name{nullptr};
  GWBox innerbox{0,0,0,0}; // x, y, w, h
  GWBox outerbox{0,0,1,1}; // x, y, w, h
  bool normalized{false};
};


// forward declaration for ppa operations
namespace lgfx
{
  // hw accel for pixels needed by GWGame struct
  class PPASrm;
  class PPABlend;
}


// Game holder
struct GWGame
{
  const char* name{nullptr};    // Game name e.g. "Parachute"
  const char* romname{nullptr}; // Rom name e.g. "pchute"
  GWLayout view;                // Inner/Outer emulator view
  GWTouchButton* btns{nullptr}; // Array of touch buttons

  lgfx::PPASrm* ppa_tft{nullptr}; // scale/rotate/mirror ppa object for target: TFT
  lgfx::PPASrm* ppa_fb{nullptr}; // scale/rotate/mirror ppa object for target : BGSprite

  const size_t btns_count{0};   // Amount of buttons in the array

  // jpg background image data + dimensions without scaling
  GWImage *bgImage{nullptr};
  uint32_t jpg_width{0};
  uint32_t jpg_height{0};
  // background image will be zoomed to fit the outer view
  float jpg_zoom{2.0};
  // emulator panel will be zoomed to fit the inner view
  double zoomx{1.0f}; // view.input to output framebuffer
  double zoomy{1.0f}; // view.input to output framebuffer

  GWFile romFile = GWFile();
  GWFile jpgFile = GWFile();

  bool preloaded{false};

  template <size_t N>
  GWGame(const char* name, const char*romname, GWLayout view, GWTouchButton (&tbtns)[N])
   : name(name), romname(romname), view(view), btns((GWTouchButton *)&tbtns), btns_count(N) { }


  GWTouchButton* getButtonByLabel(const char*named_like)
  {
    String suffix = String(named_like);
    for( int i=0;i<btns_count;i++) {
      auto tbnameStr = String(btns[i].name);
      if( tbnameStr.endsWith(suffix) )
        return &btns[i];
    }
    return nullptr;
  }

  // scale jpeg, normalize buttons, inner and outer box
  void adjustLayout(uint32_t width, uint32_t height)
  {
    assert(width>0 && height>0);
    if( jpg_width==0 || jpg_height==0 ) { // jpg width and height must be set prior to expanding coords
      log_e("Bad layout for %s", name);
      return;
    }

    if( !view.normalized ) { // normalize rom view and touch buttons to display dimensions/coords
      // calculate jpeg zoom ratio
      jpg_zoom = fmin(float(height)/float(jpg_height), float(width)/float(jpg_width));
      // layout real size based on the image dimensions
      float layout_width  = jpg_width*jpg_zoom;
      float layout_height = jpg_height*jpg_zoom;
      // h/v offsets to center the layout
      float offset_x = float( width/2.0f) - layout_width/2.0f;
      float offset_y = float(height/2.0f) - layout_height/2.0f;
      // populate outer view

      if(!view.outerbox.normalized)
        view.outerbox.normalize({offset_x, offset_y, layout_width, layout_height});
        //Serial.printf("view [outer-normalized] = [%d:%d][%d*%d]\n", (int)view.outerbox.x, (int)view.outerbox.y, (int)view.outerbox.w, (int)view.outerbox.h);
      // normalize inner view
      if(!view.innerbox.normalized) {
        view.innerbox.normalize( view.outerbox );
        view.innerbox.w = int(view.innerbox.w/16.0f) * 16.0f; // round to nearest multiple of 16
        view.innerbox.h = int(view.innerbox.h/16.0f) * 16.0f; // round to nearest multiple of 16
      }
      //Serial.printf("view [inner-normalized] = [%d:%d][%d*%d]\n", (int)view.innerbox.x, (int)view.innerbox.y, (int)view.innerbox.w, (int)view.innerbox.h);
      // buttons are shared across screen types, so normalize separately
      for(int i=0;i<btns_count;i++) {
        if( !btns[i].normalized )
          btns[i].normalize(view.outerbox); // apply zoom and offset
      }
      view.normalized = true; // mark coords as normalized
    }
    // Serial.printf("view [absolute] = [%d:%d][%d*%d]\n", (int)view.innerbox.x, (int)view.innerbox.y, (int)view.innerbox.w, (int)view.innerbox.h);
  }
};


// for USB-HID debugging
struct gw_kbd_debugger
{
  void printKey(int byteNum, uint8_t byteVal)
  {
    Serial.printf(" K%d=", byteNum+1);
    uint8_t bits_enabled = 0;
    for(int bitNum=0;bitNum<8;bitNum++) {
      if( bitRead(byteVal, bitNum) == 0 ) {
        continue;
      }
      bits_enabled++;
      String buttonLabel = "                ";
      switch(bitNum) {
        case 0: buttonLabel ="BUTTON_LEFT" ; break;
        case 1: buttonLabel ="BUTTON_UP"   ; break;
        case 2: buttonLabel ="BUTTON_RIGHT"; break;
        case 3: buttonLabel ="BUTTON_DOWN" ; break;
        case 4: buttonLabel ="BUTTON_A"    ; break;
        case 5: buttonLabel ="BUTTON_B"    ; break;
        case 6: buttonLabel ="BUTTON_TIME" ; break;
        case 7: buttonLabel ="BUTTON_GAME" ; break;
      }
      if(bits_enabled>1) {
        Serial.print("+");
      }
      Serial.print(buttonLabel);
    }
  }

  void printKeyboard(unsigned int *kbd)
  {
    uint8_t total_screens = 0;
    uint8_t total_keys = 0;

    for (int i = 0; i < 8; i++) {
      if( kbd[i] == 0 || kbd[i] == 0x00804000 || kbd[i]==0x00008000)
        continue;
      uint8_t K1 = kbd[i] & 0xFF;
      uint8_t K2 = (kbd[i] >> 8) & 0xFF;
      uint8_t K3 = (kbd[i] >> 16) & 0xFF;
      uint8_t K4 = (kbd[i] >> 24) & 0xFF;
      uint8_t S[4] = {K1, K2, K3, K4};
      Serial.printf("[S%d] 0x%08x ", i+1, kbd[i]);
      uint8_t enabled_keys = 0;
      for(int byteNum=0;byteNum<4;byteNum++) {
        if( S[byteNum] == 0 )
          continue;
        enabled_keys++;
        printKey(byteNum, S[byteNum]);
      }
      total_keys += enabled_keys;
      total_screens ++;
      Serial.println();
    }

    if(total_screens>0) {
      ESP_LOGD("KDB Debug", "Game has %d Screen%s and %d Keys\n", total_screens, total_screens>1?"s":"", total_keys);
    }

  }
} kbd_debugger;


// togglable GUI options
static bool show_fps = false;
static bool debug_buttons = false;
static bool debug_gestures = false;

static bool in_options_menu = false; // navigation : show/hide options menu

static bool sdcard_ok = false;
static bool littlefs_ok = false;

// togglable hardware features
static bool audio_enabled   = true;
static bool usbhost_enabled = true;
