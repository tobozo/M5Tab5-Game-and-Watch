/*\
 *
 * M5Stack Tab5 Game&Watch Emulator
 *
 * A speculation brought to you by tobozo, copyleft (c+) 2025
 *
\*/
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



struct AudioBuffer
{
  uint16_t __attribute__((aligned(1024))) data[GW_AUDIO_BUFFER_LENGTH];
};
#define AUDIO_POOL_SIZE 16
static AudioBuffer bufferPool[AUDIO_POOL_SIZE];
QueueHandle_t audioBufferQueue;
static bool audio_ready = false;


// Emulator refresh rate (GW_REFRESH_RATE) and audio quality are tied, higher refresh rate = bigger audio buffer = better audio quality
// e.g. GW_REFRESH_RATE default value (128Hz) is unsustainable for the display.
// select a more realistic refresh rate. NOTE: real refresh rate will be approximated from this
static const uint32_t target_fps = 32; // must be a divisor of GW_REFRESH_RATE and a power of 2 while being sustainable by the display, not so many choices :)
// frames to skip e.g. if GW_REFRESH_RATE is 128Hz and target_fps is 16, blit every (128/16=8) frames instead of every frame
static const uint32_t blitCount = (GW_REFRESH_RATE/target_fps);
// microseconds per Game&Watch system cycle
static const double us_per_cycle = (1000.0f*1000.0f)/float(GW_SYS_FREQ);
// milliseconds per call to gw_system_run(GW_SYSTEM_CYCLES), one "game tick"
static const double ms_per_cycles = (us_per_cycle*GW_SYSTEM_CYCLES)/1000.0f;
// same as ms_per_cycles but in ticks
static const TickType_t ticks_per_game_cycles = pdMS_TO_TICKS(ms_per_cycles);
