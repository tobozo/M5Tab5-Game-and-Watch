/*\
 *
 * M5Stack Tab5 Game&Watch Emulator
 *
 * A speculation brought to you by tobozo, copyleft (c+) 2025
 *
 * Features:
 *
 * - Game menu:
 *   - +Overlayed menu to navigate between games
 *   - +Supports 30+ single screen games
 *   - +Load roms and artworks from filesystem (SD or LittleFS)
 *     NOTE: assets are loaded to psram, ~8MB max. will be used
 *   - +Save Volume and last game selection to RTC memory
 *   - +Adjust RTC from Game Time
 * - Support for:
 *   - Builtin TouchScreen
 *   - Builtin RTC Clock
 *   - External USB Gamepad
 *   - External USB Keyboard
 *   - Automatic Screen Orientation at boot based on IMU
 * - Modified Emulator:
 *   - Lowered emulator frame rate to 64Hz and display frame rate to ~16Hz
 *   - Added support for gzipped roms
 *   - Added malloc setter to use psram
 *
 * Credits/Acknowledgements:
 *  - The LCD Game emulator engine -> https://github.com/bzhxx/LCD-Game-Emulator
 *  - This project was inspired by -> https://github.com/slowlane112/Esp32-Game-and-Watch
 *  - Roms are shrunk using MAME rom shrinker Tool -> https://github.com/bzhxx/LCD-Game-Shrinker
 *    NOTE: Archive 0.229 is required, see https://archive.org/details/mame-0.229-roms-split.-7z
 *  - Rom shrinking Tutorial -> https://gist.github.com/DNA64/16fed499d6bd4664b78b4c0a9638e4ef
 *    WARNING: shrinking requirements are tight (Ubuntu 20.04, Inkscape 1.1, Python3)
 *
 *
 * TODO:
 *  - MAME ROM/Artwork importer+shrinker+packer
 *  - Auto sleep / screen dim / wake on touch
 *
 * */

// Tab5 Hosted WiFi pins
// Use with WiFi.setPins(SDIO2_CLK, SDIO2_CMD, SDIO2_D0, SDIO2_D1, SDIO2_D2, SDIO2_D3, SDIO2_RST);
#define SDIO2_CLK GPIO_NUM_12
#define SDIO2_CMD GPIO_NUM_13
#define SDIO2_D0 GPIO_NUM_11
#define SDIO2_D1 GPIO_NUM_10
#define SDIO2_D2 GPIO_NUM_9
#define SDIO2_D3 GPIO_NUM_8
#define SDIO2_RST GPIO_NUM_15


#include "gw/gfx_utils.hpp"

size_t gamesCount = sizeof(SupportedGWGames)/sizeof(GWGame);

struct AudioBuffer
{
  uint16_t data[GW_AUDIO_BUFFER_LENGTH];
};

#define AUDIO_POOL_SIZE 16
static AudioBuffer bufferPool[AUDIO_POOL_SIZE];
QueueHandle_t audioBufferQueue;

static bool audio_ready = false;

static bool buffer_ready = false; // notify video task that a frame is ready to be pushed
// Emulator refresh rate (GW_REFRESH_RATE) and audio quality are tied, higher refresh rate = bigger audio buffer = better audio quality
// e.g. GW_REFRESH_RATE default value (128Hz) is unsustainable for the display.
// select a more realistic refresh rate. NOTE: real refresh rate will be approximated from this
static const uint32_t target_fps = 16; // must be a divisor of GW_REFRESH_RATE and a power of 2 while being sustainable by the display, not so many choices :)
// frames to skip e.g. if GW_REFRESH_RATE is 128Hz and target_fps is 16, blit every (128/16=8) frames instead of every frame
static const uint32_t blitCount = GW_REFRESH_RATE/target_fps;
// microseconds per Game&Watch system cycle
static const float us_per_cycle = (1000.0f*1000.0f)/float(GW_SYS_FREQ);
// milliseconds per call to gw_system_run(GW_SYSTEM_CYCLES), one "game tick"
static const float ms_per_cycles = (us_per_cycle*GW_SYSTEM_CYCLES)/1000.0f;
// last game tick for main loop
static TickType_t last_game_tick;
// same as ms_per_cycles but in ticks
static const TickType_t ticks_per_game_cycles = pdMS_TO_TICKS(ms_per_cycles);


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
    if(total_screens>0)
      Serial.printf("     %d Screens %d Keys\n", total_screens, total_keys);
  }
} kbd_debugger;




void gw_set_time()
{
  if( rtc_ok ) {
    time_t now = time(0);
    tm* localtm = localtime(&now);
    gw_time_t gwtime = { (uint8_t)localtm->tm_hour, (uint8_t)localtm->tm_min, (uint8_t)localtm->tm_sec };
    gw_system_set_time(gwtime);
  }
}


void gameCycle()
{
  static int audioBufferPos = 0;
  gw_system_run(GW_SYSTEM_CYCLES);
  // transfer audio to buffer pool
  for (size_t i = 0; i < GW_AUDIO_BUFFER_LENGTH; i++) {
    bufferPool[audioBufferPos].data[i] = (gw_audio_buffer[i] > 0) ? volume : 0;
  }
  // send pool buffer index to queue to signal it's ready to be played
  xQueueSend(audioBufferQueue, (void*)&audioBufferPos, portMAX_DELAY);
  audioBufferPos++;
  audioBufferPos = audioBufferPos%AUDIO_POOL_SIZE;
  gw_audio_buffer_copied = true;
}



bool preload_game(GWGame* gamePtr)
{
  assert(gamePtr);
  if(gamePtr->preloaded)
    return true;

  tft.setFont(&FreeSansBold24pt7b);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.setCursor(tft.width()/4, tft.height()/2-tft.fontHeight()*2);
  tft.printf("Checking %s        ", gamePtr->romname );

  if(!game_is_loadable(gamePtr)) {
    Serial.printf("Game %s is not loadable from %s\n", gamePtr->romname, gwFS==&LittleFS?"LittleFS":"SD_MMC");
    return false;
  }

  if( !load_asset(&gamePtr->romFile) ) {
    Serial.printf("Failed to load asset %s for rom %s\n", gamePtr->romFile.path, gamePtr->romname);
    return false;
  }

  if( !load_asset(&gamePtr->jpgFile) ) {
    Serial.printf("Failed to load asset %s for rom %s\n", gamePtr->jpgFile.path, gamePtr->romname);
    return false;
  }

  if(! get_jpeg_size(gamePtr->jpgFile.data, gamePtr->jpgFile.len, &gamePtr->jpg_width, &gamePtr->jpg_height) ) {
    Serial.printf("Artwork size guessing failed for %s, not a jpeg?\n", gamePtr->romname);
    return false;
  }
  // adjust zoom and offsets according to image size, will either fit height or width
  gamePtr->adjustLayout(canvas.width(), canvas.height());

  gamePtr->preloaded = true;

  return gamePtr->romFile.data && gamePtr->romFile.len>0 && gamePtr->jpgFile.data && gamePtr->jpgFile.len>0;
}



bool load_game(uint8_t gameid)
{
  gamesCount = GWGames.size();

  if( gamesCount == 0 ) {
    return false;
  }

  gameid = gameid%gamesCount; // sanitize

  auto gamePtr = GWGames[gameid];
  if(!gamePtr)
    return false;

  if(!gamePtr->preloaded)
    if(!preload_game(gamePtr))
      return false;

  if( gamePtr->view.outerbox.x > 0 )
    // fill blank space on the left
    canvas.fillRect(0, 0, gamePtr->view.outerbox.x+1, canvas.height(), TFT_BLACK );
  // draw artwork
  canvas.drawJpg(gamePtr->jpgFile.data, gamePtr->jpgFile.len, gamePtr->view.outerbox.x, gamePtr->view.outerbox.y, 0, 0, 0, 0, gamePtr->jpg_zoom, gamePtr->jpg_zoom);
  if( gamePtr->view.outerbox.x > 0 )
    // fill blank space on the right
    canvas.fillRect(canvas.width()-(gamePtr->view.outerbox.x+1), 0, gamePtr->view.outerbox.x+1, canvas.height(), TFT_BLACK );

  // attach preloaded game len and data to the emulator
  ROM_DATA_LENGTH = gamePtr->romFile.len;
  ROM_DATA = (unsigned char *)gamePtr->romFile.data;

  if(!gw_system_romload()) {
    // gw emulator may be leaky, try to reload the rom file
    gamePtr->romFile.data = nullptr; // this is even more leaky
    gamePtr->romFile.len = 0;
    if( !load_asset(&gamePtr->romFile) ) {
      tft_error("ROM fs reload failed");
      return false;
    }
    ROM_DATA_LENGTH = gamePtr->romFile.len;
    ROM_DATA = (unsigned char *)gamePtr->romFile.data;
    if(!gw_system_romload()) {
      tft_error("ROM reload failed");
      return false;
    }
  }

  Serial.printf("Loaded Game: '%s' (rom=%s)\n", gamePtr->name, gamePtr->romname);

  currentGame = gameid;

  Tab5Rtc.writeRAM(0x02, currentGame); // save "currentGame" to RTC memory

  gw_system_sound_init();
  gw_system_config();
  gw_system_start();
  gw_system_reset();

  // run a few cycles to render the welcome screen
  for(int i=0;i<blitCount*2;i++) {
    gw_set_time();
    gw_system_run(GW_SYSTEM_CYCLES);
  }
  //M5.Speaker.stop();
  gw_system_blit(gw_fb);

  //kbd_debugger.printKeyboard(gw_keyboard);

  // 320x200 emulator panel will be stretched to fit in the innerview
  zoomx = float(gamePtr->view.innerbox.w)/float(GW_SCREEN_WIDTH);
  zoomy = float(gamePtr->view.innerbox.h)/float(GW_SCREEN_HEIGHT);
  // draw emulated panel into the inner box
  canvas.pushImageRotateZoom(gamePtr->view.innerbox.x, gamePtr->view.innerbox.y, 0, 0, 0.0, zoomx, zoomy, GW_SCREEN_WIDTH, GW_SCREEN_HEIGHT, gw_fb);

  // push outer and inner views
  static uint8_t previd = 0xff;

  if( gameid==0 ) {
    pushSpriteTransition(&canvas, 0);
  } else if( gameid > previd ) {
    pushSpriteTransition(&canvas, 1);
  } else {
    pushSpriteTransition(&canvas, -1);
  }
  previd = gameid;

  // // Debug touch buttons and view
  for( int i=0;i<gamePtr->btns_count;i++)
    drawBounds(&(gamePtr->btns[i]));
  tft.drawRect(gamePtr->view.innerbox.x-1, gamePtr->view.innerbox.y-1, gamePtr->view.innerbox.w+2, gamePtr->view.innerbox.h+2, TFT_RED);
  return true;
}


void handle_option_button(int state)
{
  switch(state) {
    case GW_BUTTON_UP:
      if( volume == 0 )
        volume = volume_increment-1;
      else
        volume += volume_increment;
      if( volume>0x7fff )
        volume = 0x7fff;
    break;
    case GW_BUTTON_DOWN:
      if( volume>= volume_increment )
        volume -= volume_increment;
      else
        volume=0;
    break;
    case GW_BUTTON_TIME:
      if( rtc_ok ) {
        gw_time_t gwtime = gw_system_get_time();
        const m5::rtc_time_t m5time(gwtime.hours, gwtime.minutes, gwtime.seconds);
        Tab5Rtc.setTime(m5time);
        Tab5Rtc.setSystemTimeFromRtc();
        Serial.printf("RTC+System time adjusted from G&W to %d:%d:%d\n", gwtime.hours, gwtime.minutes, gwtime.seconds);
      }
    break;
    default:
    break;
  }
  drawVolume(0, 0, 200, 48);
  Tab5Rtc.writeRAM(0x0, (uint8_t*)&volume, sizeof(volume));
}


int get_menu_gesture()
{

  M5.Speaker.stop(); // stop audio
  draw_menu();

  gamesCount = GWGames.size();

  if( gamesCount == 0 ) {
    // TODO: filesystem picker
    return 0;
  }

  static m5::touch_state_t prev_state;
  static int32_t flickx = -1;
  GWTouchButton *btns;
  int state = 0;
  int flick_dir = 0;
  int gameOffset = 0;
  std::vector<String> controlPorts = {/*"GAW",*/"GameA","GameB","Time","Alarm","ACL"};


  GWTouchButton OptionButtons[] = {
    GWTouchButton("🔈-", 0,     0, 100,  48, GW_BUTTON_DOWN, ARROW_DOWN, true),
    GWTouchButton("🔈+", 100,   0, 100,  48, GW_BUTTON_UP,   ARROW_UP,   true),
    GWTouchButton("🕑",  300,   0,  77,  48, GW_BUTTON_TIME, KEY_TIME,   true)
  };
  size_t optionButtonsCount = sizeof(OptionButtons)/sizeof(GWTouchButton);

//  for( int i=0;i<optionButtonsCount;i++) {
//     drawBounds(&OptionButtons[i]);
//
//     auto box = OptionButtons[i].getBox();
//     alphaSprite.createSprite(box.w,box.h);
//     int buffer_size = box.w*box.h*2;
//     auto buffer16 = (lgfx::rgb565_t*)ps_malloc(buffer_size);
//     auto buffer32 = (lgfx::argb8888_t*)alphaSprite.getBuffer();
//     canvas.readRect(box.x, box.y, box.w, box.h, buffer16);
//     for(int j=0;j<box.w*box.h;j++) {
//       lgfx::rgb565_t color16 = buffer16[j];
//       lgfx::argb8888_t color32 { 0xff, color16.R8(), color16.G8(), color16.B8() };
//       buffer32[j] = color32;
//     }
//     alphaSprite.fillCircle((box.w/2)-1, (box.h/2)-1, box.w/4, (lgfx::argb8888_t)0x80808080u);
//     canvas.pushAlphaImage(box.x, box.y, box.w, box.h, buffer32 );
//     // alphaSprite.pushSprite(box.x, box.y); // will it blend ?
//     alphaSprite.deleteSprite();
//     free(buffer16);
//  }

  drawVolume(0, 0, 200, 48);
  tft.drawJpg(save_rtc_jpg, save_rtc_jpg_len, 300, 0);

  uint8_t keycodeQueued;
  int last_keycode_usb = 0;
  m5::touch_detail_t touchDetail;

  while(1)
  {
    M5.delay(1);

    // got any queued HID signal to handle?
    if( xQueueReceive( usbHIDQueue, &( keycodeQueued ), 0) == pdPASS ) {
      keycode_usb = keycodeQueued;
      if( keycode_usb != 0 ) {
        last_keycode_usb = keycode_usb;
        goto _continue;
      }
      // WARNING: that logic does not handle multiple keypress
      if( keycode_usb == 0 ) {
        // Serial.printf("Received keyrelase\n");
        switch(last_keycode_usb) {
          case KEY_ESC     : return 0; break; // ESC: GAW
          case ARROW_LEFT  : goto _load_prev_game;
          case ARROW_RIGHT : goto _load_next_game;
          case ARROW_DOWN  : handle_option_button(GW_BUTTON_DOWN); goto _continue;
          case ARROW_UP    : handle_option_button(GW_BUTTON_UP);   goto _continue;
          case KEY_TIME    : handle_option_button(GW_BUTTON_TIME); goto _continue; // F3: Time
          case BUTTON_1    : break; // 0x2b, // TAB: Jump/Fire/Action
          case KEY_F1      : break; // 0x3a, // F1: Game A
          case KEY_F2      : break; // 0x3b, // F2: Game B
          case KEY_ALARM   : break; // 0x3d, // F4: Alarm
          case KEY_ACL     : break; // 0x3e  // F5: ACL
          case KEYCODE_NONE: break;
        }
        // return 0;
      }
    }

    // got any touch signal to handle ?
    M5.update();

    touchDetail = M5.Touch.getDetail();

    if (prev_state == touchDetail.state)
      goto _continue;

    prev_state = touchDetail.state;

    if( touchDetail.state == m5::touch_state_t::flick_begin ) {
      flickx = touchDetail.x;
      goto _continue;
    }

    // detect swipes
    if( touchDetail.state == m5::touch_state_t::flick_end ) {
      flick_dir = touchDetail.x - flickx;
      // Serial.printf("Flick dir : %s\n", flick_dir > 0 ? "right" : "left" );
      if(flick_dir<0)
        goto _load_next_game;
      else
        goto _load_prev_game;
    }

    // touch happening but not released yet
    if( touchDetail.state != m5::touch_state_t::touch_end )
      goto _continue;

    // touch happened, check if it was an Option Button
    for( int i=0;i<optionButtonsCount;i++) {
      state = OptionButtons[i].getHWState(touchDetail.x, touchDetail.y);
      if(state !=0) {
        handle_option_button(state);
        goto _continue;
      }
    }

    // check if it was a Game&Watch "Control" button
    btns = GWGames[currentGame]->btns;
    for( int i=0;i<GWGames[currentGame]->btns_count;i++) {
      state = btns[i].getHWState(touchDetail.x, touchDetail.y);
      if(state !=0) {
        auto tbnameStr = String(btns[i].name);
        Serial.printf("Touch button %s was hit\n", tbnameStr.c_str());
        for( int j=0;j<controlPorts.size();j++ ) {
          if( tbnameStr.endsWith(controlPorts[j]) ) {
            Serial.printf("Is control button -> %s\n", tbnameStr.c_str());
            // TODO: handle_option_button(state)
            goto _continue;
          }
        }
        if( tbnameStr.endsWith("GAW") )
          return 0; // exit game selector
      }
    }

    // check if it was the left side or the right side of the screen
    if(touchDetail.x>0 && touchDetail.x<canvas.width()/2)
      goto _load_prev_game;
    else if( touchDetail.x>0 && touchDetail.x>canvas.width()/2)
      goto _load_next_game;

    _continue:
    //debugTouchState(t.state);
    continue;

    _load_next_game:
      gameOffset = 1;
      goto _load_game;

    _load_prev_game:
      gameOffset = gamesCount-1;
      goto _load_game;

    _load_game:
      currentGame += gameOffset;
      currentGame = currentGame%gamesCount;
      load_game(currentGame);
      draw_menu();

  } // end while(1)

  return 0;
}



// buttons polling function called from the Game&Watch emulator
unsigned int gw_get_buttons()
{
  // TODO: handle multi-touch
  int32_t xt=0, yt=0;

  // static int keycode_usb = 0;
  uint8_t keycodeQueued;
  if( xQueueReceive( usbHIDQueue, &( keycodeQueued ), 0) == pdPASS ) {
    keycode_usb = keycodeQueued;
    // WARNING: that logic does not handle multiple keypress
    if( keycode_usb == 0 ) {
      Serial.printf("Received keyrelase\n");
      return 0;
    }
    Serial.printf("Received keypress (keycode=0x%x)\n", keycode_usb);
  } else if( keycode_usb==0 ) {
    if(!tft.getTouch(&xt, &yt))
      return 0;
  }

  if( keycode_usb==0 && xt==0 && yt==0 )
    return 0;

  auto btns = GWGames[currentGame]->btns;

  for( int i=0;i<GWGames[currentGame]->btns_count;i++) {
    int state = 0;
    if( keycode_usb != 0 ) {
      if( keycode_usb == btns[i].keyCode ) {
        state = btns[i].hw_val; // process keycode from USB keyboard
        // Serial.printf("Processed keycode [0x%x] to state 0x%x for btn %s\n", keycode_usb, state, btns[i].name);
      }
    } else if(xt>0 && yt>0) {
      state = btns[i].getHWState(xt, yt); // process coords from touch interface
      // if(state!=0)
      //   Serial.printf("Processed touch coords [%d:%d] to state 0x%x for btn %s\n", xt, yt, state, btns[i].name);
    }
    switch(state)
    {
      case -2: // GAW button
        if(xt>0 && yt>0) // was triggered by touch, wait for release
          while(tft.getTouch(&xt, &yt))
            vTaskDelay(1);
        // NOTE: Doing this is stupid, gw_get_buttons() is called from within the emulator, and further emulator operations will happen when it returns.
        //       Since get_menu_gesture() can trigger a rom overwrite and emulator reset, who knows what could happen? It is fortunate this even works.
        state = get_menu_gesture(); // blocking menu
        pushSpriteTransition(&canvas, 0);
        last_game_tick = xTaskGetTickCount(); // reset game tick
        gw_set_time(); // sync Game&Watch time with system time
        return 0;
      case -1:
        gw_system_reset();
        return 0;
      case 0:
        continue;
      default:
        return state;
    }
  }
  return 0;
}



void gameLoop()
{
  static int display_update_count = 0;

  gameCycle();

  display_update_count++;

  if (display_update_count == blitCount) {
    gw_system_blit(gw_fb);
    std::lock_guard<std::mutex> lck(tft_mtx);
    buffer_ready = true; // buffer readiness signal for video task
    display_update_count = 0;
  }
}



void videoBufferTask(void*param)
{
  while(1)
  {
    if( buffer_ready ) {
      auto game = GWGames[currentGame];
      tft.pushImageRotateZoom(game->view.innerbox.x, game->view.innerbox.y, 0, 0, 0.0, zoomx, zoomy, GW_SCREEN_WIDTH, GW_SCREEN_HEIGHT, gw_fb);
      std::lock_guard<std::mutex> lck(tft_mtx);
      buffer_ready = false;
      if( show_fps ) {
        fpsCounter.addFrame();
        tft.setCursor(0,tft.height()-1);
        tft.setTextDatum(BL_DATUM);
        tft.setFont(&FreeSansBold18pt7b);
        tft.setTextSize(1);
        tft.printf("%.2f fps ", fpsCounter.getFps() );
      }
    }
    vTaskDelay(1);
  }
}



void audioTask(void*param)
{
  auto spk_cfg = M5.Speaker.config();

  spk_cfg.sample_rate = GW_AUDIO_FREQ;
  spk_cfg.stereo = false;
  spk_cfg.task_pinned_core = xPortGetCoreID(); // attach to the same core

  M5.Speaker.config(spk_cfg);

  M5.Speaker.begin();
  M5.delay(100);
  M5.Speaker.tone(2000, 100);
  M5.delay(100);
  M5.Speaker.tone(1000, 100);
  M5.delay(100);
  M5.Speaker.stop();

  // zerofill buffer pool
  for( int i=0; i<AUDIO_POOL_SIZE; i++ )
    for( int j=0; j<GW_AUDIO_BUFFER_LENGTH; j++ )
      bufferPool[i].data[j] = 0;

  audio_ready = true;

  int *bufPos = new int();
  AudioBuffer buffer;

  while (true)
  {
    if (xQueueReceive(audioBufferQueue, bufPos, portMAX_DELAY) == pdPASS) {
      AudioBuffer *bufPtr = &bufferPool[*bufPos];
      if( bufPtr ) {
        for (size_t i = 0; i < GW_AUDIO_BUFFER_LENGTH; i++) {
          buffer.data[i] = bufPtr->data[i];
          bufPtr->data[i] = 0;
        }
        M5.Speaker.playRaw((const int16_t*)buffer.data, GW_AUDIO_BUFFER_LENGTH/2, GW_AUDIO_FREQ, false, 0, -1, true);
      }
    }
  }
}




void gameTask(void*param)
{

  M5.begin();

  // M5Tab5: force landcsape mode
  if (M5.Imu.isEnabled()) {
    M5.delay(100); // wait for IMU to collect some data
    float val[3];
    // not sure why Imu.getAccel() is needed to get the gyro data but why not :)
    M5.Imu.getAccel(&val[0], &val[1], &val[2]);
    if( val[0]<0 ) { // battery on the left hand side
      tft.setRotation(1);
    } else { // battery on the right hand side
      tft.setRotation(3);
    }
  } else {
    tft.setRotation(1); // default
  }

  tft.fillCircle(tft.width()/2, tft.height()/2, 48, random());

  // //debugButtons();

  xTaskCreatePinnedToCore(
    usbHostTask,   // function
    "usbHostTask", // name
    16384,      // stack size
    NULL,       // parameter
    1,          // priority
    NULL,       // handle
    xPortGetCoreID() // "this" core
  );

  audioBufferQueue = xQueueCreate(AUDIO_POOL_SIZE, sizeof(int *));

  // spawn subtask for audio
  xTaskCreatePinnedToCore(
    audioTask,   // function
    "audioTask", // name
    4096,      // stack size
    NULL,       // parameter
    1,         // priority
    NULL,       // handle
    (portNUM_PROCESSORS-1)-xPortGetCoreID() // attach to the "other" core
  );

  // spawn subtask for audio
  xTaskCreatePinnedToCore(
    videoBufferTask,   // function
    "videoBufferTask", // name
    4096,      // stack size
    NULL,       // parameter
    1,         // priority
    NULL,       // handle
    (portNUM_PROCESSORS-1)-xPortGetCoreID() // attach to the "other" core
  );


  while(!audio_ready) {
    vTaskDelay(10);
  }

  tft.fillCircle(tft.width()/2, tft.height()/2, 48, TFT_BLACK);

  initSDCard();
  initLittleFS();
  initRTC();

  if( rtc_ok ) {
    uint16_t ram_volume;
    if( Tab5Rtc.readRAM(0x0, (uint8_t*)&ram_volume, sizeof(ram_volume)) )
      if( ram_volume<=0x7fff )
        volume = ram_volume;
  }

  if( littlefs_ok && sdcard_ok ) {
    do {
      fsPicker();
      size_t loadable_games = 0;
      if(gwFS) {
        for(int i=0;i<gamesCount;i++) {
          auto gamePtr = (GWGame*)&SupportedGWGames[i];
          if( game_is_loadable(gamePtr) )
            loadable_games++;
        }
        if( loadable_games==0 )
          gwFS = nullptr;
      }
    } while( gwFS == nullptr );

  }
  else if( littlefs_ok )
    gwFS = &LittleFS;
  else if( sdcard_ok )
    gwFS = &SD_MMC;
  else
  {
    tft_error("No filesystem available, halting");
    while(1)
      vTaskDelay(1);
  }


  alphaSprite.setColorDepth(32);

  // sprite for stylized text boxes
  textBox.setPsram(true);
  if(!textBox.createSprite(tft.width()/3, tft.height()/3)) {
    Serial.println("halting");
    while(1);
  }

  // memory for screen
  gw_fb = (uint16_t *)ps_calloc(1, fbsize);

  if( gw_fb == NULL ) {
    tft_error("Failed to allocate %lu bytes for gw_fb, halting", fbsize);
    while(1);
  }

  // main sprite (tft fullscreen)
  canvas.setPsram(true);
  canvas.setColorDepth(16);
  if(!canvas.createSprite(tft.width(), tft.height())) {
    tft_error("Failed to create canvas sprite, Halting");
    while(1);
  }

  if( GWGames.size() == 0 ) {

    if( gamesCount == 0 ) {
      tft_error("WTF??");
      while(1);
    }

    int before = ESP.getFreePsram();
    Serial.printf("PSRAM Available: %d\n", before );

    tft.setTextSize(5);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);

    for(int i=0;i<gamesCount;i++) {
      auto gamePtr = (GWGame*)&SupportedGWGames[i];

      if(!preload_game(gamePtr))
        continue;

      GWGames.push_back( gamePtr );
      vTaskDelay(10);
    }
    gamesCount = GWGames.size();

    int after = ESP.getFreePsram();
    Serial.printf("PSRAM Available: %d (allocated=%.1f MB)\n", after, float(before-after)/(1024.0*1024.0) );
  }

  tft.setCursor(tft.width()/4, tft.height()*.8);
  tft.printf("    Found %d games \n", GWGames.size() );
  lgfx::delay(2000);

  gw_set_malloc( ps_malloc ); // set G&W emulator to use psram allocator

  uint8_t savedGame = Tab5Rtc.readRAM(0x02);
  if(savedGame<gamesCount)
    currentGame = savedGame;

  if(! load_game(currentGame) ) {
    tft_error("Game loading failed, Halting");
    while(1);
  }
  //Serial.printf("Game loaded with zoom factor x:%.2f, y:%.2f\n", zoomx, zoomy);

  last_game_tick = xTaskGetTickCount();

  while(1) {
    vTaskDelayUntil( &last_game_tick, ticks_per_game_cycles );
    gameLoop();
  }
}



void setup()
{
  Serial.begin(115200);
  Serial.printf("Game&Watch emulator (sys freq=%dKHz, refresh rate=%dfps, audiobuffer=%d bytes, cycles per loop=%d, us_per_cycle=%f, ms_per_cycles=%f)\n", GW_SYS_FREQ, GW_REFRESH_RATE, GW_AUDIO_BUFFER_LENGTH, GW_SYSTEM_CYCLES, us_per_cycle, ms_per_cycles);

  // the task that will spawn all tasks and run the game loop in a timely fashion
  xTaskCreatePinnedToCore(
    gameTask,   // function
    "gameTask", // name
    8192,      // stack size
    NULL,       // parameter
    16,          // priority
    NULL,       // handle
    xPortGetCoreID() // "this" core
  );


}



void loop()
{
  vTaskDelete(NULL);
}


