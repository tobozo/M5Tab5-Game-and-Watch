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
 *  - Battery level indicator
 *  - Auto sleep / screen dim / wake on touch
 *  - Joystick/Keyboard calibration
 *  - MAME ROM/Artwork importer+shrinker+packer
 *  - Hack: Use RX8130 FOUT pin as clock signal (pure 32Khz)
 *
 * */


// this "empty" file is needed by Arduino IDE but can be ignored by platformio
// see src/main.cpp file for the real stuff
