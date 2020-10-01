#ifndef _PLAYERS_H_
#define _PLAYERS_H_

/* use mysql database instead of ascii pfiles */
#define USING_MYSQL_DATABASE_FOR_PLAYERFILE 1

typedef enum
{
  PLAYER_ARRAY_SKILLS,
  PLAYER_ARRAY_QUESTS,
  PLAYER_ARRAY_COOLDOWNS
} PlayerArrayTypes; 

/* need to change types/sizes later */
#define CREATE_PLAYERFILE_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
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
  `Intel` int(11) DEFAULT NULL, \
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
  `Ac` int(11) DEFAULT NULL, \
  `Act_0` int(11) DEFAULT NULL, \
  `Act_1` int(11) DEFAULT NULL, \
  `Act_2` int(11) DEFAULT NULL, \
  `Act_3` int(11) DEFAULT NULL, \
  `Aff_0` int(11) DEFAULT NULL, \
  `Aff_1` int(11) DEFAULT NULL, \
  `Aff_2` int(11) DEFAULT NULL, \
  `Aff_3` int(11) DEFAULT NULL, \
  `Alin` int(11) DEFAULT NULL, \
  `Badp` int(11) DEFAULT NULL, \
  `Bank` int(11) DEFAULT NULL, \
  `Descr` varchar(4096) DEFAULT NULL, \
  `Drnk` int(11) DEFAULT NULL, \
  `Drol` int(11) DEFAULT NULL, \
  `Frez` int(11) DEFAULT NULL, \
  `Hrol` int(11) DEFAULT NULL, \
  `Hung` int(11) DEFAULT NULL, \
  `Invs` int(11) DEFAULT NULL, \
  `Lmot` int(11) DEFAULT NULL, \
  `Lnew` int(11) DEFAULT NULL, \
  `Olc` int(11) DEFAULT NULL, \
  `Page` int(11) DEFAULT NULL, \
  `PfIn` varchar(256) DEFAULT NULL, \
  `PfOt` varchar(256) DEFAULT NULL, \
  `Pref_0` int(11) DEFAULT NULL, \
  `Pref_1` int(11) DEFAULT NULL, \
  `Pref_2` int(11) DEFAULT NULL, \
  `Pref_3` int(11) DEFAULT NULL, \
  `Qstp` int(11) DEFAULT NULL, \
  `Qpnt` int(11) DEFAULT NULL, \
  `Qcnt` int(11) DEFAULT NULL, \
  `ScrW` int(11) DEFAULT NULL, \
  `Thir` int(11) DEFAULT NULL, \
  `Thr1` int(11) DEFAULT NULL, \
  `Thr2` int(11) DEFAULT NULL, \
  `Thr3` int(11) DEFAULT NULL, \
  `Thr4` int(11) DEFAULT NULL, \
  `Thr5` int(11) DEFAULT NULL, \
  `Wimp` int(11) DEFAULT NULL, \
  `Qcur` int(11) DEFAULT NULL, \
  `MaxHit` int(11) DEFAULT NULL, \
  `MaxMana` int(11) DEFAULT NULL, \
  `MaxMove` int(11) DEFAULT NULL, \
  `Deleted` int(11) DEFAULT -1, \
  PRIMARY KEY (`ID`), \
  UNIQUE KEY `idx_playerfile_Name` (`Name`) ) ENGINE=MyISAM DEFAULT CHARSET=latin1;"

#define CREATE_ALIAS_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
  `PlayerID` int(11) NOT NULL, \
  `Alias` varchar(100) NOT NULL, \
  `Replacement` varchar(100) DEFAULT NULL, \
  `Type` varchar(100) DEFAULT NULL \
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1;"

#define CREATE_PLAYER_VARS_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
  `PlayerId` INT NOT NULL, \
  `Name` VARCHAR(100) NOT NULL, \
  `Value` VARCHAR(45) NULL, \
  `Context` INT NULL DEFAULT NULL \
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1;"

#define CREATE_PLAYER_ARRAYS_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
  `PlayerId` INT NOT NULL, \
  `ArrayId` INT NOT NULL, \
  `Value` INT NULL DEFAULT NULL, \
  `Type` INT NULL DEFAULT NULL \
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1;"

#define CREATE_PLAYER_OBJECTS_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
  `PlayerId` int(11) NOT NULL, \
  `Vnum` int(11) NOT NULL, \
  `Loc` int(11) DEFAULT NULL, \
  `Name` varchar(100) DEFAULT NULL, \
  `Shrt`  varchar(100) DEFAULT NULL \
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1;"

#define CREATE_ROOMS_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
  `Vnum` int(11) NOT NULL, \
  `Zone` int(11) DEFAULT NULL, \
  `Name` varchar(100) DEFAULT NULL, \
  `Description`  varchar(2048) DEFAULT NULL, \
  `SectorType` int(11) NOT NULL, \
  `RoomFlags_0` int(11) NOT NULL, \
  `RoomFlags_1` int(11) NOT NULL, \
  `RoomFlags_2` int(11) NOT NULL, \
  `RoomFlags_3` int(11) NOT NULL \
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1;"


#define CREATE_ROOM_DIRS_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
  `Vnum` int(11) NOT NULL, \
  `DirectionNum` int(11) NOT NULL, \
  `Keyword` varchar(250) DEFAULT NULL, \
  `Description`  varchar(2048) DEFAULT NULL, \
  `ExitInfo` int(11) NOT NULL, \
  `KeyNum` int(11) NOT NULL, \
  `ToRoom` int(11) NOT NULL \
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1;"


#endif /* _PLAYERS_H_ */
