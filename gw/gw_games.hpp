#pragma once

// This code was generated using gen_littlefs.php utility

#include "./gw_layouts.hpp"

// Button sets (prefix S=Silver, G=Gold, WS=Wide, NWS=New Wide, C=Crystal)
GWTouchButton    CS5Btns[] = { CSACL, CSAlarm, CSGAW, CSGame, CSTime, CS5Button1, CS5Down, CS5Left, CS5Right, CS5Up };
GWTouchButton    GS2Btns[] = { GSACL, GSAlarm, GSGAW, GSGameA, GSGameB, GSTime, GS2Left, GS2Right };
GWTouchButton    GS4Btns[] = { GSACL, GSAlarm, GSGAW, GSGameA, GSGameB, GSTime, GS4LeftDown, GS4LeftUp, GS4RightDown, GS4RightUp };
GWTouchButton    SS2Btns[] = { SSACL, SSGAW, SSGameA, SSGameB, SSTime, SS2Left, SS2Right };
GWTouchButton    SS4Btns[] = { SSACL, SSGAW, SSGameA, SSGameB, SSTime, SS4LeftDown, SS4LeftUp, SS4RightDown, SS4RightUp };
GWTouchButton    WS2Btns[] = { WSACL, WSAlarm, WSGAW, WSGameA, WSGameB, WSTime, WS2Left, WS2Right };
GWTouchButton    WS3Btns[] = { WSACL, WSAlarm, WSGAW, WSGameA, WSGameB, WSTime, WS3Button1, WS3Down, WS3Up };
GWTouchButton    WS4Btns[] = { WSACL, WSAlarm, WSGAW, WSGameA, WSGameB, WSTime, WS4LeftDown, WS4LeftUp, WS4RightDown, WS4RightUp };
GWTouchButton   NWS2Btns[] = { NWSACL, NWSAlarm, NWSGAW, NWSGameA, NWSGameB, NWSTime, NWS2Left, NWS2Right };
GWTouchButton   NWS3Btns[] = { NWSACL, NWSAlarm, NWSGAW, NWSGameA, NWSGameB, NWSTime, NWS3Button1, NWS3Left, NWS3Right };
GWTouchButton   NWS4Btns[] = { NWSACL, NWSAlarm, NWSGAW, NWSGameA, NWSGameB, NWSTime, NWS4LeftDown, NWS4LeftUp, NWS4RightDown, NWS4RightUp };
GWTouchButton   NWS5Btns[] = { NWSACL, NWSAlarm, NWSGAW, NWSGameA, NWSGameB, NWSTime, NWS5Button1, NWS5Down, NWS5Left, NWS5Right, NWS5Up };

// Supported single screen games
GWGame gw_ball    ( "Ball"                  , "gnw_ball"    , SSLay , SS2Btns  );
GWGame gw_bfight  ( "Balloon Fight"         , "gnw_bfight"  , CSLay , CS5Btns  ); // TODO: CSLay GWButton
GWGame gw_chef    ( "Chef"                  , "gnw_chef"    , WSLay , WS2Btns  );
GWGame gw_climber ( "Climber"               , "gnw_climber" , CSLay , CS5Btns  ); // TODO: CSLay GWButton
GWGame gw_dkjr    ( "Donkey Kong Jr."       , "gnw_dkjr"    , NWSLay, NWS5Btns );
GWGame gw_dkjrp   ( "Donkey Kong Jr."       , "gnw_dkjrp"   , NWSLay, NWS5Btns );
GWGame gw_fire    ( "Fire"                  , "gnw_fire"    , WSLay , WS2Btns  );
GWGame gw_fireatk ( "Fire Attack"           , "gnw_fireatk" , WSLay , WS4Btns  );
GWGame gw_fires   ( "Fire"                  , "gnw_fires"   , SSLay , SS2Btns  );
GWGame gw_flagman ( "Flagman"               , "gnw_flagman" , SSLay , SS4Btns  );
GWGame gw_helmet  ( "Helmet"                , "gnw_helmet"  , GSLay , GS2Btns  );
GWGame gw_judge   ( "Judge"                 , "gnw_judge"   , SSLay , SS4Btns  );
GWGame gw_lion    ( "Lion"                  , "gnw_lion"    , GSLay , GS4Btns  );
GWGame gw_manhole ( "Manhole"               , "gnw_manhole" , NWSLay, NWS4Btns );
GWGame gw_manholeg( "Manhole"               , "gnw_manholeg", GSLay , GS4Btns  );
GWGame gw_mariocm ( "Mario's Cement Factory", "gnw_mariocm" , NWSLay, NWS3Btns );
GWGame gw_mariocmt( "Mario's Cement Factory", "gnw_mariocmt", NWSLay, NWS3Btns );
GWGame gw_mariotj ( "Mario the Juggler"     , "gnw_mariotj" , NWSLay, NWS2Btns ); // TODO: CS2Btns ?
GWGame gw_mbaway  ( "Mario's Bombs Away"    , "gnw_mbaway"  , NWSLay, NWS3Btns );
GWGame gw_mmouse  ( "Mickey Mouse"          , "gnw_mmouse"  , WSLay , WS4Btns  );
GWGame gw_mmousep ( "Mickey Mouse"          , "gnw_mmousep" , WSLay , WS2Btns  );
GWGame gw_octopus ( "Octopus"               , "gnw_octopus" , WSLay , WS2Btns  );
GWGame gw_pchute  ( "Parachute"             , "gnw_pchute"  , WSLay , WS2Btns  );
GWGame gw_popeye  ( "Popeye"                , "gnw_popeye"  , WSLay , WS2Btns  );
GWGame gw_popeyep ( "Popeye"                , "gnw_popeyep" , NWSLay, NWS3Btns );
GWGame gw_smb     ( "Super Mario Bros."     , "gnw_smb"     , CSLay , CS5Btns  ); // TODO: CSLay GWButton
GWGame gw_snoopyp ( "Snoopy"                , "gnw_snoopyp" , NWSLay, NWS3Btns );
GWGame gw_stennis ( "Snoopy Tennis"         , "gnw_stennis" , WSLay , WS3Btns  );
GWGame gw_tbridge ( "Turtle Bridge"         , "gnw_tbridge" , WSLay , WS2Btns  );
GWGame gw_tfish   ( "Tropical Fish"         , "gnw_tfish"   , NWSLay, NWS2Btns );
GWGame gw_vermin  ( "Vermin"                , "gnw_vermin"  , SSLay , SS2Btns  );

// Games array
GWGame SupportedGWGames[] = {
  gw_ball, gw_bfight, gw_chef, gw_climber, gw_dkjr, gw_dkjrp, gw_fire, gw_fireatk, gw_fires, gw_flagman, gw_helmet, gw_judge, gw_lion, gw_manhole, gw_manholeg, gw_mariocm, gw_mariocmt, gw_mariotj, gw_mbaway, gw_mmouse, gw_mmousep, gw_octopus, gw_pchute, gw_popeye, gw_popeyep, gw_smb, gw_snoopyp, gw_stennis, gw_tbridge, gw_tfish, gw_vermin
};

