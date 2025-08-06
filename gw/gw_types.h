#pragma once

#include <FS.h>

struct GWBox
{
  double x{0}, y{0}; // 0..1 until normalized
  double w{0}, h{0}; // 0..1 until normalized
  void normalize(GWBox box)
  {
    x = x * box.w + box.x;
    y = y * box.h + box.y;
    w = w * box.w;
    h = h * box.h;
  }
};


// GWTouchButton: Translate touch coords to Game&Watch hardware button signal
struct GWTouchButton
{
public:
  const char* name{nullptr};
  double xs{0}, ys{0}; // 0..1 until normalized
  double xe{0}, ye{0}; // 0..1 until normalized
  const uint32_t hw_val{0}; // button code for the emulator
  bool normalized{false};
  const uint8_t keyCode{0};

  GWTouchButton(const char* name, double x, double y, double w, double h, uint32_t hw_val,  uint8_t keyCode, bool normalized=false)
  : name(name), xs(x), ys(y), xe(w+x), ye(h+y), hw_val(hw_val), normalized(normalized), keyCode(keyCode) { }

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


// Emulator layout (inner and outer) sizes and positions
struct GWLayout
{
  const char* name{nullptr};
  GWBox innerbox{0,0,0,0}; // x, y, w, h
  GWBox outerbox{0,0,1,1}; // x, y, w, h
  bool normalized{false};
};


// Ram file holder
struct GWFile
{
  char *path{nullptr};          // file path
  unsigned char *data{nullptr}; // data
  uint32_t len{0};              // file size

  GWFile() { }
  ~GWFile()
  {
    if(path) delete(path);
    if(data) free(data);
  }
  void setPath(const char*name)
  {
    assert(name);
    path = new char[strlen(name)+1];
    strcpy(path, name);
    path[strlen(name)] = '\0'; // superstitious terminator: strcpy supposedly did it, but terminate anyway
  }
};


// Game holder
struct GWGame
{
  const char* name{nullptr};    // Game name e.g. "Parachute"
  const char* romname{nullptr}; // Rom name e.g. "pchute"
  GWLayout view;                // Inner/Outer emulator view
  GWTouchButton* btns{nullptr}; // Array of touch buttons
  const size_t btns_count{0};   // Amount of buttons in the array

  // jpg original size (image will be zoomed to fit the outer view)
  uint32_t jpg_width{0};
  uint32_t jpg_height{0};
  float jpg_zoom{2.0};

  GWFile romFile = GWFile();
  GWFile jpgFile = GWFile();

  bool preloaded{false};

  template <size_t N>
  GWGame(const char* name, const char*romname, GWLayout view, GWTouchButton (&tbtns)[N])
   : name(name), romname(romname), view(view), btns((GWTouchButton *)&tbtns), btns_count(N) { }

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
      view.outerbox.normalize({offset_x, offset_y, layout_width, layout_height});
        //Serial.printf("view [outer-normalized] = [%d:%d][%d*%d]\n", (int)view.outerbox.x, (int)view.outerbox.y, (int)view.outerbox.w, (int)view.outerbox.h);
      // normalize inner view
      view.innerbox.normalize( view.outerbox );
      // Serial.printf("view [inner-normalized] = [%d:%d][%d*%d]\n", (int)view.innerbox.x, (int)view.innerbox.y, (int)view.innerbox.w, (int)view.innerbox.h);
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
      frames_count = 0;//1;//last_avg!=0 ?        1 : 0;
      ms_count     = 0;//last_avg;//last_avg!=0 ? last_avg : 0;
    }

  }

  float getFps()
  {
    return avgms!=0 ? 1000.f/avgms : 0;
  }

} fpsCounter;

