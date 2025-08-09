<?php

/*
 * This php script takes the output of LCD-Game-Shrinker and gzips it
 * so that more roms can fit in the destination device.
 *
 * It is intended to be used **after** running LCD-Game-Shrinker.
 *  - https://gist.github.com/DNA64/16fed499d6bd4664b78b4c0a9638e4ef
 *
 * It will also shrink artwork images and generate some cpp code for the
 * Arduino app.
 *
 *
 *
 * */

// LittleFS data folder path (or SD card), where the final compressed roms/artworks will be saved
$ExportDir = getCWD().'/data';
$RomExportDir = $ExportDir;
$JpgExportDir = $ExportDir;
// Uncomment those if generating for the SD Card
// $ExportDir = getCWD().'/SD';
// $RomExportDir = $ExportDir.'/roms';
// $JpgExportDir = $ExportDir.'/artworks';

$gamesMap = [
  'ball'     => ['AC-01' , 'gnw_ball'    , "Ball"                  , 'Silver'         , 'SS2Btns' , 'SSLay' , 'AC-01.png' ],
  'bfight'   => ['BF-803', 'gnw_bfight'  , "Balloon Fight"         , 'Crystal Screen' , 'CS5Btns' , 'CSLay' , 'new-wide-screen-bfight.png' ],
  'chef'     => ['FP-24' , 'gnw_chef'    , "Chef"                  , 'Wide Screen'    , 'WS2Btns' , 'WSLay' , 'FP-24.png' ],
  'climber'  => ['DR-802', 'gnw_climber' , "Climber"               , 'Crystal Screen' , 'CS5Btns' , 'CSLay' , 'new-wide-screen-climber.png' ],
  'dkjr'     => ['DJ-101', 'gnw_dkjr'    , "Donkey Kong Jr."       , 'New Wide Screen', 'NWS5Btns', 'NWSLay', 'DJ-101.png' ],
  'dkjrp'    => ['CJ-71' , 'gnw_dkjrp'   , "Donkey Kong Jr."       , 'Table Top'      , 'NWS5Btns', 'NWSLay', 'new-wide-screen-5-buttons-dkjrp.png' ],
  'fire'     => ['FR-27' , 'gnw_fire'    , "Fire"                  , 'Wide Screen'    , 'WS2Btns' , 'WSLay' , 'FR-27.png' ],
  'fireatk'  => ['ID-29' , 'gnw_fireatk' , "Fire Attack"           , 'Wide Screen'    , 'WS4Btns' , 'WSLay' , 'ID-29.png' ],
  'fires'    => ['RC-04' , 'gnw_fires'   , "Fire"                  , 'Silver'         , 'SS2Btns' , 'SSLay' , 'RC-04.png' ],
  'flagman'  => ['FL-02' , 'gnw_flagman' , "Flagman"               , 'Silver'         , 'SS4Btns' , 'SSLay' , 'FL-02.png' ],
  'helmet'   => ['CN-07' , 'gnw_helmet'  , "Helmet"                , 'Gold'           , 'GS2Btns' , 'GSLay' , 'CN-07.png' ],
  'judge'    => ['IP-05' , 'gnw_judge'   , "Judge"                 , 'Silver'         , 'SS4Btns' , 'SSLay' , 'IP-05.png' ],
  'lion'     => ['LN-08' , 'gnw_lion'    , "Lion"                  , 'Gold'           , 'GS4Btns' , 'GSLay' , 'Unit.png' ],
  'manhole'  => ['NH-103', 'gnw_manhole' , "Manhole"               , 'New Wide Screen', 'NWS4Btns', 'NWSLay', 'NH-103.png' ],
  'manholeg' => ['MH-06' , 'gnw_manholeg', "Manhole"               , 'Gold'           , 'GS4Btns' , 'GSLay' , 'Unit.png' ],
  'mariocm'  => ['ML-102', 'gnw_mariocm' , "Mario's Cement Factory", 'New Wide Screen', 'NWS3Btns', 'NWSLay', 'ML-102.png' ],
  'mariocmt' => ['CM-72' , 'gnw_mariocmt', "Mario's Cement Factory", 'Table Top'      , 'NWS3Btns', 'NWSLay', 'wide-screen-3-buttons-mariocmt.png' ],
  'mariotj'  => ['MB-108', 'gnw_mariotj' , "Mario the Juggler"     , 'New Wide Screen', 'NWS2Btns', 'NWSLay', 'Unit.png' ],
  'mbaway'   => ['TB-94' , 'gnw_mbaway'  , "Mario's Bombs Away"    , 'Panorama'       , 'NWS3Btns', 'NWSLay', 'new-wide-screen-3-buttons-mbaway.png' ],
  'mmouse'   => ['MC-25' , 'gnw_mmouse'  , "Mickey Mouse"          , 'Wide Screen'    , 'WS4Btns' , 'WSLay' , 'MC-25.png' ],
  'mmousep'  => ['DC-95' , 'gnw_mmousep' , "Mickey Mouse"          , 'Panorama'       , 'WS2Btns' , 'WSLay' , 'wide-screen-from-panoramic-mmousep.png' ],
  'octopus'  => ['OC-22' , 'gnw_octopus' , "Octopus"               , 'Wide Screen'    , 'WS2Btns' , 'WSLay' , 'OC-22.png' ],
  'pchute'   => ['PR-21' , 'gnw_pchute'  , "Parachute"             , 'Wide Screen'    , 'WS2Btns' , 'WSLay' , 'PR-21.png' ],
  'popeye'   => ['PP-23' , 'gnw_popeye'  , "Popeye"                , 'Wide Screen'    , 'WS2Btns' , 'WSLay' , 'PP-23.png' ],
  'popeyep'  => ['PG-92' , 'gnw_popeyep' , "Popeye"                , 'Panorama'       , 'NWS3Btns', 'NWSLay', 'new-wide-screen-popeyep.png' ],
  'smb'      => ['YM-801', 'gnw_smb'     , "Super Mario Bros."     , 'Crystal Screen' , 'CS5Btns' , 'CSLay' , 'new-wide-screen-smb.png' ],
  'snoopyp'  => ['SM-73' , 'gnw_snoopyp' , "Snoopy"                , 'Table Top'      , 'NWS3Btns', 'NWSLay', 'new-wide-screen-snoopyp.png' ],
  'stennis'  => ['SP-30' , 'gnw_stennis' , "Snoopy Tennis"         , 'Wide Screen'    , 'WS3Btns' , 'WSLay' , 'SP-30.png' ],
  'tbridge'  => ['TL-28' , 'gnw_tbridge' , "Turtle Bridge"         , 'Wide Screen'    , 'WS2Btns' , 'WSLay' , 'TF-104.png' ],
  'tfish'    => ['TF-104', 'gnw_tfish'   , "Tropical Fish"         , 'New Wide Screen', 'NWS2Btns', 'NWSLay', 'TF-104.png' ],
  'vermin'   => ['MT-03' , 'gnw_vermin'  , "Vermin"                , 'Silver'         , 'SS2Btns' , 'SSLay' , 'MT-03.png' ],
]; // 31 items

// add some fields to the array, will be populated from the mame cpp file
foreach($gamesMap as $game => $map)
{
  $gamesMap[$game][] = [];      // inputs
  $gamesMap[$game][] = ['GAW']; // controls
}


function game($game_id)
{
  global $gamesMap;
  if(!isset($gamesMap[$game_id]))
    return false;
  $game = $gamesMap[$game_id];
  return [
    'model'          => $game[0],
    'romname'        => $game[1],
    'title'          => $game[2],
    'series'         => $game[3],
    'buttons_layout' => $game[4],
    'artwork_layout' => $game[5],
    'artwork_path'   => $game[6],
    'inputs'         => $game[7],
    'controls'       => $game[8],
  ];
}


// temporary dir for imagemagick, no trailing slash

$tmpDir = getCWD().'/tmp/artworks';

// LCD-Game-Shrinker path, cloned in the current directory
$LCDShrinkerDir = getCWD().'/LCD-Game-Shrinker';
// list of roms to shrink (same as rom names in MAME archive, but without the "gnw_" prefix)
// NOTE: those are single-display games!
$games = [
 'ball',  'bfight', 'chef', 'climber', 'dkjr', 'dkjrp', 'fire', 'fireatk', 'fires', 'flagman',  'helmet',
 'judge', 'lion', 'manhole', 'manholeg', 'mariocm', 'mariocmt', 'mariotj', 'mbaway', 'mmouse', 'mmousep',
 'octopus', 'pchute', 'popeye', 'popeyep', 'smb', 'snoopyp', 'stennis', 'tbridge', 'tfish', 'vermin'
];
$artworkSize = "640x360"; // jpeg max size, keep this at half the final size e.g. 640x360 for a 12080*720 panel, going below this will produce artefacts
$artworkQuality = 80;  // jpeg quality (0..100), the more downsizing, the higher it should be e.g. quality 80 for 50% downsizing
$artworkAltPath = getCWD();//.'/generic';

// don't edit anything below this

// build dir in LCD-Game-Shrinker directory, no trailing slash
$LCDShrinkerBuildDir  = $LCDShrinkerDir.'/build';
// path to the Game&Watch hpp file taken from MAME source tree, should be in LCD Game Shrinker dir too as it's downloaded during shrinking
$mameClassFile = $LCDShrinkerDir.'/build/hh_sm510.cpp';


// some arrays for cpp code generation
$objects = [];
$GwTouchButtons    = [];
$GwTouchButtonsSet = [];
$gameslist = [];

// support for image creation
//extension_loaded("gd") or die("Enable gd extension in php.ini!!".PHP_EOL);
exec("magick -help") or die("imagemagick not found".PHP_EOL);


// temp folder for images
if(!is_dir($tmpDir))
  mkdir($tmpDir, 0777, true) or die("Unable to create temporary dir: $tmpDir");
// dest folder(s) for final export
if(!is_dir($ExportDir))
  mkdir($ExportDir, 0777, true) or die("Unable to create ExportDir: $ExportDir");
if(!is_dir($RomExportDir))
  mkdir($RomExportDir, 0777, true) or die("Unable to create RomExportDir: $RomExportDir");
if(!is_dir($JpgExportDir))
  mkdir($JpgExportDir, 0777, true) or die("Unable to create JpgExportDir: $JpgExportDir");


if(!is_dir($LCDShrinkerDir) || !is_dir($LCDShrinkerBuildDir)) {
  // if(!is_dir($LCDShrinkerBuildDir)) {
  //   foreach($games as $game) {
  //     $cmd = sprintf("python3 %s/shrink_it.py input/rom/gnw_%s.zip\n", $LCDShrinkerDir, $game);
  //     echo $cmd.PHP_EOL;
  //     exec($cmd);
  //   }
  // } else
  {
    echo 'FATAL: No LCD Shrinker directory found, but this script needs the shrunk roms and artwork files!'.PHP_EOL;
    echo 'Either edit $LCDShrinkerDir with the proper path, or install an run LCD Game Shrinker first!'.PHP_EOL;
    echo 'See: https://gist.github.com/DNA64/16fed499d6bd4664b78b4c0a9638e4ef'.PHP_EOL;
    echo 'Also see: https://github.com/bzhxx/LCD-Game-Shrinker'.PHP_EOL;
    die();
  }
}

// sort games alphabetically
//asort($games);

// some control port names to help with filtering
$controlPorts = ['IPT_UNUSED', 'IPT_SELECT', 'IPT_START1', 'IPT_START2', 'IPT_SERVICE1', 'IPT_SERVICE2'];

// extract port names
$mameClassLines = explode("\n", file_get_contents($mameClassFile)) or die("whoops");
for($l=0;$l<count($mameClassLines);$l++)
{
  $line = trim($mameClassLines[$l]);

  if(!preg_match("/INPUT_PORTS_START\(([^\)]+)\)/", $line, $out))
    continue;

  if(!isset($out[1])) // regexp succeeded but capture empty
    continue;

  $romname = trim($out[1]);

  if(!preg_match("/^gnw_/", $romname))
    continue; // not a Game & Watch rom

  $romshortname = str_replace("gnw_", "", $romname);

  //if(!in_array($romshortname, $games))
  if(!isset($gamesMap[$romshortname]))
  {
    while(trim($mameClassLines[$l])!='INPUT_PORTS_END' && $l<count($mameClassLines))
      $l++; // jump to closing tag
    continue;
  }

  do {
    $l++;
    $line = trim($mameClassLines[$l]);

    if(!preg_match("/PORT_BIT\(([^,]+),([^,]+),([^\)]+)\)/", $line, $out))
      continue; // not a PORT_BIT macro
    if(count($out)!=4)
      continue; // malformed PORT_BIT macro

    $inputName = trim($out[3]); // trim name
    $inputName = preg_replace("/IPT_(JOYSTICK)?_?/", "", $inputName); // remove macro prefix
    if( !in_array(trim($out[3]), $controlPorts)) { // is an input port (up/down/left/right/action)
      // TouchButton objects in the destination cpp code will expect this formatting
      $gamesMap[$romshortname][7][]   = str_replace(' ', '', mb_convert_case(str_replace('_', ' ', strtolower($inputName)), MB_CASE_TITLE, 'UTF-8')); // add to rom

    } else { // is a control port (time/alarm/acl/game a/game b)
      if( $inputName!='UNUSED' && !in_array($inputName, $gamesMap[$romshortname][8])) { // prevent duplicates
        if(!preg_match('/PORT_NAME\("([^\)]+)"\)/', $line, $out))
          die("$romname::$inputName has no port name");
        $gamesMap[$romshortname][8][]   = str_replace(" ", "", ($out[1])); // add to rom
      }
    }

  } while(trim($line)!='INPUT_PORTS_END');
}


foreach($gamesMap as $game => $ary)
{
  $gameAry = game($game) or die("Unknown game:".$game.PHP_EOL);

  $btns  = $gameAry['buttons_layout'];
  $layer = $gameAry['artwork_layout'];

  if( !preg_match('/([A-Z]+)([0-9]+)([a-zA-Z]+)/', $btns, $out) )
    die("Bad layout name: $btns".PHP_EOL);

  if(empty($out) || count($out)!=4)
    die("Bad layout name: $btns".PHP_EOL);

  // extract button layout name prefix and count for later cpp variable naming
  $prefix = $out[1];
  $count  = $out[2];

  $romname   = $gameAry['romname'];
  $gameTitle = $gameAry['title'];
  $artwork   = $gameAry['artwork_path'];

  // temp source file name
  $pngartwork = $tmpDir.'/gnw_'.$game.'.png';

  // dst jpg filename and its gzname
  $jpgartwork = 'gnw_'.$game.'.jpg';
  $gzjpgartwork = $ExportDir.'/gnw_'.$game.'.jpg.gz';

  // dst rom filename and its gzname
  $gwrom = 'gnw_'.$game.'.gw';
  $gzgwrom = $ExportDir.'/gnw_'.$game.'.gw.gz';

  // where to find the rom
  $LCDShrinkerGameBuildDir = $LCDShrinkerBuildDir.'/gnw_'.$game;
  // where to find the artwork
  $LCDShrinkerGameMAMEDir  = $LCDShrinkerGameBuildDir.'/original';

  // if the artwork generic, imagemagick will add the game name to the artwork
  $isGeneric = false;

  if( file_exists( $LCDShrinkerGameMAMEDir.'/'.$artwork ) ) // artwork found in MAME dir
    $artwork_path = $LCDShrinkerGameMAMEDir.'/'.$artwork;
  else if( file_exists( $artworkAltPath.'/generic/'.$artwork ) ) { // artwork found in M5Tab-Game-and-Watch dir (substitution)
    $artwork_path = $artworkAltPath.'/generic/'.$artwork;
    $isGeneric = preg_match('/generic/', $artwork);
  } else die("can't find path for $artwork ($artworkAltPath / $LCDShrinkerGameMAMEDir".PHP_EOL);

  if(!file_exists($artwork_path))
    die("File $artwork_path for game $game does not exist, it should be either in $LCDShrinkerGameBuildDir or $artwork_path".PHP_EOL);

  if(!copy("$artwork_path", "$pngartwork"))
    die("Unable to copy $artwork_path to $png_artwork".PHP_EOL);

  if(!file_exists("$LCDShrinkerGameBuildDir/$gwrom")) {
    $cmd = sprintf("python3 %s/shrink_it.py input/rom/gnw_%s.zip\n", $LCDShrinkerDir, $game);
    die("That rom was not build: $gwrom, you can build it using the following command:".PHP_EOL.$cmd.PHP_EOL);
  }
  copy("$LCDShrinkerGameBuildDir/$gwrom", $tmpDir.'/'.$gwrom);

  $optionalCmd = "";
  if( $isGeneric ) { // compose game title into the generic artwork
    if( PHP_OS_FAMILY!='Windows' ) { // Unless it's windows :D
      $optionalCmd = sprintf('textpos=%s && magick %s -font \'%s\' -fill \'%s\' -pointsize %d -gravity center -annotate %s "%s" %s',
        '$(magick '.$jpgartwork.' -format "+0-%[fx:int(h*0.33)]" info:)', // NOTE: this is a linux shell command
        $tmpDir.'/'.$jpgartwork,
        "DejaVu-Sans",  // $font,
        "#202020", // $fillColor
        20, // $pointSize,
        '$textpos',
        $gameTitle,
        $jpgartwork
      );
    }
  }

  // list( $dstwidth, $dstheight ) = explode("x", $artworkSize); // dst
  // list( $srcwidth, $srcheight ) = getimagesize($pngartwork);    // src
  //
  // $multiplier = min( $dstwidth/$srcwidth, $dstheight/$srcheight );
  // $dstwidth  = (int)($srcwidth*$multiplier);
  // $dstheight = (int)($srcheight*$multiplier);
  //
  // // echo sprintf("[$srcwidth:$srcheight] * %.2f = [%d:%d]".PHP_EOL, $multiplier, $srcwidth*$multiplier, $srcheight*$multiplier);
  //
  // $imgsrc = @imagecreatefrompng($pngartwork);
  // $imgdst = imagecreatetruecolor($dstwidth, $dstheight);
  //
  // imagecopyresampled($imgdst, $imgsrc, 0, 0, 0, 0, $dstwidth, $dstheight, $srcwidth, $srcheight);


  $cmd = sprintf('magick %s -resize %s -size %s -quality %d %s', $pngartwork, $artworkSize, $artworkSize, $artworkQuality, $tmpDir.'/'.$jpgartwork); // shrink the artwork, downsize+compress
  exec($cmd, $out);


  if($isGeneric) {
    // TODO: add text
    // list($left, $bottom, $right, , , $top) = imageftbbox($font_size, $angle, $font, $text);
    // Imagettftext($imgdst, ($right - $left) / 2, $dstheight*0.33, $start_x, $start_y, $black, 'verdana.ttf', "text to write");
  }

  //echo imageresolution($imgsrc)[0].PHP_EOL;
  //imageresolution($imgsrc, 300);

  // fuck that, imagejpeg is unable to produce a consistent result between windows and linux (windows images are bigger)
  // imagejpeg($imgdst, $tmpDir.'/'.$jpgartwork, 80);

  // gzip'em all
  foreach([$tmpDir.'/'.$gwrom=>$gzgwrom, $tmpDir.'/'.$jpgartwork=>$gzjpgartwork] as $src=>$dst)
  {
    $data = file_get_contents($src) or die("File $src is unreadable".PHP_EOL);
    $gzdata = gzencode($data, 9);
    file_put_contents($dst, $gzdata) or die("File $dst can't be saved".PHP_EOL);
  }

  @unlink($tmpDir.'/'.$gwrom);
  @unlink($tmpDir.'/'.$jpgartwork);
  @unlink($pngartwork);

  $controls = $gameAry['controls'];
  $inputs   = $gameAry['inputs'];

  for($i=0;$i<count($inputs);$i++) {
    $inputs[$i]   = $prefix.$count.$inputs[$i];
  }
  for($i=0;$i<count($controls);$i++) {
    $controls[$i] = $prefix.$controls[$i];
  }

  asort($controls);
  asort($inputs);

  $GwTouchButtons[] = sprintf("GWTouchButton %10s[] = { %s };", $btns, implode(", ", array_merge($controls, $inputs)) );
  $objects[] = sprintf('GWGame gw_%-8s( %-24s, %-14s, %-6s, %-8s );', $game, '"'.$gameTitle.'"', '"gnw_'.$game.'"', $layer, $btns);

  $gameslist[] = 'gw_'.$game;
}

$gw_games_fmt = '#pragma once

// This code was generated using '.basename(__FILE__).' utility

#include "./gw_layouts.hpp"

// Button sets (prefix S=Silver, G=Gold, WS=Wide, NWS=New Wide, C=Crystal)
%s

// Supported single screen games
%s

// Games array
GWGame SupportedGWGames[] = {
  %s
};

';

asort($GwTouchButtons);

file_put_contents("gw/gw_games.hpp",
  sprintf($gw_games_fmt,
    implode(PHP_EOL, array_unique($GwTouchButtons)),
    implode(PHP_EOL, $objects),
    implode(', ', $gameslist),
  )
);

