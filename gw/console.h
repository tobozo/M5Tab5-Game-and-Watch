#pragma once

extern "C"
{ // emulator
  #include "../src/gw_system.h"    // imported from https://github.com/bzhxx/LCD-Game-Emulator
  #include "../src/gw_romloader.h" // imported from https://github.com/bzhxx/LCD-Game-Emulator
  unsigned char *ROM_DATA;
  unsigned int ROM_DATA_LENGTH;
}

// multi rom manager
#include "./gw_types.h"     // objects for the game menu manager
#include "./gw_layouts.hpp" // different game layouts (background, buttons)
#include "./gw_games.hpp"   // game catalog (generated code)
