# M5Tab5-Game-and-Watch

A game selection menu to run single screen Game&Watch roms on M5Stack Tab5 via [LCD Game Emulator](https://github.com/bzhxx/LCD-Game-Emulator).

<img width="512" alt="image" src="https://github.com/user-attachments/assets/b5cde28d-9204-441e-95f5-2984c5c87be3" />


## Supported Games:

Game Name                          | ROM Name
---------------------------------- | -----------
Ball                               | gnw_ball
Balloon Fight (Crystal)            | gnw_bfight
Chef                               | gnw_chef 
Climber (Crystal)                  | gnw_climber 
Donkey Kong Jr.                    | gnw_dkjr 
Donkey Kong Jr. (Panorama)         | gnw_dkjrp
Fire                               | gnw_fire 
Fire Attack                        | gnw_fireatk 
Fire                               | gnw_fires 
Flagman                            | gnw_flagman 
Helmet                             | gnw_helmet
Judge                              | gnw_judge 
Lion                               | gnw_lion
Manhole                            | gnw_manhole 
Manhole (Gold)                     | gnw_manholeg
Mario's Cement Factory             | gnw_mariocm
Mario's Cement Factory (Table-Top) | gnw_mariocmt
Mario the Juggler                  | gnw_mariotj
Mario's Bombs Away                 | gnw_mbaway
Mickey Mouse                       | gnw_mmouse
Mickey Mouse (Panorama)            | gnw_mmousep
Octopus                            | gnw_octopus
Parachute                          | gnw_pchute 
Popeye                             | gnw_popeye
Popeye (Panorama)                  | gnw_popeyep 
Super Mario Bros. (Crystal)        | gnw_smb 
Snoopy (Panorama)                  | gnw_snoopyp 
Snoopy Tennis                      | gnw_stennis 
Turtle Bridge                      | gnw_tbridge 
Tropical Fish                      | gnw_tfish 
Vermin                             | gnw_vermin

Those are single screen games, for DIY dual screen implementation see the [parent project](https://github.com/slowlane112/Esp32-Game-and-Watch).

## Features:

  - Overlayed menu to navigate between games
  - Automatic (landscape forced) screen orientation at boot using builtin IMU
  - Roms and artworks preloading (SD or LittleFS)
  - Volume and last game selection persistence to RTC memory
  - Adjust game time from Tab5 RTC module
  - Adjust Tab5 RTC module from game time
  - Builtin TouchScreen buttons
  - Support for USB SNES Gamepad (0810:e501)
  - Support for USB Keyboard
  - Support for gzipped roms/artworks e.g. `gzip -c gnw_parachute.gw > gnw_parachute.gw.gz`


## Wiring:
- None :)

## Roms

Even though the Tab5 runs at 400MHz (ESP32P4 rulezz), MAME original '.gw' roms are shipped with png/svg 
artworks that would cripple the performances and make the games unplayable.

Fortunately the roms can be shrunk using LCD-Game-Shrinker:

https://github.com/bzhxx/LCD-Game-Shrinker

Below is a link to a guide describing how to use LCD-Game-Shrinker to generate the shrunk roms.

https://gist.github.com/DNA64/16fed499d6bd4664b78b4c0a9638e4ef ([local copy](LCD-Game-Shrinker-Guide.md))

⚠️ Requirements from this guide are very tight with OS/software versions: Ubuntu 20.04, Inkscape 1.1, and the MAME rom archive 0.229 is ~68GB.

If using a SD card, put the shrunk rom files in the `/roms` folder, if using LittleFS, leave the files at the root.

```c
#define GW_ROMS_DIR     "/roms"     
```

## Artworks

Only one artwork image per game is required: the actual pic of the console as provided in the MAME rom package.
That image can be found in the "art packs" mentioned in the [LCD-Game-Shrinker guide](https://gist.github.com/DNA64/16fed499d6bd4664b78b4c0a9638e4ef)

That image will be stretched to fit the height of the display, but it must be a jpeg, and be of a reasonable size to avoid slow refresh rates.

Speed vs quality tradeoff: best results observed when resizing the image at half the height of the Tab5 display (360px) and saving the jpg at 80% quality.

Most of those images can be found in the MAME artwork files as PNG format, just resize and save as jpeg using the same filename as the rom.
e.g. for "Parachute" game, the jpeg filename will be `gnw_parachute.jpg` and the rom filename will be `gnw_parachute.gw`.

⚠️ "Table-Top Screen", "Panorama Screen" and "Crystal Screen" games artworks (e.g. Mario's Cement Factory, Snoopy) have button positions that can't be mapped 
efficiently on the Tab5 screen or a layout that can't fit in the landscape mode. In order to fix this problem, those games will have their button positions 
borrowed from the "New Wide Screen" layout instead. See the image templates in the `generic/` folder of this repository, just apply the same resize/save-as-jpg 
procedure.

If using a SD card, put the resized jpg files in the `/artworks` folder, if using LittleFS, leave the files at the root.

```c
#define GW_ARTWORKS_DIR "/artworks"
```

## Building

- Open Arduino IDE
- Install [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost) library and apply the [fix for M5Tab5](https://github.com/tanakamasayuki/EspUsbHost/issues/18)
- Clone those additional libraries (don't use the library manager, some of those are outdated in the registry): 
  - [M5GFX](https://github.com/M5Stack/M5GFX)
  - [M5Unified](https://github.com/M5Stack/M5Unified)
  - [ESP32-targz](https://github.com/tobozo/ESP32-targz)
  - [arduino-BufferStream](https://github.com/Industrial-Shields/arduino-BufferStream)
- Optional (if not using the SD card): select a partition scheme where SPIFFS size is between 1MB and 3MB
- Compile and flash
- Optional (if not using the SD card): use the sketch data upload utility to upload the roms/artworks


## Known bugs
- https://github.com/tanakamasayuki/EspUsbHost/issues/18
- https://github.com/Industrial-Shields/arduino-BufferStream/issues/1


## Credits/Acknowledgements
 - The LCD Game emulator engine -> https://github.com/bzhxx/LCD-Game-Emulator
 - This project was inspired by -> https://github.com/slowlane112/Esp32-Game-and-Watch
 - Awesome rom shrinker Tool -> https://github.com/bzhxx/LCD-Game-Shrinker
 - Rom shrinking Tutorial -> https://gist.github.com/DNA64/16fed499d6bd4664b78b4c0a9638e4ef
