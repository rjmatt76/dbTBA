#ifndef _PLAYERS_H_
#define _PLAYERS_H_

#define MYSQL_PLAYER_TABLE "playerfile"

/* use mysql database instead of ascii pfiles */
#define USING_MYSQL_DATABASE_FOR_PLAYERFILE 1

/* need to change types/sizes later */
#define CREATE_PLAYERFILE_TABLE "CREATE TABLE IF NOT EXISTS `strife_mud`.`playerfile` ( \
  `ID` int(11) NOT NULL, \
  `Name` varchar(100) NOT NULL, \
  `Pass` varchar(100) DEFAULT NULL, \
  `Titl` varchar(100) DEFAULT NULL, \
  `Sex`  int(11) DEFAULT NULL, \
  `Levl` int(11) DEFAULT NULL, \
  `Home` varchar(100) DEFAULT NULL, \
  `Brth` varchar(100) DEFAULT NULL, \
  `Plyd` varchar(100) DEFAULT NULL, \
  `Last` varchar(100) DEFAULT NULL, \
  `Host` varchar(100) DEFAULT NULL, \
  `Hite` varchar(100) DEFAULT NULL, \
  `Wate` varchar(100) DEFAULT NULL, \
  `Act` varchar(100) DEFAULT NULL, \
  `Smem` varchar(100) DEFAULT NULL, \
  `Inna` varchar(100) DEFAULT NULL, \
  `Kamt` varchar(100) DEFAULT NULL, \
  `Feat` varchar(100) DEFAULT NULL, \
  `FtSl` varchar(100) DEFAULT NULL, \
  `Spk` varchar(100) DEFAULT NULL, \
  `Preg` varchar(100) DEFAULT NULL, \
  `Room` varchar(100) DEFAULT NULL, \
  `Prf` varchar(100) DEFAULT NULL, \
  `Lern` varchar(100) DEFAULT NULL, \
  `Str` int(11) DEFAULT NULL, \
  `Int` int(11) DEFAULT NULL, \
  `Wis` int(11) DEFAULT NULL, \
  `Dex` int(11) DEFAULT NULL, \
  `Con` int(11) DEFAULT NULL, \
  `Cha` int(11) DEFAULT NULL, \
  `Hit` int(11) DEFAULT NULL, \
  `Mana` int(11) DEFAULT NULL, \
  `Move` int(11) DEFAULT NULL, \
  `Gold` int(11) DEFAULT NULL, \
  `Exp` int(11) DEFAULT NULL, \
  `God` varchar(100) DEFAULT NULL, \
  `Pver` varchar(100) DEFAULT NULL, \
  `Affs` varchar(100) DEFAULT NULL, \
  `Clas` int(11) DEFAULT NULL, \
  `Deleted` int(11) DEFAULT -1, \
  PRIMARY KEY (`ID`), \
  UNIQUE KEY `idx_playerfile_Name` (`Name`) ) ENGINE=MyISAM DEFAULT CHARSET=latin1;"

#define CREATE_ALIAS_TABLE "CREATE TABLE IF NOT EXISTS `strife_mud`.`player_alias` ( \
  `PlayerID` int(11) NOT NULL, \
  `Alias` varchar(100) NOT NULL, \
  `Replacement` varchar(100) DEFAULT NULL, \
  `Type` varchar(100) DEFAULT NULL \
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1;"

#define CREATE_PLAYER_OBJECTS_TABLE "CREATE TABLE IF NOT EXISTS `strife_mud`.`player_objects` ( \
  `PlayerId` int(11) NOT NULL, \
  `Vnum` int(11) NOT NULL, \
  `Loc` int(11) DEFAULT NULL, \
  `Name` varchar(100) DEFAULT NULL, \
  `Shrt`  varchar(100) DEFAULT NULL \
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1;"

#endif /* _PLAYERS_H_ */
