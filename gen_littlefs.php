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
 *
 *
 *
 *
 * */




// temporary dir for imagemagick, no trailing slash
$magickDir = '/tmp/artworks';
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

// don't edit anything below this

// build dir in LCD-Game-Shrinker directory, no trailing slash
$buildRoot  = $LCDShrinkerDir.'/build';
// path to the Game&Watch hpp file taken from MAME source tree, should be in LCD Game Shrinker dir too as it's downloaded during shrinking
$mameClassFile = $LCDShrinkerDir.'/build/hh_sm510.cpp';

// Most game IDs can be guessed from the default.lay file in MAME archive, except for those
$gamesByRomName = [
  'cgrab'    => 'UD-202',
  'dkjrp'    => 'CJ-71',
  'lion'     => 'LN-08',
  'manholeg' => 'MH-06',
  'mariocmt' => 'CM-72',
  'mbaway'   => 'TB-94',
  'mmousep'  => 'DC-95',
  'popeyep'  => 'PG-92',
  'smb'      => 'YM-801',
  'snoopyp'  => 'SM-73',
  'mariotj'  => 'MB-108',
  'tbridge'  => 'TL-28'
];

// Most game images can be guessed from the default.lay file in MAME archive, except for those
$imagesByRomName = [
  'ball'     => '/gnw_ball/original/AC-01.png',
  'bfight'   => '/new-wide-screen-bfight.png', // substitute
  'cgrab'    => '/gnw_cgrab/original/Unit.png',
  'climber'  => '/new-wide-screen-climber.png', // substitute
  'dkjr'     => '/gnw_dkjr/original/DJ-101.png',
  'dkjrp'    => '/new-wide-screen-5-buttons-dkjrp.png',
  'fireatk'  => '/gnw_fireatk/original/ID-29.png',
  'fires'    => '/gnw_fires/original/RC-04.png',
  'helmet'   => '/gnw_helmet/original/CN-07.png',
  'lion'     => '/gnw_lion/original/Unit.png',
  'manholeg' => '/gnw_manholeg/original/Unit.png',
  'mariocm'  => '/gnw_mariocm/original/ML-102.png',
  'mariocmt' => '/wide-screen-3-buttons-mariocmt.png', // substitute
  'mariotj'  => '/gnw_mariotj/original/Unit.png',
  'mbaway'   => '/new-wide-screen-3-buttons-generic.png', // substitute
  'mmouse'   => '/gnw_mmouse/original/MC-25.png',
  'mmousep'  => '/wide-screen-from-panoramic-mmousep.png', // substitute
  'octopus'  => '/gnw_octopus/original/OC-22.png',
  'popeyep'  => '/new-wide-screen-popeyep.png', // substitute
  'pchute'   => '/gnw_pchute/original/PR-21.png',
  'smb'      => '/new-wide-screen-smb.png', // substitute
  'snoopyp'  => '/new-wide-screen-snoopyp.png', // substitute
  'stennis'  => '/gnw_stennis/original/SP-30.png',
  'tfish'    => '/gnw_tfish/original/TF-104.png',
  'vermin'   => '/gnw_vermin/original/MT-03.png'
];


$objects = [];
$GwTouchButtons    = [];
$GwTouchButtonsSet = [];
$gameslist = [];


// temp folder for imagemagick
if(!is_dir($magickDir))
  mkdir($magickDir, 0777, true) or die("Unable to create dir $magickDir");

if(!is_dir($LCDShrinkerDir) || !is_dir($buildRoot)) {
  // if(!is_dir($buildRoot)) {
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
asort($games);




$mameRomSettings = [];

// some control port names to help with filtering
$controlPorts = ['IPT_UNUSED', 'IPT_SELECT', 'IPT_START1', 'IPT_START2', 'IPT_SERVICE1', 'IPT_SERVICE2'];

// extract port names
$mameClassLines = explode("\n", file_get_contents($mameClassFile));
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

  if(!in_array($romshortname, $games)) {
    while(trim($mameClassLines[$l])!='INPUT_PORTS_END' && $l<count($mameClassLines))
      $l++; // jump to closing tag
    continue;
  }

  // Insert a new "GAW" button, it'll be used over the Game&Watch logo
  // to enable the game picker menu.
  $mameRomSettings[$romname] = [ 'inputs'=>[], 'controls' => ['GAW'] ];

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
      $mameRomSettings[$romname]['inputs'][] = str_replace(' ', '', mb_convert_case(str_replace('_', ' ', strtolower($inputName)), MB_CASE_TITLE, 'UTF-8')); // add to rom
    } else { // is a control port (time/alarm/acl/game a/game b)
      if( $inputName!='UNUSED' && !in_array($inputName, $mameRomSettings[$romname]['controls'])) { // prevent duplicates
        if(!preg_match('/PORT_NAME\("([^\)]+)"\)/', $line, $out))
          die("$romname::$inputName has no port name");
        $mameRomSettings[$romname]['controls'][] = str_replace(" ", "", ($out[1])); // add to rom
      }
    }

  } while(trim($line)!='INPUT_PORTS_END');
}

// extract game titles
for($l=0;$l<count($mameClassLines);$l++)
{
  $line = trim($mameClassLines[$l]);

  if(preg_match("/^CONS\(([^,]+),([^,]+),([^,]+),([^,]+),([^,]+),([^,]+),([^,]+),([^,]+),([^,]+),([^,]+),([^\)]+)\)/", $line, $out)) {
    // CONS( 1980, gnw_ball, 0, 0, gnw_ball, gnw_ball, gnw_ball_state, empty_init, "Nintendo", "Game & Watch: Ball", MACHINE_SUPPORTS_SAVE )
    if( count($out)!=12 )
      continue;
    if( trim($out[9])!='"Nintendo"')
      continue;
    if(!preg_match("/^gnw_/", trim($out[2])))
      continue;
    $romname = trim($out[2]);
    $romshortname = str_replace("gnw_", "", $romname);
    if(!in_array($romshortname, $games) )
      continue;

    $gameTitle = str_replace( ["Game & Watch: ", '"'], '', trim($out[10]));

    $mameRomSettings[$romname]['title']['long'] = $gameTitle;

    echo sprintf("%-16s : %s".PHP_EOL, $romname, $gameTitle);
  }
}


{
  // retrieve game titles from wikipedia, associate with game ID
  $gamesBymodel = [];
  $wikipediaPage = file_get_contents("https://en.wikipedia.org/wiki/List_of_Game_%26_Watch_games") or die("http whoops".PHP_EOL);

  $doc = Dom\HTMLDocument::createFromString($wikipediaPage);
  $tables = $doc->getElementsByTagName('table');
  // games list is in the first table
  $xml = simplexml_import_dom($tables[0]) or die("Error: Cannot create object from Wikipedia html");
  $columnTitles = $xml->tbody->tr[0]->th;
  $rows = count($xml->tbody->tr);

  for($i=1;$i<$rows;$i++) {
    // remove wikipedia notes, if any
    if(isset($xml->tbody->tr[$i]->td[4]->sup))
      unset($xml->tbody->tr[$i]->td[4]->sup);

    $title  = trim(strip_tags($xml->tbody->tr[$i]->td[0]->asXml()));
    $series = trim(strip_tags($xml->tbody->tr[$i]->td[3]->asXml()));
    $model  = trim(strip_tags($xml->tbody->tr[$i]->td[4]->asXml()));
    //echo sprintf("%-32s %-32s %-8s", $title, $series, $model).PHP_EOL;
    $gamesBymodel[strtoupper($model)] = ['title'=>$title, 'series'=>$series];
  }
}


chdir("gw");

foreach($games as $game) {

  switch($game) {

    case "bfight":
    case "climber":
    case "smb":
      $btns = "CS5Btns";
      $layer = 'CSLay';
      $prefix = "CS"; $count = 5;
    break;

    case "dkjr":
    case "dkjrp":
      $btns = "NWS5Btns";
      $layer = 'NWSLay';
      $prefix = "NWS"; $count = 5;
    break;

    case 'manhole':
      $btns = 'NWS4Btns';
      $layer = 'NWSLay';
      $prefix = "NWS"; $count = 4;
    break;

    case 'mbaway':
    case 'snoopyp':
    case 'popeyep':
    case 'mariocm':
    case 'mariocmt':
      $btns = 'NWS3Btns';
      $layer = 'NWSLay';
      $prefix = "NWS"; $count = 3;
    break;

    case 'tfish':
    case 'mariotj':
    //case 'tbridge':
      $btns = 'NWS2Btns';
      $layer = 'NWSLay';
      $prefix = "NWS"; $count = 2;
    break;

    case 'fireatk':
    case 'mmouse':
      $btns  = 'WS4Btns';
      $layer = 'WSLay';
      $prefix = "WS"; $count = 4;
    break;

    case 'stennis':
      $btns  = 'WS3Btns';
      $layer = 'WSLay';
      $prefix = "WS"; $count = 3;
    break;

    case 'tbridge':
    case 'mmousep':
    case 'chef':
    case 'fire':
    case 'octopus':
    case 'popeye':
    case 'pchute':
      $layer = "WSLay";
      $btns  = "WS2Btns";
      $prefix = "WS"; $count = 2;
    break;

    case 'flagman':
    case 'judge':
      $btns = 'SS4Btns';
      $layer = 'SSLay';
      $prefix = "SS"; $count = 4;
    break;

    case "ball":
    case "vermin":
    case "fires":
      $btns = 'SS2Btns';
      $layer = 'SSLay';
      $prefix = "SS"; $count = 2;
    break;

    case "manholeg":
    case "lion":
      $layer = "GSLay";
      $btns  = "GS4Btns";
      $prefix = "GS"; $count = 4;
    break;

    case "helmet":
      $layer = "GSLay";
      $btns  = "GS2Btns";
      $prefix = "GS"; $count = 2;
    break;

    default:
      die("Missing game: $game".PHP_EOL);
  }


  $mameRomSettings["gnw_$game"]['game_layout'] = $layer;
  $mameRomSettings["gnw_$game"]['btns_layout'] = $btns;

  $pngartwork = $magickDir.'/gnw_'.$game.'.png';

  $jpgartwork = 'gnw_'.$game.'.jpg';
  $gzjpgartwork = '../data/gnw_'.$game.'.jpg.gz';

  $gwrom = 'gnw_'.$game.'.gw';
  $gzgwrom = '../data/gnw_'.$game.'.gw.gz';

  $buildPath  = $buildRoot.'/gnw_'.$game;
  $originalPath = $buildPath.'/original';
  $layoutFile = $originalPath.'/default.lay';

  if(!file_exists($layoutFile)) {
    die("File $layoutFile does not exist".PHP_EOL);
  }

  // try to find game-id in layout file, title will be matched from that
  $xml = simplexml_load_file("$layoutFile") or die("Error: Cannot create object from XML in $layoutFile");

  $gameTitle = $game;

  if(isset($gamesByRomName[$game])) {
    $gameTitle = $gamesBymodel[$gamesByRomName[$game]]['title'];
  } else {
    foreach($xml->element as $element) {
      if(!isset($element->image[0]['file']) || !isset($element['name']))
        continue; // malformed element
      if( isset($element['defstate']) || substr_count($element['name'], "-") == 0 )
        continue; // not interested by those
      $model = strtoupper($element['name']);
      if(!isset($gamesBymodel[$model])) {
        continue;
      }
      $gameTitle = $gamesBymodel[$model]['title'];
      $gamesBymodel[$model]['game'] = $game;
      break;
    }
  }

  if($gameTitle == $game )
    echo 'Missing game name for '.$game.PHP_EOL;

  // remove whatever is (between parenthesis) in the game title
  $gameTitle = preg_replace("~\((.+?)\)~", "", $gameTitle);

  $isGeneric = false;

  if( isset($imagesByRomName[$game]) && file_exists($buildRoot.$imagesByRomName[$game]) ) {
    copy($buildRoot.$imagesByRomName[$game], "$pngartwork");
    if( preg_match("/generic/", $imagesByRomName[$game] ) )
      $isGeneric = true;
  } else if(!file_exists($pngartwork)) {

    if( isset($imagesByRomName[$game]) ) {
      if(!file_exists($buildRoot.$imagesByRomName[$game]) ) {
        die($buildRoot.$imagesByRomName[$game].' does not exist'.PHP_EOL);
      }
      copy($buildRoot.$imagesByRomName[$game], "$pngartwork");
    } else {
      if(!isset($xml->element[5]->image[0]['file'])) {
        die("Cannot recover from missing image for $game".PHP_EOL);
      }

      $img = $xml->element[5]->image[0]['file']; // >element[5]->image[0]->file;

      if(!file_exists("$originalPath/$img")) {
        die("Found image in layout but file $img is missing in folder $originalPath".PHP_EOL);
      }

      copy("$originalPath/$img", "$pngartwork");
    }
  }

  if(!file_exists($gwrom)) {
    if(!file_exists("$buildPath/$gwrom")) {
      $cmd = sprintf("python3 %s/shrink_it.py input/rom/gnw_%s.zip\n", $LCDShrinkerDir, $game);
      die("That rom was not build: $gwrom, you can build it using the following command:".PHP_EOL.$cmd.PHP_EOL);
    }
    copy("$buildPath/$gwrom", $gwrom);
  }

  $optionalCmd = "";
  if( $isGeneric ) { // compose game title into the generic artwork
    $optionalCmd = sprintf('textpos=%s && magick %s -font \'%s\' -fill \'%s\' -pointsize %d -gravity center -annotate %s "%s" %s',
      '$(magick '.$jpgartwork.' -format "+0-%[fx:int(h*0.33)]" info:)',
      $jpgartwork,
      "DejaVu-Sans",  // $font,
      "#202020", // $fillColor
      20, // $pointSize,
      '$textpos',
      $gameTitle,
      $jpgartwork
    );
  }

  $cmds = [
    sprintf('magick %s -resize %s -size %s -quality %d %s', $pngartwork, $artworkSize, $artworkSize, $artworkQuality, $jpgartwork), // shrink the artwork, downsize+compress
    $optionalCmd,
    sprintf("gzip -c %s > %s",  $gwrom,      $gzgwrom),
    sprintf("gzip -c %s > %s",  $jpgartwork, $gzjpgartwork),
    sprintf("rm %s",  $gwrom),
    sprintf("rm %s",  $jpgartwork),
    sprintf("rm %s",  $pngartwork)
  ];

  foreach($cmds as $cmd)
  {
    if(empty($cmd))
      continue;
    $out = [];
    exec($cmd, $out);
    if(!empty($out)) {
      print_r($out);
    }
  }


  for($i=0;$i<count($mameRomSettings["gnw_$game"]['inputs']);$i++) {
    $mameRomSettings["gnw_$game"]['inputs'][$i] = $prefix.$count.$mameRomSettings["gnw_$game"]['inputs'][$i];
  }
  for($i=0;$i<count($mameRomSettings["gnw_$game"]['controls']);$i++) {
    $mameRomSettings["gnw_$game"]['controls'][$i] = $prefix.$mameRomSettings["gnw_$game"]['controls'][$i];
  }

  $controls = $mameRomSettings["gnw_$game"]['controls'];
  $inputs   = $mameRomSettings["gnw_$game"]['inputs'];

  asort($controls);
  asort($inputs);

  $GwTouchButtons[] = sprintf("GWTouchButton %10s[] = { %s };", $btns, implode(", ", array_merge($controls, $inputs)) );
  $GwTouchButtonsSet[] = sprintf('GWButtonsSet %14s( %16s, %s );', $layer.$btns, '"'.$layer.$btns.'"', $btns);
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

file_put_contents("gw_games.hpp",
  sprintf($gw_games_fmt,
    implode(PHP_EOL, array_unique($GwTouchButtons)),
    //implode(PHP_EOL, array_unique($GwTouchButtonsSet)),
    implode(PHP_EOL, $objects),
    implode(', ', $gameslist),
  )
);




