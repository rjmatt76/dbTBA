#include <mysql.h>

void get_mysql_database_conn();

#define MYSQL_HOST "localhost"
#define MYSQL_USER "strifemud"
#define MYSQL_PASS "x3L5$pqZw_"
#define MYSQL_DB "strife_mud"
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
  PRIMARY KEY (`ID`), \
  UNIQUE KEY `idx_playerfile_Name` (`Name`) ) ENGINE=MyISAM DEFAULT CHARSET=latin1;"

#define MYSQL_QUERY_INSERT 0
#define MYSQL_QUERY_SELECT 1
#define MYSQL_QUERY_UPDATE 2
#define MYSQL_QUERY_DELETE 3

struct mysql_column_bind_adapter {
  const char *column_name;
  long data_type;
};

struct mysql_parameter_bind_adapter {
  char * string_data;
  int int_data;
  long data_type;
  int data_length;
};

struct mysql_bind_column {
  char name[MAX_INPUT_LENGTH];
  char col_string_buffer[MAX_INPUT_LENGTH];
  int col_int_buffer;
  my_bool is_null;
  unsigned long col_length;
  unsigned long buffer_length;
};
