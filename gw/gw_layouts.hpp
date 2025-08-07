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
    KEY_TIME     = 0x3c, // F3: Time
    KEY_ALARM    = 0x3d, // F4: Alarm
    KEY_ACL      = 0x3e  // F5: ACL
  };

  #define GW_BUTTON_ACL -1 // ACL Button performs reset
  #define GW_BUTTON_GAW -2 // An extra button over the "Game&Watch" logo

  // Game & Watch single screen layouts, with their respective buttons layouts


  // NWS : 560x360
  // GS:   464x308
  // SS:   443:283
  // WS:   560:360


  namespace SILVER_SCREEN
  {
    // Game/Time/Alarm/ACL bottom middle
    // logo top left
    // 2 different button layouts
    GWLayout SSLay = {.name="Silver Screen", .innerbox={.x=0.292135, .y=0.255556, .w=0.415730, .h=0.394444}};

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
    GWLayout WSLay = {.name="Wide Screen", .innerbox={.x=0.267112, .y=0.250000, .w=0.467446, .h=0.500000}};

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
    GWLayout NWSLay = {.name="New Wide Screen", .innerbox={.x=0.267112, .y=0.250000, .w=0.467446, .h=0.500000}};

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
