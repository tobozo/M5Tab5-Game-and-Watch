/*\
 *
 * M5Stack Tab5 Game&Watch Emulator
 *
 * A speculation brought to you by tobozo, copyleft (c+) 2025
 *
\*/
#pragma once

#include "gw_types.h"

#include <SD_MMC.h>
#include <LittleFS.h>     // builtin
#include <BufferStream.h> // https://github.com/Industrial-Shields/arduino-BufferStream
#include <ESP32-targz.h>  // https://github.com/tobozo/ESP32-targz


#define GW_ROMS_DIR     "/roms"
#define GW_ARTWORKS_DIR "/artworks"


static fs::FS* gwFS = nullptr;//&LittleFS;


char *createPath(const char* dir, const char* basename, const char* extension)
{
  static char path[256]; // WARNING: LittleFS paths have 32 chars max
  snprintf(path, 255, "%s/%s.%s", dir, basename, extension );
  return path;
}


void initLittleFS()
{
  littlefs_ok = LittleFS.begin();
}


void initSDCard()
{
  // Serial.println("SD_MMC Start...");
  if (!SD_MMC.begin("/sdcard", false)) {  // false = 4-bit mode
    // Serial.println("SD_MMC 4bit mode not available");
    if (!SD_MMC.begin("/sdcard", true)) {  // true = 1-bit mode
      Serial.println("SD_MMC 1 bit mode not available");
      sdcard_ok = false;
      return;
    } else {
      // Serial.println("SD_MMC 1 bit mode available");
    }
  } else {
    // Serial.println("SD_MMC 4 bit mode available");
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("SD Card type unknown");
    sdcard_ok = false;
    return;
  }

  sdcard_ok = true;
  // Serial.print("SD Card type: ");
  // if (cardType == CARD_MMC) {
  //   Serial.print("MMC ");
  // } else if (cardType == CARD_SD) {
  //   Serial.print("SDSC ");
  // } else if (cardType == CARD_SDHC) {
  //   Serial.print("SDHC ");
  // } else {
  //   Serial.print("UNKNOWN ");
  // }
  //
  // uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024 * 1024);
  // Serial.println(", size: " + String(cardSize) + "GB");
}


static ReadBufferStream *gzStreamPtr; // for reading from memory
static WriteBufferStream *romStreamPtr; // for writing to memory

extern void tft_error(const char *fmt, ...);

extern void gzProgressCb(uint8_t progress);
static uint32_t gz_progress_len = 1;
static uint32_t gz_progress = 0;

// type: GzUnpacker::gzStreamWriter
static bool gzWriter( unsigned char* buff, size_t buffsize )
{
  if(!romStreamPtr)
    return false;
  if(!buff)
    return false;
  if( buffsize == 0 )
    return false;
  gz_progress += buffsize;
  if( gz_progress_len > 0 ) {
    float pg = (float(gz_progress)/float(gz_progress_len))*100.0f;
    gzProgressCb(pg);
  }
  return romStreamPtr->write(buff, buffsize);
};


bool game_is_loadable(GWGame* gamePtr)
{
  assert(gamePtr);
  if( gamePtr->romFile.path != nullptr && gamePtr->jpgFile.path != nullptr )
    return true;
  const char* romFolder = gwFS==&SD_MMC ? GW_ROMS_DIR     : "";
  const char* jpgFolder = gwFS==&SD_MMC ? GW_ARTWORKS_DIR : "";
  char *path;
  path = createPath(romFolder, gamePtr->romname, "gw.gz"); // look for gzipped version first
  if( !gwFS->exists(path) ) {
    path = createPath(romFolder, gamePtr->romname, "gw");
    if( !gwFS->exists(path) )
      return false;
  }
  gamePtr->romFile.setPath(path);
  path = createPath(jpgFolder, gamePtr->romname, "jpg.gz"); // look for gzipped version first
  if( !gwFS->exists(path) ) {
    path = createPath(jpgFolder, gamePtr->romname, "jpg");
    if( !gwFS->exists(path) )
      return false;
  }
  gamePtr->jpgFile.setPath(path);
  return true;
}


bool expand_asset(GWFile *asset, Stream* gzStream, uint32_t gzlen)
{
  assert(asset);
  assert(gzStream);

  if(gzlen==0) {
    Serial.println("gz size=0");
    return false;
  }

  unsigned char * gzdata = (unsigned char *)ps_malloc(gzlen+1);
  if(!gzdata) {
    tft_error("Malloc error (%d bytes)", (int)gzlen);
    return false;
  }

  // read gzfile into memory
  if( gzStream->readBytes((char*)gzdata, gzlen) != gzlen ) {
    tft_error("Size mismatch (expected %d bytes)", (int)gzlen);
    free(gzdata);
    return false;
  }

  // get uncompressed size from end of gzip data
  asset->len = gzdata[gzlen-4] + gzdata[gzlen-3]*256 + gzdata[gzlen-2]*65536;
  if(asset->len==0) {
    Serial.println("file size=0");
    free(gzdata);
    return false;
  }

  asset->data = (unsigned char *)ps_calloc(1, asset->len+1);
  if( asset->data == NULL ) {
    tft_error("Malloc error (%d bytes)", (int)asset->len+1);
    free(gzdata);
    return false;
  }

  gzStreamPtr  = new ReadBufferStream((void*)gzdata, gzlen);
  romStreamPtr = new WriteBufferStream((void*)asset->data, asset->len);

  GzUnpacker *unpacker = new GzUnpacker();
  // reset progress data
  gz_progress_len = asset->len;
  gz_progress = 0;

  unpacker->setLoggerCallback( nullptr );
  unpacker->setStreamWriter( gzWriter );
  unpacker->setGzProgressCallback( gzProgressCb );

  gzProgressCb(0);
  bool ret = unpacker->gzStreamExpander( gzStreamPtr);
  gzProgressCb(100);

  if( gz_progress != asset->len ) {
    Serial.printf("Memory overflow (%lu were allocated, %lu were written)\n", asset->len, gz_progress );
    ret = false;
  }

  delete(unpacker);
  delete(romStreamPtr);
  delete(gzStreamPtr);
  free(gzdata);

  if( asset->data == NULL ) {
    Serial.printf("Post Malloc error (%d bytes)\n", (int)asset->len);
    return false;
  }
  return ret;
}


bool load_asset(GWFile* asset)
{
  assert(asset);
  assert(asset->path);
  if( asset->data != nullptr && asset->len>0)
    return true; // already loaded

  auto fname_len = strlen(asset->path);
  if( fname_len<4) {
    Serial.println("Invalid file name");
    return false; // invalid file name (e.g. shortest filename is "a.gz")
  }

  // if(!gwFS->exists(asset->path)) {
  //   Serial.printf("File %s does not exist\n", asset->path);
  //   return false; // not found!
  // }

  bool is_gz = (strcmp(asset->path + fname_len - 3, ".gz") == 0);

  auto srcFile = gwFS->open(asset->path);
  if(!srcFile) {
    Serial.printf("Unable to open %s\n", asset->path);
    return false; // unable to open!
  }

  bool ret = false;
  if(is_gz) {
    ret = expand_asset(asset, &srcFile, srcFile.size());
  } else {
    asset->len = srcFile.size();
    asset->data = (unsigned char*)ps_calloc(1, asset->len+1);
    if(asset->data) {
      ret = srcFile.readBytes((char*)asset->data, asset->len) == asset->len;
    } else {
      Serial.println("Alloc failed");
    }
  }
  srcFile.close();
  return ret;
}




