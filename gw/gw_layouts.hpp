/*\
 *
 * M5Stack Tab5 Game&Watch Emulator
 *
 * A speculation brought to you by tobozo, copyleft (c+) 2025
 *
\*/
#pragma once

#include "./gw_types.h"
#include "./console.h"

extern GWImage IconSaveRTC;
extern GWImage IconDebug  ;
extern GWImage IconFPS    ;
extern GWImage IconGamepad;
extern GWImage IconMute   ;
extern GWImage IconPPA    ;
extern GWImage IconPrefs  ;
extern GWImage IconSndM   ;
extern GWImage IconSndP   ;

namespace GW
{

  /*
   * NOTE: All coordinates need to be normalized.
   *
   * */

  // HID Keycodes for USB support
  enum HID2GWKeycodes_t
  {
    KEYCODE_NONE = 0,
    ARROW_LEFT   = 0x50,
    ARROW_DOWN   = 0x51,
    ARROW_RIGHT  = 0x4f,
    ARROW_UP     = 0x52,
    BUTTON_1     = 0x2b, // TAB: Jump/Fire/Action
    KEY_ESC      = 0x29, // ESC: GAW
    KEY_F1       = 0x3a, // F1: Game A
    KEY_F2       = 0x3b, // F2: Game B
    KEY_F3       = 0x3c, // F3: reserved
    KEY_F4       = 0x3d, // F4: reserved
    KEY_F5       = 0x3e, // F5: reserved
    KEY_F6       = 0x3f, // F6: reserved
    KEY_TIME     = 0x40, // F7: Time
    KEY_ALARM    = 0x41, // F8: Alarm
    KEY_ACL      = 0x42  // F9: ACL
  };

  // extra buttons functionalities for menu routing
  #define GW_BUTTON_ACL            -1 // ACL Button performs reset
  #define GW_BUTTON_GAW            -2 // An extra button over the "Game&Watch" logo
  #define GW_BUTTON_TOGGLE_AUDIO   -3 // audio_enabled
  #define GW_BUTTON_TOGGLE_USB_HID -4 // usbhost_enabled
  #define GW_BUTTON_TOGGLE_PPA     -5 // ppa_enabled
  #define GW_BUTTON_TOGGLE_FPS     -6 // show_fps
  #define GW_BUTTON_TOGGLE_DEBUG   -7 // debug_buttons
  #define GW_BUTTON_PREFS          -8 // in_options_menu
  #define GW_BUTTON_NONE         -999 // do nothing


  GWTouchButton BtnOptionPPA  (/*🚀*/"HW Accel",  400,    8,    0,  0, GW_BUTTON_TOGGLE_PPA,     KEY_F3,       true, &IconPPA);     // ppa_enabled
  GWTouchButton BtnOptionTime (/*🕑*/"Set Time",  525,    8,  100, 90, GW_BUTTON_TIME,           KEY_TIME,     true, &IconSaveRTC); // save game time to rtc
  GWTouchButton BtnOptionSnd  (/*🔇*/"Mute",      650,    8,  100, 90, GW_BUTTON_TOGGLE_AUDIO,   KEY_F1,       true, &IconMute);    // audio_enabled
  GWTouchButton BtnOptionJoy  (/*🕹*/"USB-HID",   775,    8,  100, 90, GW_BUTTON_TOGGLE_USB_HID, KEY_F2,       true, &IconGamepad); // usbhost_enabled
  GWTouchButton BtnOptionFPS  (/*🎞*/"Show FPS",  900,    8,  100, 90, GW_BUTTON_TOGGLE_FPS,     KEY_F4,       true, &IconFPS);     // show_fps
  GWTouchButton BtnOptionDbg  (/*🐞*/"Debug",    1025,    8,  100, 90, GW_BUTTON_TOGGLE_DEBUG,   KEY_F5,       true, &IconDebug);   // debug_buttons
  GWTouchButton BtnOptionOpts (/*⚙*/"Options",  1163,    8,  100, 90, GW_BUTTON_PREFS,          KEY_F6,       true, &IconPrefs);   // toggle options menu
  GWTouchButton BtnOptionVolP (/*🔈*/"Vol-",      439,  611,  100, 90, GW_BUTTON_DOWN,           ARROW_DOWN,   true, &IconSndM);    // increase volume
  GWTouchButton BtnOptionVolM (/*🔈*/"Vol+",      739,  611,  100, 90, GW_BUTTON_UP,             ARROW_UP,     true, &IconSndP);    // decrease volume
  GWTouchButton BtnOptionTerm (/*🗂*/"term",        0,    0, 1280, 90, GW_BUTTON_NONE,           KEYCODE_NONE, true); // Touch propagation terminator (prevents bubbling)


  // UI Menu buttons (extra options, not related to Game&Watch UI)
  GWTouchButton OptionButtons[] = {
    BtnOptionVolP,
    BtnOptionVolM,
    BtnOptionTime,
    BtnOptionSnd,
    BtnOptionJoy,
    BtnOptionPPA,
    BtnOptionFPS,
    BtnOptionDbg,
    BtnOptionOpts,
    BtnOptionTerm
  };

  const size_t optionButtonsCount = sizeof(OptionButtons)/sizeof(GWTouchButton);

  // Wrappers around some option buttons used as switches
  GWToggleSwitch BtnOption( &BtnOptionOpts, &in_options_menu );
  GWToggleSwitch BtnDbg   ( &BtnOptionDbg,  &debug_buttons   );
  GWToggleSwitch BtnFps   ( &BtnOptionFPS,  &show_fps        );
  GWToggleSwitch BtnJoy   ( &BtnOptionJoy,  &usbhost_enabled );
  GWToggleSwitch BtnMute  ( &BtnOptionSnd,  &audio_enabled   );

  GWToggleSwitch toggleSwitches[] =
  {
    BtnOption,
    BtnDbg   ,
    BtnFps   ,
    BtnJoy   ,
    BtnMute
  };

  const size_t toggleSwitchesCount = sizeof(toggleSwitches)/sizeof(GWToggleSwitch);

  // volume box position and dimensions
  GWBox volumeBox = { 539, 661, 200, 48 };

  // Game&Watch single screen layouts, with their respective buttons layouts

  namespace SILVER_SCREEN
  {
    // Game/Time/Alarm/ACL bottom middle
    // logo top left
    // 2 different button layouts
    GWLayout SSLay = {.name="Silver Screen", .innerbox={.x=0.292134831461, .y=0.255555555556, .w=0.422, .h=0.4}};

    GWTouchButton SSGAW(   "SSGAW",   0.071161, 0.130556, 0.116105, 0.152778, GW_BUTTON_GAW, KEY_ESC); // nintendo logo
    GWTouchButton SSGameA( "SSGameA", 0.473783, 0.825000, 0.086142, 0.086111, GW_BUTTON_GAME, KEY_F1);
    GWTouchButton SSGameB( "SSGameB", 0.580524, 0.825000, 0.086142, 0.086111, GW_BUTTON_TIME, KEY_F2);
    GWTouchButton SSTime(  "SSTime",  0.685393, 0.825000, 0.086142, 0.086111, GW_BUTTON_B + GW_BUTTON_TIME, KEY_TIME);
    GWTouchButton SSAlarm( "SSAlarm", 0,        0,        0,        0,        GW_BUTTON_B + GW_BUTTON_GAME, KEYCODE_NONE); // silver screen has no alarm button
    GWTouchButton SSACL(   "SSACL",   0.316479, 0.847222, 0.033708, 0.044444, GW_BUTTON_ACL, KEY_ACL);
    // - 2 buttons, right and left
    namespace TWO_BUTTONS
    {
      GWTouchButton SS2Left(  "SS2Left",  0.076779, 0.638889, 0.108614, 0.161111, GW_BUTTON_LEFT, ARROW_LEFT);
      GWTouchButton SS2Right( "SS2Right", 0.816479, 0.638889, 0.108614, 0.161111, GW_BUTTON_RIGHT, ARROW_RIGHT);
    }
    // - 4 buttons, two vertical left, two vertical right
    namespace FOUR_BUTTONS
    {
      GWTouchButton SS4LeftUp(    "SS4LeftUp",    0.084270, 0.597222, 0.093633, 0.138889, GW_BUTTON_LEFT  + GW_BUTTON_UP, ARROW_LEFT);
      GWTouchButton SS4LeftDown(  "SS4LeftDown",  0.084270, 0.766667, 0.093633, 0.138889, GW_BUTTON_LEFT  + GW_BUTTON_DOWN, ARROW_UP);
      GWTouchButton SS4RightUp(   "SS4RightUp",   0.823970, 0.597222, 0.093633, 0.138889, GW_BUTTON_RIGHT + GW_BUTTON_UP, ARROW_RIGHT);
      GWTouchButton SS4RightDown( "SS4RightDown", 0.823970, 0.766667, 0.093633, 0.138889, GW_BUTTON_RIGHT + GW_BUTTON_DOWN, ARROW_DOWN);
    }
    using namespace TWO_BUTTONS;
    using namespace FOUR_BUTTONS;
  };



  namespace GOLD_SCREEN
  {
    // Game/Time/Alarm/ACL bottom middle
    // logo top left
    // 2 different button layouts
    GWLayout GSLay = {.name="Gold Screen", .innerbox={.x=0.284644, .y=0.238889, .w=0.434457, .h=0.427778}};

    GWTouchButton GSGAW(   "GSGAW",   0.065543, 0.133333, 0.121723, 0.152778, GW_BUTTON_GAW, KEY_ESC); // nintendo logo
    GWTouchButton GSGameA( "GSGameA", 0.479401, 0.819444, 0.086142, 0.091667, GW_BUTTON_GAME, KEY_F1);
    GWTouchButton GSGameB( "GSGameB", 0.588015, 0.819444, 0.086142, 0.091667, GW_BUTTON_TIME, KEY_F2);
    GWTouchButton GSTime(  "GSTime",  0.694757, 0.819444, 0.086142, 0.091667, GW_BUTTON_B + GW_BUTTON_TIME, KEY_TIME);
    GWTouchButton GSAlarm( "GSAlarm", 0.359551, 0.844444, 0.033708, 0.044444, GW_BUTTON_B + GW_BUTTON_GAME, KEY_ALARM);
    GWTouchButton GSACL(   "GSACL",   0.269663, 0.844444, 0.033708, 0.044444, GW_BUTTON_ACL, KEY_ACL);
    // - 2 buttons, right and left
    namespace TWO_BUTTONS
    {
      GWTouchButton GS2Left(  "GS2Left",  0.073801, 0.636111, 0.107011, 0.161111, GW_BUTTON_LEFT, ARROW_LEFT );
      GWTouchButton GS2Right( "GS2Right", 0.817343, 0.636111, 0.107011, 0.161111, GW_BUTTON_RIGHT, ARROW_RIGHT );
    }
    // - 4 buttons, two vertical left, two vertical right
    namespace FOUR_BUTTONS
    {
      GWTouchButton GS4LeftUp(    "GS4LeftUp",    0.084270, 0.597222, 0.093633, 0.138889, GW_BUTTON_LEFT  + GW_BUTTON_UP, ARROW_LEFT );
      GWTouchButton GS4LeftDown(  "GS4LeftDown",  0.084270, 0.766667, 0.093633, 0.138889, GW_BUTTON_LEFT  + GW_BUTTON_DOWN, ARROW_UP );
      GWTouchButton GS4RightUp(   "GS4RightUp",   0.823970, 0.597222, 0.093633, 0.138889, GW_BUTTON_RIGHT + GW_BUTTON_UP, ARROW_RIGHT );
      GWTouchButton GS4RightDown( "GS4RightDown", 0.823970, 0.766667, 0.093633, 0.138889, GW_BUTTON_RIGHT + GW_BUTTON_DOWN, ARROW_DOWN );
    }
    using namespace TWO_BUTTONS;
    using namespace FOUR_BUTTONS;
  };


  namespace WIDE_SCREEN
  {
    // Game/Time/Alarm/ACL top right
    // logo top left
    // 3 different button layouts
    GWLayout WSLay = {.name="Wide Screen", .innerbox={.x=0.267112, .y=0.25, .w=0.48, .h=0.5}};

    GWTouchButton WSGAW(   "WSGAW",   0.043406, 0.158333, 0.123539, 0.172222, GW_BUTTON_GAW, KEY_ESC); // nintendo logo
    GWTouchButton WSGameA( "WSGameA", 0.834725, 0.097222, 0.071786, 0.077778, GW_BUTTON_GAME, KEY_F1);
    GWTouchButton WSGameB( "WSGameB", 0.834725, 0.211111, 0.071786, 0.077778, GW_BUTTON_TIME, KEY_F2);
    GWTouchButton WSTime(  "WSTime",  0.834725, 0.327778, 0.071786, 0.077778, GW_BUTTON_B + GW_BUTTON_TIME, KEY_TIME);
    GWTouchButton WSAlarm( "WSAlarm", 0.928214, 0.172222, 0.023372, 0.038889, GW_BUTTON_B + GW_BUTTON_GAME, KEY_ALARM);
    GWTouchButton WSACL(   "WSACL",   0.928214, 0.288889, 0.023372, 0.038889, GW_BUTTON_ACL, KEY_ACL);
    // - 2 buttons, right and left
    namespace TWO_BUTTONS
    {
      GWTouchButton WS2Left(  "WS2Left",  0.061770, 0.630556, 0.103506, 0.166667, GW_BUTTON_LEFT, ARROW_LEFT );
      GWTouchButton WS2Right( "WS2Right", 0.834725, 0.630556, 0.103506, 0.166667, GW_BUTTON_RIGHT, ARROW_RIGHT );
    }
    // - 3 buttons, one left, two vertical right
    namespace THREE_BUTTONS
    {
      GWTouchButton WS3Button1("WS3Button1", 0.058431, 0.630556, 0.103506, 0.166667, GW_BUTTON_A, BUTTON_1 );
      GWTouchButton WS3Up(     "WS3Up",      0.841402, 0.558333, 0.083472, 0.138889, GW_BUTTON_UP, ARROW_UP );
      GWTouchButton WS3Down(   "WS3Down",    0.841402, 0.733333, 0.083472, 0.138889, GW_BUTTON_DOWN, ARROW_DOWN );
    }
    // - 4 buttons, two vertical left, two vertical right
    namespace FOUR_BUTTONS
    {
      GWTouchButton WS4LeftUp(    "WS4LeftUp",    0.073456, 0.608333, 0.080134, 0.133333, GW_BUTTON_LEFT  + GW_BUTTON_UP, ARROW_LEFT );
      GWTouchButton WS4LeftDown(  "WS4LeftDown",  0.073456, 0.772222, 0.080134, 0.133333, GW_BUTTON_LEFT  + GW_BUTTON_DOWN, ARROW_UP );
      GWTouchButton WS4RightUp(   "WS4RightUp",   0.839733, 0.608333, 0.080134, 0.133333, GW_BUTTON_RIGHT + GW_BUTTON_UP, ARROW_RIGHT );
      GWTouchButton WS4RightDown( "WS4RightDown", 0.839733, 0.772222, 0.080134, 0.133333, GW_BUTTON_RIGHT + GW_BUTTON_DOWN, ARROW_DOWN );
    }
    using namespace TWO_BUTTONS;
    using namespace THREE_BUTTONS;
    using namespace FOUR_BUTTONS;
  };


  namespace NEW_WIDE_SCREEN
  {
    // Game/Time/Alarm/ACL top right
    // logo top left
    // 4 different button layouts
    GWLayout NWSLay = {.name="New Wide Screen", .innerbox={.x=0.264214046823, .y=0.25, .w=0.471571906355, .h=0.497222222222}};

    GWTouchButton NWSGAW(   "NWSGAW",   0.051753, 0.144444, 0.123539, 0.175000, GW_BUTTON_GAW, KEY_ESC); // nintendo logo
    GWTouchButton NWSGameA( "NWSGameA", 0.834725, 0.097222, 0.071786, 0.077778, GW_BUTTON_GAME, KEY_F1);
    GWTouchButton NWSGameB( "NWSGameB", 0.834725, 0.211111, 0.071786, 0.077778, GW_BUTTON_TIME, KEY_F2);
    GWTouchButton NWSTime(  "NWSTime",  0.834725, 0.327778, 0.071786, 0.077778, GW_BUTTON_B + GW_BUTTON_TIME, KEY_TIME);
    GWTouchButton NWSAlarm( "NWSAlarm", 0.928214, 0.172222, 0.023372, 0.038889, GW_BUTTON_B + GW_BUTTON_GAME, KEY_ALARM);
    GWTouchButton NWSACL(   "NWSACL",   0.928214, 0.288889, 0.023372, 0.038889, GW_BUTTON_ACL, KEY_ACL);
    // - 2 buttons, right and left
    namespace TWO_BUTTONS
    {
      GWTouchButton NWS2Left(  "NWS2Left",  0.061770, 0.641667, 0.103506, 0.166667, GW_BUTTON_LEFT, ARROW_LEFT );
      GWTouchButton NWS2Right( "NWS2Right", 0.834725, 0.641667, 0.103506, 0.166667, GW_BUTTON_RIGHT, ARROW_RIGHT );
    }
    // - 3 buttons, two horizontal left, one right
    namespace THREE_BUTTONS
    {
      GWTouchButton NWS3Left(    "NWS3Left",    0.041736, 0.666667, 0.061770, 0.102778, GW_BUTTON_LEFT, ARROW_LEFT );
      GWTouchButton NWS3Right(   "NWS3Right",   0.130217, 0.666667, 0.061770, 0.102778, GW_BUTTON_RIGHT, ARROW_RIGHT );
      GWTouchButton NWS3Button1( "NWS3Button1", 0.834725, 0.641667, 0.103506, 0.166667, GW_BUTTON_A, BUTTON_1 );
    }
    // - 4 buttons, two vertical left, two vertical right
    namespace FOUR_BUTTONS
    {
      GWTouchButton NWS4LeftUp(    "NWS4LeftUp",    0.073456, 0.772222, 0.080134, 0.133333, GW_BUTTON_LEFT  + GW_BUTTON_UP, ARROW_LEFT);
      GWTouchButton NWS4LeftDown(  "NWS4LeftDown",  0.073456, 0.608333, 0.080134, 0.133333, GW_BUTTON_LEFT  + GW_BUTTON_DOWN, ARROW_UP);
      GWTouchButton NWS4RightUp(   "NWS4RightUp",   0.839733, 0.772222, 0.080134, 0.133333, GW_BUTTON_RIGHT + GW_BUTTON_UP, ARROW_RIGHT);
      GWTouchButton NWS4RightDown( "NWS4RightDown", 0.839733, 0.608333, 0.080134, 0.133333, GW_BUTTON_RIGHT + GW_BUTTON_DOWN, ARROW_DOWN);
    }
    // - 5 buttons, d-pad left, action right
    namespace FIVE_BUTTONS
    {
      GWTouchButton NWS5Up(      "NWS5Up",      0.090150, 0.608333, 0.046745, 0.077778, GW_BUTTON_UP, ARROW_UP );
      GWTouchButton NWS5Down(    "NWS5Down",    0.090150, 0.763889, 0.046745, 0.077778, GW_BUTTON_DOWN, ARROW_DOWN );
      GWTouchButton NWS5Left(    "NWS5Left",    0.043406, 0.686111, 0.046745, 0.077778, GW_BUTTON_LEFT, ARROW_LEFT );
      GWTouchButton NWS5Right(   "NWS5Right",   0.136895, 0.686111, 0.046745, 0.077778, GW_BUTTON_RIGHT, ARROW_RIGHT );
      GWTouchButton NWS5Button1( "NWS5Button1", 0.834725, 0.644444, 0.103506, 0.172222, GW_BUTTON_A+GW_BUTTON_UP, BUTTON_1 );
    }
    using namespace TWO_BUTTONS;
    using namespace THREE_BUTTONS;
    using namespace FOUR_BUTTONS;
    using namespace FIVE_BUTTONS;
  };


  namespace CRYSTAL_SCREEN
  {
    // Game/Time/Alarm/ACL top left
    // logo top right
    // only 1 button layout
    // However touch buttons are too tiny to be practical and the screen ratio
    // doesn't help, so we borrow the New Wide Screen layout
    using namespace NEW_WIDE_SCREEN;

    auto CSLay   = NWSLay;
    auto CSGAW   = NWSGAW;
    auto CSGame  = NWSGameA;
    auto CSTime  = NWSTime;
    auto CSAlarm = NWSAlarm;
    auto CSACL   = NWSACL;


    auto CS5Up      = NWS5Up;
    auto CS5Down    = NWS5Down;
    auto CS5Left    = NWS5Left ;
    auto CS5Right   = NWS5Right ;
    auto CS5Button1 = NWS5Button1;
  };

  using namespace SILVER_SCREEN;
  using namespace GOLD_SCREEN;
  using namespace WIDE_SCREEN;
  using namespace NEW_WIDE_SCREEN;
  using namespace CRYSTAL_SCREEN;

};

using namespace GW;
