/*\
 *
 * M5Stack Tab5 Game&Watch Emulator
 *
 * A speculation brought to you by tobozo, copyleft (c+) 2025
 *
\*/

#include "../gw/gfx_utils.hpp"

const char* GW_TAG = "Game&Watch"; // debug tag

static size_t gamesCount = sizeof(SupportedGWGames)/sizeof(GWGame);

static void (*onAfterGameCycles)() = nullptr;
static void (*onBeforeGameCycles)() = nullptr;



void gw_set_time()
{
  if( rtc_ok ) {
    time_t now = time(0);
    tm* localtm = localtime(&now);
    gw_time_t gwtime = { (uint8_t)localtm->tm_hour, (uint8_t)localtm->tm_min, (uint8_t)localtm->tm_sec };
    gw_system_set_time(gwtime);
  }
}


bool boot_game(GWGame* gamePtr)
{
  assert(gamePtr);
  // attach preloaded game len and data to the emulator
  ROM_DATA_LENGTH = gamePtr->romFile.len;
  ROM_DATA = (unsigned char *)gamePtr->romFile.data;

  if(!gw_system_romload())
  {
    //ESP_LOGE(GW_TAG, "ROM %s failed to load", gamePtr->romname);
    gamePtr->romFile.data = nullptr; // this is leaky
    gamePtr->romFile.len = 0;
    gamePtr->preloaded = false;
    return false;
  }

  gw_system_sound_init();
  gw_system_config();
  gw_system_start();
  gw_system_reset();

  // run a few cycles to render the welcome screen
  for(int i=0;i<blitCount*2;i++) {
    gw_set_time();
    gw_system_run(GW_SYSTEM_CYCLES);
  }
  gw_system_blit(gw_fb);
  return true;
}


bool preload_game(GWGame* gamePtr)
{
  assert(gamePtr);
  if(gamePtr->preloaded)
    return true;

  static char checking[256] = {0};
  snprintf(checking, 255, "        Checking %s        ", gamePtr->romname );

  tft.setFont(&FreeSansBold24pt7b);
  tft.setTextSize(1);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(checking, tft.width()/2, tft.height()/2-tft.fontHeight()*3);

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

  if(! get_jpeg_size((uint8_t*)gamePtr->jpgFile.data, gamePtr->jpgFile.len, &gamePtr->jpg_width, &gamePtr->jpg_height) ) {
    Serial.printf("Artwork size guessing failed for %s, not a jpeg?\n", gamePtr->romname);
    return false;
  }
  // adjust zoom and offsets according to image size, will either fit height or width
  gamePtr->adjustLayout(BGSprite.width(), BGSprite.height());

  gamePtr->zoomx = gamePtr->view.innerbox.w/GWFBBox.w;
  gamePtr->zoomy = gamePtr->view.innerbox.h/GWFBBox.h;

  //Serial.printf("[%-12s] ", gamePtr->romname);
  gamePtr->ppa_tft = new PPASrm(&tft);
  gamePtr->ppa_fb  = new PPASrm(&BGSprite, false, false);

  // pre-render the jpg file
  gamePtr->bgImage = new GWImage
  {
    &gamePtr->jpgFile,
    GWImage::JPG,
    gamePtr->jpg_width,
    gamePtr->jpg_height,
  };

  gamePtr->preloaded = true;

  return gamePtr->romFile.data && gamePtr->romFile.len>0 && gamePtr->jpgFile.data && gamePtr->jpgFile.len>0;
}


void preload_games()
{
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

    if(!boot_game( gamePtr ))
      continue;

    // compose/cache background image + game frame, and print a thumbnail to the display
    composeArtwork(gamePtr, &tft, tft.width()/2, tft.height()/2, .25f);

    GWGames.push_back( gamePtr );

  }
  gamesCount = GWGames.size();

  int after = ESP.getFreePsram();
  Serial.printf("PSRAM Available: %d (allocated=%.1f MB)\n", after, float(before-after)/(1024.0*1024.0) );
  tft.setCursor(tft.width()/4, tft.height()*.8);
  tft.printf("    Found %d games \n", GWGames.size() );

  lgfx::delay(500);
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

  if(!boot_game(gamePtr)) {
    ESP_LOGE(GW_TAG, "ROM %s failed to load", gamePtr->romname);
    return false;
  }

  ESP_LOGI(GW_TAG, "Loaded Game: '%s' (rom=%s, fbsize=[%dx%d])", gamePtr->name, gamePtr->romname, (int)gamePtr->view.innerbox.w, (int)gamePtr->view.innerbox.h);
  currentGame = gameid;

  if( rtc_ok ) {
    Tab5Rtc.writeRAM(RTC_MEM_GAMEID_ADDR, currentGame); // save "currentGame" to RTC memory
  }

  // Debug touch buttons and view. TODO: move this out
  if( debug_buttons ) {
    kbd_debugger.printKeyboard(gw_keyboard);
    for( int i=0;i<gamePtr->btns_count;i++)
      drawBounds(&BGSprite, &(gamePtr->btns[i]));
    BGSprite.drawRect(gamePtr->view.innerbox.x-1, gamePtr->view.innerbox.y-1, gamePtr->view.innerbox.w+2, gamePtr->view.innerbox.h+2, TFT_RED);
  }

  return true;
}


void updateVolume()
{
  drawVolumeBox();
  if( rtc_ok )
    if( Tab5Rtc.writeRAM(RTC_MEM_VOLUME_ADDR, (uint8_t*)&volume, sizeof(volume)) !=  sizeof(volume)) { // 2 bytes
      ESP_LOGE(GW_TAG, "Unable to write %d bytes to RTC ram at address %d", sizeof(volume), RTC_MEM_VOLUME_ADDR );
    }
  ESP_LOGD(GW_TAG, "Volume changed to: %d", volume);
  audio_enabled = volume > 0;
}

void updateBrightness()
{
  drawBrightnessBox();
  tft.setBrightness(brightness);
  if( rtc_ok )
    if( Tab5Rtc.writeRAM(RTC_MEM_BRIGHT_ADDR, (uint8_t*)&brightness, sizeof(brightness)) != sizeof(brightness)) { // 1 byte
      ESP_LOGE(GW_TAG, "Unable to write %d bytes to RTC ram at address %d", sizeof(brightness), RTC_MEM_BRIGHT_ADDR );
    }
  ESP_LOGD(GW_TAG, "Brightness changed to: %d", brightness);
}


void handle_option_button(int state)
{
  #define CASE_OPTMENU(opt, optname) \
    if(!in_options_menu) { \
      break; \
    } \
    opt = !opt; \
    ESP_LOGI(GW_TAG, "%sabled %s", opt  ?"en":"dis", optname  ); \
    break; \

  switch(state) {
    case GW_BUTTON_UP:
      if( volume == 0 )
        volume = volume_increment-1;
      else
        volume += volume_increment;
      if( volume>volume_max )
        volume = volume_max;
      updateVolume();
    break;
    case GW_BUTTON_DOWN:
      if( volume>= volume_increment )
        volume -= volume_increment;
      else
        volume=0;
      updateVolume();
    break;
    case GW_BUTTON_BRIGHT: // bright, not right!
      if( brightness == 0 )
        brightness = brightness_increment-1;
      else
        brightness += brightness_increment;
      if( brightness>brightness_max )
        brightness = brightness_max;
      updateBrightness();
    break;
    case GW_BUTTON_DIM:
      if( brightness - brightness_increment > 0 )
        brightness -= brightness_increment;
      else
        brightness = 1; // don't turn display off by setting brightness to 0
      updateBrightness();
    break;

    case GW_BUTTON_POWEROFF:
      ESP_LOGD(GW_TAG, "Powering off");
      M5.Power.powerOff();
    break;
    // gear icon, show/hide options menu
    case GW_BUTTON_PREFS         : in_options_menu = !in_options_menu; ESP_LOGI(GW_TAG, "%sabled %s", in_options_menu?"en":"dis", "option menu"); break;
    // those are ignored when in_options_menu=false
    case GW_BUTTON_TOGGLE_AUDIO  : CASE_OPTMENU(audio_enabled, "audio");
    case GW_BUTTON_TOGGLE_USB_HID: CASE_OPTMENU(usbhost_enabled, "usbhost");
    case GW_BUTTON_TOGGLE_FPS    : CASE_OPTMENU(show_fps, "fps");
    case GW_BUTTON_TOGGLE_DEBUG  : CASE_OPTMENU(debug_buttons, "debug");

    default:
    break;
  }
  #undef CASE_OPTMENU
}


int handle_menu_gesture()
{
  M5.Speaker.stop(); // stop audio

  uint32_t now = millis();

  draw_menu(&tft);

  ESP_LOGD(GW_TAG, "spent %d ms drawing menu", millis()-now);

  auto gamePtr = GWGames[currentGame];

  gamesCount = GWGames.size();

  if( gamesCount == 0 ) {
    // TODO: filesystem picker
    return 0;
  }

  static m5::touch_state_t prev_state;
  static int32_t flickx = -1;
  GWTouchButton* GAWButton;
  int state = 0;
  int flick_dir = 0;
  int gameOffset = 0;
  std::vector<String> controlPorts = {/*"GAW",*/"GameA","GameB","Time","Alarm","ACL"};

  uint8_t keycodeQueued;
  int last_keycode_usb = 0;
  m5::touch_detail_t touchDetail;
  int transition_direction = 0;

  while(1)
  {
    vTaskDelay(1);

    // got any queued HID signal to handle?
    if( usbhost_enabled ) {
      if( xQueueReceive( usbHIDQueue, &( keycodeQueued ), 0) == pdPASS ) {
        keycode_usb = keycodeQueued;
        if( keycode_usb != 0 ) {
          last_keycode_usb = keycode_usb;
          goto _continue;
        }
        // WARNING: that logic does not handle multiple keypress
        if( keycode_usb == 0 ) { // a key or gamepad button was released
          switch(last_keycode_usb) {
            case KEY_ESC     : return 0; break; // ESC: GAW, leave menu
            case ARROW_LEFT  : goto _load_prev_game;
            case ARROW_RIGHT : goto _load_next_game;
            case ARROW_DOWN  : handle_option_button(GW_BUTTON_DOWN); goto _continue;
            case ARROW_UP    : handle_option_button(GW_BUTTON_UP);   goto _continue;
            case KEY_TIME    : handle_option_button(GW_BUTTON_TIME); goto _continue; // F7: Time
            case BUTTON_1    : break; // 0x2b, // TAB: Jump/Fire/Action
            case KEY_F1      : handle_option_button(GW_BUTTON_TOGGLE_AUDIO);   goto _continue; // 0x3a,
            case KEY_F2      : handle_option_button(GW_BUTTON_TOGGLE_USB_HID); goto _continue; // 0x3b,
            case KEY_F4      : handle_option_button(GW_BUTTON_TOGGLE_FPS);     goto _continue; // 0x3d,
            case KEY_F5      : handle_option_button(GW_BUTTON_TOGGLE_DEBUG);   goto _continue; // 0x3e,
            case KEY_F6      : handle_option_button(GW_BUTTON_PREFS);          goto _continue; // 0x3f,
            case KEY_ALARM   : break; // 0x3d, // F4: Alarm
            case KEY_ACL     : break; // 0x3e  // F5: ACL
            case KEYCODE_NONE: break;
          }

          if( last_keycode_usb == KEY_F6 )
            transition_direction = in_options_menu ? 1 : -1;
          else
            transition_direction = 0;
          goto _draw_menu;
        }
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
        if( debug_buttons ) {
          debugPrintButton(&OptionButtons[i], "Touched");
        }
        handle_option_button(state);
        transition_direction = 0;

        switch(state) {
          case GW_BUTTON_PREFS: transition_direction = in_options_menu ? 1 : -1; break;
          case GW_BUTTON_DIM:    // brightness down: don't redraw menu
          case GW_BUTTON_BRIGHT: // brightness up: don't redraw menu
          case GW_BUTTON_DOWN:   // volume down: don't redraw menu
          case GW_BUTTON_UP:     // volume up: don't redraw menu
            goto _continue; break;
          default: transition_direction = 0;
        }
        goto _draw_menu;
      }
    }

    // check if it was the "Game&Watch" button
    GAWButton = GWGames[currentGame]->getButtonByLabel("GAW");
    if( GAWButton ) {
      state = GAWButton->getHWState(touchDetail.x, touchDetail.y);
      if(state !=0) {
        pushSpriteTransition(&tft, &BGSprite, 0); // send background without the menu decoration
        return 0; // exit game selector
      }
    }

    // check if it was the left side or the right side of the screen
    if(touchDetail.x>0 && touchDetail.x<BGSprite.width()/2)
      goto _load_prev_game;
    else if( touchDetail.x>0 && touchDetail.x>BGSprite.width()/2)
      goto _load_next_game;

    _continue:
    if( debug_gestures )
      debugTouchState(touchDetail.state);
    continue;

    _load_next_game:
      // TODO: animate right
      invert_box(&tft, &FGSprite, {1183, 290, 80, 110});
      gameOffset = 1;
      transition_direction = 1;
      goto _load_game;

    _load_prev_game:
      // TODO: animate left
      invert_box(&tft, &FGSprite, {16, 290, 80, 110});
      gameOffset = gamesCount-1;
      transition_direction = -1;
      goto _load_game;

    _load_game:
      now = millis();
      drawImageAt(&tft, &IconLoading, tft.width()/2-IconLoading.width*2, tft.height()/2-IconLoading.height*2, 0, 4.0, 4.0);
      ESP_LOGD(GW_TAG, "spent %d ms rendering icon", millis()-now);
      now = millis();
      currentGame += gameOffset;
      currentGame = currentGame%gamesCount;
      if(!load_game(currentGame)) {
        if( gameOffset == 1 )
          goto _load_next_game;
        else
          goto _load_prev_game;
      }
      gamePtr = GWGames[currentGame];
      drawBackground(gamePtr, &BGSprite);
      ESP_LOGD(GW_TAG, "spent %d ms loading game", millis()-now);

    _draw_menu:
      now = millis();
      draw_menu(&tft,  transition_direction);
      ESP_LOGD(GW_TAG, "spent %d ms rendering menu", millis()-now);

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
  if( usbhost_enabled && xQueueReceive( usbHIDQueue, &( keycodeQueued ), 0) == pdPASS ) {
    keycode_usb = keycodeQueued;
    // WARNING: that logic does not handle multiple keypress
    if( keycode_usb == 0 ) {
      ESP_LOGV(GW_TAG, "Received keyrelase");
      return 0;
    }
    ESP_LOGV(GW_TAG, "Received keypress (keycode=0x%x)", keycode_usb);
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
        if(xt>0 && yt>0) // action was triggered by touch
          while(tft.getTouch(&xt, &yt)) // wait for touch release
            M5.delay(1);

        onBeforeGameCycles = []()
        {
          handle_menu_gesture();
        };
        onAfterGameCycles  = gw_set_time;

        return 0;
      case -1:
        onBeforeGameCycles = []()
        {
          gw_system_reset();
        };
        onAfterGameCycles  = gw_set_time;
        return 0;
      case 0:
        continue;
      default:
        return state;
    }
  }
  return 0;
}



void gameCycle()
{
  if( onBeforeGameCycles ) {
    onBeforeGameCycles();
    onBeforeGameCycles = nullptr;
  }

  auto diff_cycles_run = gw_system_run(GW_SYSTEM_CYCLES);

  // sometimes less than GW_SYSTEM_CYCLES have been consumed
  if( diff_cycles_run != 0 ) { // TODO: adjust timers using that value
    ESP_LOGV("GW_CYCLES", "diff: %d cycles", diff_cycles_run );
  }

  if( onAfterGameCycles ) {
    onAfterGameCycles();
    onAfterGameCycles = nullptr;
    if( audio_enabled ) {
      xQueueReset(audioBufferQueue);
    }

  } else {

    if( audio_enabled ) {
      static int audioBufferPos = 0;
      // transfer audio to buffer pool
      for (size_t i = 0; i < GW_AUDIO_BUFFER_LENGTH; i++) {
        bufferPool[audioBufferPos].data[i] = (gw_audio_buffer[i] > 0) ? volume : 0;
      }
      // send pool buffer index to queue to signal it's ready to be played
      xQueueSend(audioBufferQueue, (void*)&audioBufferPos, portMAX_DELAY);
      audioBufferPos++;
      audioBufferPos = audioBufferPos%AUDIO_POOL_SIZE;
    }
  }

  gw_audio_buffer_copied = true;
}



void gameLoop(bool skip_frame=false)
{
  gameCycle();

  static uint32_t loop_counter = 0;

  loop_counter++;

  if( loop_counter<blitCount ) {
    return;
  }

  loop_counter = 0;

  if( skip_frame )
    return;

  auto gamePtr = GWGames[currentGame];

  if( gamePtr->ppa_tft->available() ) {

    gw_system_blit(gw_fb);
    handleFps();

    auto invy = tft.height()-(gamePtr->view.innerbox.y+gamePtr->view.innerbox.h-1); // translation for PPA_SRM: position y is calculated from the bottom
    if( gamePtr->ppa_tft->pushImageSRM(gamePtr->view.innerbox.x-1, invy, 0, 0, 0, gamePtr->zoomx, gamePtr->zoomy, GW_SCREEN_WIDTH, GW_SCREEN_HEIGHT, gw_fb, true) )
      fpsCounter.addFrame();
  }
}



void audioTask(void*param)
{
  auto spk_cfg = M5.Speaker.config();

  spk_cfg.sample_rate = GW_AUDIO_FREQ;
  spk_cfg.stereo = false;
  spk_cfg.task_pinned_core = xPortGetCoreID(); // attach to the same core

  M5.Speaker.config(spk_cfg);

  if(! M5.Speaker.begin() ) {
    audio_enabled = false;
    audio_ready = true; // don't block main task
    vTaskDelete(NULL);
    return;
  }
  M5.Speaker.stop();
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

  const uint64_t first_us_tick = micros();
  const double us_per_cycles = (1000.0f*1000.0f*GW_SYSTEM_CYCLES)/float(GW_SYS_FREQ);

  while(1) {
    uint64_t us_now = micros();
    double us_abs_elapsed = us_now-first_us_tick; // absolute elapsed time (microseconds) since task begun
    double us_cycles_spent = fmod(us_abs_elapsed, us_per_cycles); // microseconds spent in the current time budget
    double us_remaining = us_per_cycles-us_cycles_spent; // microseconds remaining in the current time budget
    if( us_remaining > 1000.0f ) {
      auto ticks_remaining = pdMS_TO_TICKS(us_remaining/1000.0f);
      if( ticks_remaining>0 ) {
        vTaskDelay(ticks_remaining);
        us_remaining = fmod(us_remaining, 1000.0f);
      }
    }
    if( us_remaining >= 1.0f ) {
      delayMicroseconds(us_remaining);
    }
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


  bootAnimation();

  {
    if( usbhost_enabled ) {
      xTaskCreatePinnedToCore(
        usbHostTask,   // function
        "usbHostTask", // name
        16384,      // stack size
        NULL,       // parameter
        1,          // priority
        NULL,       // handle
        (portNUM_PROCESSORS-1)-xPortGetCoreID() // attach to the other core
      );
    }


    if( audio_enabled ) {
      audioBufferQueue = xQueueCreate(AUDIO_POOL_SIZE, sizeof(int *));
      // spawn subtask for audio
      xTaskCreatePinnedToCore(
        audioTask,   // function
        "audioTask", // name
        8192,      // stack size
        NULL,       // parameter
        1,         // priority
        NULL,       // handle
        (portNUM_PROCESSORS-1)-xPortGetCoreID() // attach to the other core
      );
      while(!audio_ready) {
        vTaskDelay(1);
      }
    }
  }


  {
    initSDCard();
    initLittleFS();
    initRTC();
  }

  {
    ESP_LOGI(GW_TAG, "Emulator output dimensions:         %dx%d", GW_SCREEN_WIDTH, GW_SCREEN_HEIGHT);

    ESP_LOGI(GW_TAG, "USB-HID host:                       %sabled", usbhost_enabled?"en":"dis");
    ESP_LOGI(GW_TAG, "Builtin Audio:                      %sabled", audio_enabled?"en":"dis");
    ESP_LOGI(GW_TAG, "Builtin RTC module:                 %sabled", rtc_ok?"en":"dis");
    ESP_LOGI(GW_TAG, "Gyro/accel IMU:                     %sabled", M5.Imu.isEnabled()?"en":"dis");
    ESP_LOGI(GW_TAG, "SDCard:                             %sabled", sdcard_ok?"en":"dis");
    ESP_LOGI(GW_TAG, "LittleFS:                           %sabled", littlefs_ok?"en":"dis");

    ESP_LOGI(GW_TAG, "Buttons debugging:                  %sabled", debug_buttons?"en":"dis");
    ESP_LOGI(GW_TAG, "Touch gestures debugging:           %sabled", debug_gestures?"en":"dis");
    ESP_LOGI(GW_TAG, "FPS counter:                        %sabled", show_fps?"en":"dis");
  }


  {
    if( rtc_ok ) {
      uint16_t ram_volume;
      if( Tab5Rtc.readRAM(RTC_MEM_VOLUME_ADDR, (uint8_t*)&ram_volume, sizeof(ram_volume)) )
        if( ram_volume<=volume_max )
          volume = ram_volume;

      uint8_t ram_brightness;
      if( Tab5Rtc.readRAM(RTC_MEM_BRIGHT_ADDR, (uint8_t*)&ram_brightness, sizeof(ram_brightness)) ) {
        if( ram_brightness<=brightness_max && ram_brightness>0 )
          brightness = ram_brightness;
        else
          ESP_LOGE(GW_TAG, "Invalid RTC ram value at address 0x3: %d", ram_brightness);
      } else {
        ESP_LOGE(GW_TAG, "Unable to read RTC ram at address 0x3");
      }

    }

    tft.setBrightness(brightness);

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
  }

  {
    allocSprite("Game&Watch emulator layer", &GWSprite, GW_SCREEN_WIDTH, GW_SCREEN_HEIGHT);
    gw_fb = (uint16_t*)GWSprite.getBuffer(); // attach sprite buffer to emulator for rendering

    { // FPS text layer
      FPSSprite.setPsram(true);
      if(! FPSSprite.createSprite(250, 64) ) {
        Serial.println("Failed to create FPSSprite, halting");
        while(1);
      }
      FPSSprite.setTextDatum(TL_DATUM);
      FPSSprite.setFont(&FreeSansBold24pt7b);
      FPSSprite.setTextColor(TFT_WHITE, TFT_BLACK);
      FPSSprite.setTextSize(1);
    }

    { // sprite for stylized text boxes
      textBox.setPsram(true);
      if(!textBox.createSprite(tft.width()/3, tft.height()/3)) {
        Serial.println("Failed to create textBox Sprite, halting");
        while(1);
      }
      textBox.setFont(&FreeSansBold24pt7b);
    }

    { // top icons menu mask
      MenuOptionsMask.setPsram(true);
      if( !MenuOptionsMask.createSprite( tft.width(), 128 ) ) {
        Serial.println("Failed to create menu mask Sprite, halting");
        while(1);
      }
    }

    // compositor/blender for game menu and game background
    allocSprite("Background+Emulator layer", &BGSprite,    tft.width(), tft.height());
    allocSprite("Mask+Menu layer",           &FGSprite,    tft.width(), tft.height());
    allocSprite("Composer+Blender output",   &BlendSprite, tft.width(), tft.height());

    ppa_srm   = new PPASrm(&BGSprite, false, false);
    ppa_blend = new PPABlend(&BlendSprite, false, false );
  }


  if( GWGames.size() == 0 ) {
    preload_games();
  }


  static auto gw_alloc = [](size_t size) -> void(*)
  {
    return heap_caps_aligned_alloc(16, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  };

  gw_set_malloc( gw_alloc ); // set G&W emulator to use custom allocator (psram, 16 bytes aligned)

  if( rtc_ok ) {
    // load last game id from RTC RAM
    uint8_t savedGame = Tab5Rtc.readRAM(RTC_MEM_GAMEID_ADDR);
    if(savedGame<gamesCount)
      currentGame = savedGame;
  }

  // procrastinate game launch
  onBeforeGameCycles = []()
  {
    if(! load_game(currentGame) ) {
      tft_error("Game loading failed, Halting");
      while(1);
    }
    auto gamePtr = GWGames[currentGame];
    drawBackground(gamePtr, &BGSprite);
    pushSpriteTransition(&tft, &BGSprite, 0);
    onAfterGameCycles = gw_set_time; // sync Game&Watch time with system time
  };

  const uint64_t first_us_tick = micros();
  uint64_t last_us_tick = micros();
  const double us_per_cycles = (1000.0f*1000.0f*GW_SYSTEM_CYCLES)/float(GW_SYS_FREQ);

  while(1) {
    uint64_t us_now = micros();
    double us_abs_elapsed = us_now-first_us_tick; // absolute elapsed time (microseconds) since task begun
    double us_rel_elapsed = us_now-last_us_tick;  // relative elapsed time (microseconds) since last iteration
    double us_cycles_spent = fmod(us_abs_elapsed, us_per_cycles); // microseconds spent in the current time chunk
    double us_remaining = us_per_cycles-us_cycles_spent; // microseconds remaining in the current time budget

    if( ( last_us_tick != first_us_tick ) && ( us_rel_elapsed > us_per_cycles*blitCount ) ) { // likely to happen when ppa is disabled
      // elapsed time since last loop exceeds full emulation cycles time
      ESP_LOGV("GW", "GameTask is %.2f usec behind schedule", us_rel_elapsed-us_per_cycles*blitCount);
    }

    last_us_tick = us_now; // store for next loop iteration

    if( us_remaining > 1000.0f ) {
      auto ticks_remaining = pdMS_TO_TICKS(us_remaining/1000.0f);
      if( ticks_remaining>0 ) {
        vTaskDelay(ticks_remaining);
        us_remaining = fmod(us_remaining, 1000.0f);
      }
    }
    if( us_remaining >= 1.0f ) {
      delayMicroseconds(us_remaining);
    }

    gameLoop(/*skip_frame*/);
  }

}



void setup()
{
  Serial.begin(/*115200*/);
  Serial.printf("Game&Watch emulator (sys freq=%dKHz, refresh rate=%dfps, audiobuffer=%d bytes, cycles per loop=%d, us_per_cycle=%f, ms_per_cycles=%f)\n",
    GW_SYS_FREQ,
    GW_REFRESH_RATE,
    GW_AUDIO_BUFFER_LENGTH,
    GW_SYSTEM_CYCLES,
    us_per_cycle,
    ms_per_cycles
  );

  // the task that will spawn all tasks and run the game loop in a timely fashion
  xTaskCreatePinnedToCore(
    gameTask,   // function
    "gameTask", // name
    16384,      // stack size
    NULL,       // parameter
    1,          // priority
    NULL,       // handle
    xPortGetCoreID() // "this" core
  );
}



void loop()
{
  vTaskDelete(NULL);
}

