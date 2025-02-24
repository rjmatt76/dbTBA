#ifndef _MYSQL_DB_H_
#define _MYSQL_DB_H_

#include <mysql.h>
typedef enum
{
  MYSQL_QUERY_SELECT,
  MYSQL_QUERY_UPDATE,
  MYSQL_QUERY_DELETE,
  MYSQL_QUERY_INSERT
} MySqlQueryType;

#define EXTRA_MYSQL_DEBUG_LOGS 0

#define MYSQL_PLAYER_TABLE "playerfile"
#define MYSQL_ALIAS_TABLE "player_alias"
#define MYSQL_PLAYER_OBJECTS_TABLE "player_objects"
#define MYSQL_PLAYER_ARRAYS_TABLE "player_arrays"
//dg trigger variables saved
#define MYSQL_PLAYER_VARS_TABLE "player_vars"

#define MYSQL_PLAYER_CLANS_TABLE "player_clans"

#define MYSQL_ROOM_TABLE "rooms"
#define MYSQL_ROOM_DIR_TABLE "room_direction_data"
#define MYSQL_OBJECT_TABLE "objects"
#define MYSQL_MOBILE_TABLE "mobiles"
#define MYSQL_ZONE_TABLE "zones"
#define MYSQL_ZONE_COMMANDS_TABLE "zone_commands"
#define MYSQL_TRIGGER_TABLE "triggers"

#define MYSQL_MAIL_TABLE "mail"

#define MYSQL_DB mysql_connection_strings.database
#define MYSQL_USER mysql_connection_strings.username
#define MYSQL_PASS mysql_connection_strings.password
#define MYSQL_HOST mysql_connection_strings.host

struct mysql_connection_strings {
  char username[MAX_INPUT_LENGTH];
  char password[MAX_INPUT_LENGTH];
  char database[MAX_INPUT_LENGTH];
  char host[MAX_INPUT_LENGTH];
};

extern struct mysql_connection_strings mysql_connection_strings;

struct mysql_column {
  char *column_name;
  long data_type;
};

struct mysql_parameter {
  char * string_data;
  int int_data;
  long data_type;
  int data_length;
};

struct mysql_bind_column {
  char name[MAX_INPUT_LENGTH];
  char col_string_buffer[MAX_INPUT_LENGTH];
  int col_int_buffer;
  int is_null;
  unsigned long col_length;
  unsigned long buffer_length;
  long data_type;
};

int query_stmt_mysql(MYSQL *conn, struct mysql_parameter *parameters, const struct mysql_column *columns,
  char *statement, int num_columns, int num_parameters,
  void (*load_function)(struct mysql_bind_column *, int, int, void *, MYSQL_STMT *stmt),
  void *ch, int querytype);
int test_error(MYSQL *mysql, int status);
int test_stmt_error(MYSQL_STMT *stmt, int status);
void free_mysql_parameters(struct mysql_parameter *p, int num_parameters);
int get_column_sql(char *buf, size_t buf_size, const struct mysql_column *cb);
int get_column_update_sql(char *buf, size_t buf_size, const struct mysql_column *cb);
int get_parameter_markers_sql(char *buf, size_t buf_size, int num_columns, int num_rows);
void ping_mysql_database();

/* Global database connection */
extern MYSQL *db_conn;
void close_mysqlcon_with_error(MYSQL *conn);
void create_mud_db_tables();
int connect_primary_mysql_db();
int create_mysql_conn(char *host, char *user, char *pass, char *db);
void ping_mysql_database();

/* use mysql database instead of ascii pfiles */
#define USING_MYSQL_DATABASE_FOR_PLAYERFILE 1

typedef enum
{
  PLAYER_ARRAY_SKILLS,
  PLAYER_ARRAY_QUESTS,
  PLAYER_ARRAY_COOLDOWNS,
  PLAYER_ARRAY_KILLS,
  PLAYER_ARRAY_FEATS,
  PLAYER_ARRAY_MEMORIZATION
} PlayerArrayTypes;

/* TODO: triggers */
#define CREATE_PLAYERFILE_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
  `ID` int(11) NOT NULL, \
  `Ac` int(11) DEFAULT NULL, \
  `Act_0` int(11) DEFAULT NULL, \
  `Act_1` int(11) DEFAULT NULL, \
  `Act_2` int(11) DEFAULT NULL, \
  `Act_3` int(11) DEFAULT NULL, \
  `Aff_0` int(11) DEFAULT NULL, \
  `Aff_1` int(11) DEFAULT NULL, \
  `Aff_2` int(11) DEFAULT NULL, \
  `Aff_3` int(11) DEFAULT NULL, \
  `Affs` varchar(500) DEFAULT NULL, \
  `Alin` int(11) DEFAULT NULL, \
  `Badp` int(11) DEFAULT NULL, \
  `Bank` int(11) DEFAULT NULL, \
  `Brth` varchar(100) DEFAULT NULL, \
  `Cha` int(11) DEFAULT NULL, \
/*  `Clan` int(11) DEFAULT NULL,*/ \
/*  `ClanRank` int(11) DEFAULT NULL,*/ \
  `Clas` int(11) DEFAULT NULL, \
  `Con` int(11) DEFAULT NULL, \
  `Name` varchar(100) NOT NULL, \
  `Pass` varchar(100) DEFAULT NULL, \
  `Titl` varchar(100) DEFAULT NULL, \
  `Sex`  int(11) DEFAULT NULL, \
  `Levl` int(11) DEFAULT NULL, \
  `Home` int(11) DEFAULT NULL, \
  `Plyd` int(11) DEFAULT NULL, \
  `Last` int(11) DEFAULT NULL, \
  `Host` varchar(100) DEFAULT NULL, \
  `Hite` int(11) DEFAULT NULL, \
  `Wate` int(11) DEFAULT NULL, \
  `Spk` int(11) DEFAULT NULL, \
  `Room` int(11) DEFAULT NULL, \
  `Lern` varchar(100) DEFAULT NULL, \
  `Str` int(11) DEFAULT NULL, \
  `Intel` int(11) DEFAULT NULL, \
  `Wis` int(11) DEFAULT NULL, \
  `Dex` int(11) DEFAULT NULL, \
  `Hit` int(11) DEFAULT NULL, \
  `Mana` int(11) DEFAULT NULL, \
  `Move` int(11) DEFAULT NULL, \
  `Gold` int(11) DEFAULT NULL, \
  `Exp` int(11) DEFAULT NULL, \
/*  `God` int(11) DEFAULT NULL,*/ \
  `Pver` varchar(100) DEFAULT NULL, \
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
/*  `PreTitle` varchar(100) DEFAULT NULL, */ \
  `Qstp` int(11) DEFAULT NULL, \
  `Qcnt` int(11) DEFAULT NULL, \
  `ScrW` int(11) DEFAULT NULL, \
  `Thir` int(11) DEFAULT NULL, \
  `Thr1` int(11) DEFAULT NULL, \
  `Thr2` int(11) DEFAULT NULL, \
  `Thr3` int(11) DEFAULT NULL, \
  `Thr4` int(11) DEFAULT NULL, \
  `Thr5` int(11) DEFAULT NULL, \
/*  `WhoTitle` varchar(100) DEFAULT NULL,*/ \
  `Wimp` int(11) DEFAULT NULL, \
  `Qcur` int(11) DEFAULT NULL, \
  `MaxHit` int(11) DEFAULT NULL, \
  `MaxMana` int(11) DEFAULT NULL, \
  `MaxMove` int(11) DEFAULT NULL, \
  `Deleted` int(11) DEFAULT -1, \
  PRIMARY KEY (`ID`), \
  UNIQUE KEY `idx_playerfile_Name` (`Name`) ) ENGINE=MyISAM DEFAULT CHARSET=latin1;"
/*
#define CREATE_PLAYER_CLANS_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
  `ID` int(11) NOT NULL, \
  `Name` varchar(100) NOT NULL, \
  `MemberLookStr` varchar(100) DEFAULT NULL, \
  `Leader` varchar(100) DEFAULT NULL, \
  `RankName0`  varchar(100) DEFAULT 'Rank0', \
  `RankName1`  varchar(100) DEFAULT 'Rank1', \
  `RankName2`  varchar(100) DEFAULT 'Rank2', \
  `RankName3`  varchar(100) DEFAULT 'Rank3', \
  `RankName4`  varchar(100) DEFAULT 'Rank4', \
  `RankName5`  varchar(100) DEFAULT 'Rank5', \
  `Gold` int(11) DEFAULT 0, \
  `Guard1` int(11) DEFAULT 0, \
  `Guard2` int(11) DEFAULT 0, \
  `Direction` int(11) DEFAULT 0, \
  `ClanRecall` int(11) DEFAULT 0, \
  PRIMARY KEY (`ID`), \
  UNIQUE KEY `idx_player_clans_Name` (`Name`) ) ENGINE=MyISAM DEFAULT CHARSET=latin1"
*/
#define CREATE_ALIAS_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
  `PlayerID` int(11) NOT NULL, \
  `Alias` varchar(100) NOT NULL, \
  `Replacement` varchar(100) DEFAULT NULL, \
  `Type` varchar(100) DEFAULT NULL \
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1"

#define CREATE_PLAYER_VARS_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
  `PlayerId` INT NOT NULL, \
  `Name` VARCHAR(100) NOT NULL, \
  `Value` VARCHAR(45) NULL, \
  `Context` INT NULL DEFAULT NULL \
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1"

#define CREATE_PLAYER_ARRAYS_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
  `PlayerId` INT NOT NULL, \
  `ArrayId` INT NOT NULL, \
  `Value` INT NULL DEFAULT NULL, \
  `Type` INT NULL DEFAULT NULL \
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1"

#define CREATE_PLAYER_OBJECTS_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
  `PlayerId` int(11) NOT NULL, \
  `Loc` int(11) DEFAULT NULL, \
  `Name` varchar(100) DEFAULT NULL, \
  `Vnum` int(11) NOT NULL, \
  `ItemOrder` int(11) NOT NULL, \
  `ShortDescr`  varchar(150) DEFAULT NULL, \
  `Descr` varchar(4096) NOT NULL, \
  `ActionDescr` varchar(4096) NOT NULL, \
  `Extra_0` int(11) NOT NULL, \
  `Extra_1` int(11) NOT NULL, \
  `Extra_2` int(11) NOT NULL, \
  `Extra_3` int(11) NOT NULL, \
  `Wear_0` int(11) NOT NULL, \
  `Wear_1` int(11) NOT NULL, \
  `Wear_2` int(11) NOT NULL, \
  `Wear_3` int(11) NOT NULL, \
  `Aff_0` int(11) NOT NULL, \
  `Aff_1` int(11) NOT NULL, \
  `Aff_2` int(11) NOT NULL, \
  `Aff_3` int(11) NOT NULL, \
  `Value_0` int(11) NOT NULL, \
  `Value_1` int(11) NOT NULL, \
  `Value_2` int(11) NOT NULL, \
  `Value_3` int(11) NOT NULL, \
  `Cslots` int(11) NOT NULL, \
  `Tslots` int(11) NOT NULL, \
  `Weight` int(11) NOT NULL, \
  `Cost` int(11) NOT NULL, \
  `Level` int(11) NOT NULL, \
  `Timer` int(11) NOT NULL \
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1"

/* TODO: triggers, ex_descriptions */
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
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1"

#define CREATE_ROOM_DIRS_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
  `Vnum` int(11) NOT NULL, \
  `DirectionNum` int(11) NOT NULL, \
  `Keyword` varchar(250) DEFAULT NULL, \
  `Description`  varchar(2048) DEFAULT NULL, \
  `ExitInfo` int(11) NOT NULL, \
  `KeyNum` int(11) NOT NULL, \
  `ToRoom` int(11) NOT NULL \
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1"

/* TODO:  triggers */
#define CREATE_MOBILES_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
  `Vnum` int(11) NOT NULL, \
  `Zone` int(11) DEFAULT NULL, \
  `Alias` varchar(100) DEFAULT NULL, \
  `ShortDescr`  varchar(150) DEFAULT NULL, \
  `LongDescr` varchar(150) NOT NULL, \
  `DDescr` varchar(4096) NOT NULL, \
  `MobFlags_0` int(11) NOT NULL, \
  `MobFlags_1` int(11) NOT NULL, \
  `MobFlags_2` int(11) NOT NULL, \
  `MobFlags_3` int(11) NOT NULL, \
  `Aff_0` int(11) NOT NULL, \
  `Aff_1` int(11) NOT NULL, \
  `Aff_2` int(11) NOT NULL, \
  `Aff_3` int(11) NOT NULL, \
  `Align` int(11) NOT NULL, \
  `Level` int(11) NOT NULL, \
  `HitRoll` int(11) NOT NULL, \
  `AC` int(11) NOT NULL, \
  `Hit` int(11) NOT NULL, \
  `Mana` int(11) NOT NULL, \
  `Move` int(11) NOT NULL, \
  `Ndd` int(11) NOT NULL, \
  `Sdd` int(11) NOT NULL, \
  `DamRoll` int(11) NOT NULL, \
  `Gold` int(11) NOT NULL, \
  `Exp` int(11) NOT NULL, \
  `Pos` int(11) NOT NULL, \
  `DefaultPos` int(11) NOT NULL, \
  `Sex` int(11) NOT NULL, \
  `BareHandAttack` int(11) NOT NULL, \
  `Str` int(11) NOT NULL, \
  `Dex` int(11) NOT NULL, \
  `Intel` int(11) NOT NULL, \
  `Wis` int(11) NOT NULL, \
  `Con` int(11) NOT NULL, \
  `Cha` int(11) NOT NULL, \
/*  `Clan` int(11) NOT NULL,*/ \
  `Class` int(11) NOT NULL,  \
/*  `Race` int(11) NOT NULL,*/ \
/*  `God` int(11) NOT NULL, */\
  `SavingPara` int(11) NOT NULL, \
  `SavingRod` int(11) NOT NULL, \
  `SavingPetri` int(11) NOT NULL, \
  `SavingBreath` int(11) NOT NULL, \
  `SavingSpell` int(11) NOT NULL \
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1"

/* TODO:  weapon spells, affects, ex_descriptions, triggers */
#define CREATE_OBJECTS_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
  `Vnum` int(11) NOT NULL, \
  `Name` varchar(100) DEFAULT NULL, \
  `ShortDescr`  varchar(150) DEFAULT NULL, \
  `Descr` varchar(4096) NOT NULL, \
  `ActionDescr` varchar(4096) NOT NULL, \
  `Extra_0` int(11) NOT NULL, \
  `Extra_1` int(11) NOT NULL, \
  `Extra_2` int(11) NOT NULL, \
  `Extra_3` int(11) NOT NULL, \
  `Wear_0` int(11) NOT NULL, \
  `Wear_1` int(11) NOT NULL, \
  `Wear_2` int(11) NOT NULL, \
  `Wear_3` int(11) NOT NULL, \
  `Aff_0` int(11) NOT NULL, \
  `Aff_1` int(11) NOT NULL, \
  `Aff_2` int(11) NOT NULL, \
  `Aff_3` int(11) NOT NULL, \
  `Value_0` int(11) NOT NULL, \
  `Value_1` int(11) NOT NULL, \
  `Value_2` int(11) NOT NULL, \
  `Value_3` int(11) NOT NULL, \
  `Cslots` int(11) NOT NULL, \
  `Tslots` int(11) NOT NULL, \
  `Weight` int(11) NOT NULL, \
  `Cost` int(11) NOT NULL, \
  `Level` int(11) NOT NULL, \
  `Timer` int(11) NOT NULL \
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1"

#define CREATE_ZONES_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
  `Zone` int(11) NOT NULL, \
  `Name` varchar(100) DEFAULT NULL, \
  `Builders`  varchar(250) DEFAULT NULL, \
  `Bottom` int(11) NOT NULL, \
  `Top` int(11) NOT NULL, \
  `Lifespan` int(11) NOT NULL, \
  `ResetMode` int(11) NOT NULL, \
  `ZoneFlags_0` int(11) NOT NULL, \
  `ZoneFlags_1` int(11) NOT NULL, \
  `ZoneFlags_2` int(11) NOT NULL, \
  `ZoneFlags_3` int(11) NOT NULL, \
  `MinLevel` int(11) NOT NULL, \
  `MaxLevel` int(11) NOT NULL \
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1"

#define CREATE_ZONE_COMMANDS_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
  `Zone` int(11) NOT NULL, \
  `CommandType` varchar(25) NOT NULL, \
  `If_Flag` int(11) NOT NULL, \
  `Arg1` int(11) NOT NULL, \
  `Arg2` int(11) NOT NULL, \
  `Arg3` int(11) NOT NULL, \
  `Sarg1` varchar(100) NOT NULL, \
  `Sarg2` varchar(100) NOT NULL, \
  `Comment` varchar(50) NOT NULL \
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1"

#define CREATE_TRIGGERS_TABLE "CREATE TABLE IF NOT EXISTS `%s`.`%s` ( \
  `Vnum` int(11) NOT NULL, \
  `Name` varchar(100) DEFAULT NULL, \
  `AttachType`  int(11) DEFAULT NULL, \
  `TrigType`  varchar(10) DEFAULT NULL, \
  `TrigNarg`  int(11) DEFAULT NULL, \
  `TrigArg`  varchar(100) DEFAULT NULL, \
  `CommandText`  varchar(16000) DEFAULT NULL \
  ) ENGINE=MyISAM DEFAULT CHARSET=latin1"

#endif /* _MYSQL_DB_H */
