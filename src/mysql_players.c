/**************************************************************************
*  File: mysql_players.c                                                  *
*  Usage: Player loading/saving and utility routines                      *
*   Modified by Cyric(strifemud) from tbamud's players.c to use mysql     *
*   instead of ascii pfiles.                                              *
*                                                                         *
*  All rights reserved.  See license for complete information.            *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
**************************************************************************/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "db.h"
#include "handler.h"
#include "pfdefaults.h"
#include "dg_scripts.h"
#include "comm.h"
#include "interpreter.h"
#include "genolc.h" /* for strip_cr */
#include "config.h" /* for pclean_criteria[] */
#include "dg_scripts.h" /* To enable saving of player variables to disk */
#include "quest.h"
#include "mysql_db.h"
#include "mysql_players.h"
//#include "feats.h"
#include "act.h"
#include "fight.h"

/* local functions */
static void load_affects(char *affects_str, struct char_data *ch);
static void insert_player_vars_mysql(MYSQL *conn, struct char_data *ch);
static void insert_aliases_mysql(MYSQL *conn, struct char_data *ch);
static void select_aliases_mysql(MYSQL *conn, struct char_data *ch);

const struct mysql_column playerfile_table_index[] =
{
  { "ID",             MYSQL_TYPE_LONG           },
  { "Act_0",          MYSQL_TYPE_LONG           },
  { "Name",           MYSQL_TYPE_VAR_STRING     },
  { "Levl",           MYSQL_TYPE_LONG           },
  { "Last",           MYSQL_TYPE_LONG           },
  { "\n",             MYSQL_TYPE_LONG           }
};

void load_playerfile_index_mysql(struct mysql_bind_column *fields, int num_fields, int num_rows, void *v_ch, MYSQL_STMT *stmt)
{
  int i, row;
  row = 0;

  CREATE(player_table, struct player_index_element, num_rows);

  while (!mysql_stmt_fetch(stmt))
  {
    for(i = 0; i < num_fields; i++)
    {
      if(EXTRA_MYSQL_DEBUG_LOGS)
        log("MYSQLINFO: Field name: %s, int_buffer: %d, string_buffer: %s",
            fields[i].name, fields[i].col_int_buffer, fields[i].col_string_buffer);

      if (!strcmp(fields[i].name, "ID"))   player_table[row].id = (fields[i].col_int_buffer);
      else if (!strcmp(fields[i].name, "Name"))
      {
        CREATE(player_table[row].name, char, strlen(fields[i].col_string_buffer) + 1);
        strcpy(player_table[row].name, fields[i].col_string_buffer);
      }
      else if (!strcmp(fields[i].name, "Levl"))   player_table[row].level = (fields[i].col_int_buffer);
      else if (!strcmp(fields[i].name, "Last"))  player_table[row].last = fields[i].col_int_buffer;
      else if (!strcmp(fields[i].name, "Act_0"))  player_table[row].flags = fields[i].col_int_buffer;
    }

    top_idnum = MAX(top_idnum, player_table[row].id);
    row++;
  }
  top_of_p_file = top_of_p_table = row - 1;
}

/* select a list of all players (on or offline) for the player index
   There is no separate index table with mysql, it is just a selected
   portion of the playerfile table.  You probably don't want to hold the rest
   of the table in memory for all offline players. -Cyric */
int select_player_index_mysql(MYSQL *conn)
{
  int num_parameters, num_columns;
  char sql_buf[MAX_STRING_LENGTH];
  char buf[MAX_STRING_LENGTH];
  const struct mysql_column *pft = playerfile_table_index;
  struct mysql_parameter *parameters;

  snprintf(sql_buf, sizeof(sql_buf), "SELECT ");
  num_columns = get_column_sql(sql_buf, sizeof(sql_buf), playerfile_table_index);

  snprintf(buf, sizeof(buf), " FROM %s.%s WHERE ID <> ?", MYSQL_DB, MYSQL_PLAYER_TABLE);

  size_t remaining = sizeof(sql_buf) - strlen(sql_buf) - 1;

  if (strlen(buf) >= remaining) {
    log("MYSQL ERROR: Buffer overflow prevented.");
    return 0;
  }

  strncat(sql_buf, buf, remaining);

  log("MYSQLINFO: %s", sql_buf);

  num_parameters = 1;

  //create a parameter list (which is just the character's name here)
  CREATE(parameters, struct mysql_parameter, num_parameters);
  parameters[0].int_data = 0;
  parameters[0].data_type = MYSQL_TYPE_LONG;
  parameters[0].data_length = 0;
  parameters[0].string_data = NULL;

  query_stmt_mysql(conn, parameters, pft, sql_buf, num_columns, num_parameters, load_playerfile_index_mysql, NULL, MYSQL_QUERY_SELECT);

  free_mysql_parameters(parameters, num_parameters);
  return 1;
}

void mysql_build_player_index()
{
  select_player_index_mysql(db_conn);
}

const struct mysql_column playerfile_table[] =
{
  { "Ac",             MYSQL_TYPE_LONG       },
  { "Act_0",          MYSQL_TYPE_LONG       }, // bitvector
  { "Act_1",          MYSQL_TYPE_LONG       }, // bitvector
  { "Act_2",          MYSQL_TYPE_LONG       }, // bitvector
  { "Act_3",          MYSQL_TYPE_LONG       }, // bitvector
  { "Aff_0",          MYSQL_TYPE_LONG       }, // bitvector
  { "Aff_1",          MYSQL_TYPE_LONG       }, // bitvector
  { "Aff_2",          MYSQL_TYPE_LONG       }, // bitvector
  { "Aff_3",          MYSQL_TYPE_LONG       }, // bitvector
  { "Affs",           MYSQL_TYPE_VAR_STRING },
  { "Alin",           MYSQL_TYPE_LONG       },
  { "Badp",           MYSQL_TYPE_LONG       },
  { "Bank",           MYSQL_TYPE_LONG       },
  { "Brth",           MYSQL_TYPE_LONG       },
  { "Cha",            MYSQL_TYPE_LONG       },
//  { "Clan",           MYSQL_TYPE_LONG       },
//  { "ClanRank",       MYSQL_TYPE_LONG       },
  { "Clas",           MYSQL_TYPE_LONG       },
  { "Con",            MYSQL_TYPE_LONG       },
  { "Descr",          MYSQL_TYPE_VAR_STRING }, //Desc is a mysql keyword
  { "Dex",            MYSQL_TYPE_LONG       },
  { "Drnk",           MYSQL_TYPE_LONG       },
  { "Drol",           MYSQL_TYPE_LONG       },
  { "Exp",            MYSQL_TYPE_LONG       },
  { "Frez",           MYSQL_TYPE_LONG       },
//  { "God",            MYSQL_TYPE_LONG       },
  { "Gold",           MYSQL_TYPE_LONG       },
  { "Hit",            MYSQL_TYPE_LONG       },
  { "Hite",           MYSQL_TYPE_LONG       },
  { "Host",           MYSQL_TYPE_VAR_STRING },
  { "Hrol",           MYSQL_TYPE_LONG       },
  { "Hung",           MYSQL_TYPE_LONG       },
  { "ID",             MYSQL_TYPE_LONG       },
  { "Intel",          MYSQL_TYPE_LONG       }, //int is a mysql keyword
  { "Invs",           MYSQL_TYPE_LONG       },
  { "Last",           MYSQL_TYPE_LONG       },
  { "Lern",           MYSQL_TYPE_LONG       },
  { "Levl",           MYSQL_TYPE_LONG       },
  { "Lmot",           MYSQL_TYPE_LONG       },
  { "Lnew",           MYSQL_TYPE_LONG       },
  { "Mana",           MYSQL_TYPE_LONG       },
  { "MaxHit",         MYSQL_TYPE_LONG       },
  { "MaxMana",        MYSQL_TYPE_LONG       },
  { "Move",           MYSQL_TYPE_LONG       },
  { "MaxMove",        MYSQL_TYPE_LONG       },
  { "Name",           MYSQL_TYPE_VAR_STRING },
  { "Olc",            MYSQL_TYPE_LONG       },
  { "Page",           MYSQL_TYPE_LONG       },
  { "Pass",           MYSQL_TYPE_VAR_STRING },
  { "Plyd",           MYSQL_TYPE_LONG       },
  { "PfIn",           MYSQL_TYPE_VAR_STRING },
  { "PfOt",           MYSQL_TYPE_VAR_STRING },
  { "Pref_0",         MYSQL_TYPE_LONG       },
  { "Pref_1",         MYSQL_TYPE_LONG       },
  { "Pref_2",         MYSQL_TYPE_LONG       },
  { "Pref_3",         MYSQL_TYPE_LONG       },
//  { "PreTitle",       MYSQL_TYPE_VAR_STRING },
  { "Qstp",           MYSQL_TYPE_LONG       },
  { "Qcnt",           MYSQL_TYPE_LONG       },
//  { "Race",           MYSQL_TYPE_LONG       },
  { "Room",           MYSQL_TYPE_LONG       },
  { "Sex",            MYSQL_TYPE_LONG       },
  { "ScrW",           MYSQL_TYPE_LONG       },
  { "Str",            MYSQL_TYPE_LONG       },
  { "Thir",           MYSQL_TYPE_LONG       },
  { "Thr1",           MYSQL_TYPE_LONG       },
  { "Thr2",           MYSQL_TYPE_LONG       },
  { "Thr3",           MYSQL_TYPE_LONG       },
  { "Thr4",           MYSQL_TYPE_LONG       },
  { "Thr5",           MYSQL_TYPE_LONG       },
  { "Titl",           MYSQL_TYPE_VAR_STRING },
  { "Wate",           MYSQL_TYPE_LONG       },
//  { "WhoTitle",       MYSQL_TYPE_VAR_STRING },
  { "Wimp",           MYSQL_TYPE_LONG       },
  { "Wis",            MYSQL_TYPE_LONG       },
  { "\n",             MYSQL_TYPE_LONG       }
};

void load_playerfile_mysql(struct mysql_bind_column *fields, int num_fields, int num_rows, void *v_ch, MYSQL_STMT *stmt)
{
  int i;
  struct char_data *ch = v_ch;

  if (mysql_stmt_fetch(stmt))
   return;

  for(i = 0; i < num_fields; i++)
  {
    if(EXTRA_MYSQL_DEBUG_LOGS)
      log("MYSQLINFO: Field name: %s, int_buffer: %d, string_buffer: %s",
        fields[i].name, fields[i].col_int_buffer,
        fields[i].col_string_buffer);

    if (!strcmp(fields[i].name, "Ac"))   GET_AC(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Act_0"))   PLR_FLAGS(ch)[0] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Act_1"))   PLR_FLAGS(ch)[1] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Act_2"))   PLR_FLAGS(ch)[2] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Act_3"))   PLR_FLAGS(ch)[3] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Aff_0"))   AFF_FLAGS(ch)[0] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Aff_1"))   AFF_FLAGS(ch)[1] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Aff_2"))   AFF_FLAGS(ch)[2] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Aff_3"))   AFF_FLAGS(ch)[3] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Affs"))   load_affects(fields[i].col_string_buffer, ch);
    else if (!strcmp(fields[i].name, "Alin"))   GET_ALIGNMENT(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Badp"))   GET_BAD_PWS(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Bank"))   GET_BANK_GOLD(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Brth"))   ch->player.time.birth = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Cha"))    ch->real_abils.cha = (fields[i].col_int_buffer);
//    else if (!strcmp(fields[i].name, "Clan"))   GET_CLAN(ch) = (fields[i].col_int_buffer);
//    else if (!strcmp(fields[i].name, "ClanRank")) GET_CLAN_RANK(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Clas"))  GET_CLASS(ch)      = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Con"))   ch->real_abils.con = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Descr"))   ch->player.description = strdup(fields[i].col_string_buffer);
    else if (!strcmp(fields[i].name, "Dex"))   ch->real_abils.dex = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Drnk"))   GET_COND(ch, DRUNK) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Drol"))   GET_DAMROLL(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Exp"))   GET_EXP(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Frez"))   GET_FREEZE_LEV(ch) = (fields[i].col_int_buffer);
//    else if (!strcmp(fields[i].name, "God"))   GET_GOD(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Gold"))   GET_GOLD(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Hit"))   GET_HIT(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Hite"))   GET_HEIGHT(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Host"))   GET_HOST(ch) = strdup(fields[i].col_string_buffer);
    else if (!strcmp(fields[i].name, "Hrol"))   GET_HITROLL(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Hung"))   GET_COND(ch, HUNGER) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "ID"))   GET_IDNUM(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Intel"))   ch->real_abils.intel = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Invs"))   GET_INVIS_LEV(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Last"))   ch->player.time.logon = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Lern"))   GET_PRACTICES(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Levl"))   GET_LEVEL(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Lmot"))   GET_LAST_MOTD(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Lnew"))   GET_LAST_NEWS(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "MaxHit")) GET_MAX_HIT(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "MaxMana")) GET_MAX_MANA(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "MaxMove")) GET_MAX_MOVE(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Mana"))   GET_MANA(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Move"))   GET_MOVE(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Name")) GET_PC_NAME(ch)	= strdup(fields[i].col_string_buffer);
    else if (!strcmp(fields[i].name, "Olc"))   GET_OLC_ZONE(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Page"))   GET_PAGE_LENGTH(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Pass")) strcpy(GET_PASSWD(ch), fields[i].col_string_buffer);
    else if (!strcmp(fields[i].name, "Plyd"))   ch->player.time.played = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "PfIn"))   POOFIN(ch) = strdup(fields[i].col_string_buffer);
    else if (!strcmp(fields[i].name, "PfOt"))   POOFOUT(ch) = strdup(fields[i].col_string_buffer);
    else if (!strcmp(fields[i].name, "Pref_0"))   PRF_FLAGS(ch)[0] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Pref_1"))   PRF_FLAGS(ch)[1] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Pref_2"))   PRF_FLAGS(ch)[2] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Pref_3"))   PRF_FLAGS(ch)[3] = (fields[i].col_int_buffer);
//    else if (!strcmp(fields[i].name, "PreTitle")) GET_PRETITLE(ch) = strdup(fields[i].col_string_buffer);
    else if (!strcmp(fields[i].name, "Qstp"))     GET_QUESTPOINTS(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Qcur"))     GET_QUEST(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Qcnt"))     GET_QUEST_COUNTER(ch) = (fields[i].col_int_buffer);
//    else if (!strcmp(fields[i].name, "Race"))     GET_RACE(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Room"))   GET_LOADROOM(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Sex"))    GET_SEX(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "ScrW"))   GET_SCREEN_WIDTH(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Str"))    ch->real_abils.str = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Thir"))   GET_COND(ch, THIRST) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Thr1"))   GET_SAVE(ch, 0) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Thr2"))   GET_SAVE(ch, 1) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Thr3"))   GET_SAVE(ch, 2) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Thr4"))   GET_SAVE(ch, 3) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Thr5"))   GET_SAVE(ch, 4) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Titl"))   GET_TITLE(ch) = strdup(fields[i].col_string_buffer);
    else if (!strcmp(fields[i].name, "Wate"))   GET_WEIGHT(ch) = (fields[i].col_int_buffer);
 //   else if (!strcmp(fields[i].name, "WhoTitle")) GET_WHOTITLE(ch) = strdup(fields[i].col_string_buffer);
    else if (!strcmp(fields[i].name, "Wimp"))   GET_WIMP_LEV(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Wis"))    ch->real_abils.wis = (fields[i].col_int_buffer);
  }
}

void update_playerfile_mysql_by_ID(MYSQL *conn, int ID, struct char_data *ch, struct affected_type *affects)
{
  int i, num_columns, num_parameters;
  char buf[MAX_STRING_LENGTH];
  char parameter_buf[MAX_STRING_LENGTH];
  char buf2[MAX_STRING_LENGTH];
  char sql_buf[MAX_STRING_LENGTH];
  struct mysql_parameter *parameters;

  snprintf(sql_buf, sizeof(sql_buf), "UPDATE %s.%s SET ", MYSQL_DB, MYSQL_PLAYER_TABLE);

  num_columns = get_column_update_sql(sql_buf, sizeof(sql_buf), playerfile_table);

  snprintf(buf, sizeof(buf), " WHERE ID = ?");

  size_t remaining = sizeof(sql_buf) - strlen(sql_buf) - 1;

  if (strlen(buf) >= remaining) {
    log("MYSQL ERROR: Buffer overflow prevented.");
    return;
  }

  strncat(sql_buf, buf, remaining);

  //include the ID
  num_parameters = (num_columns + 1);
  log("MYSQLINFO: Update Player ID %d : %s | num_parameter:%d", ID, sql_buf, num_parameters);
  //create a parameter list (which is just the character's name here)
  CREATE(parameters, struct mysql_parameter, num_parameters);

  /* leave a slot for the ID */
  for(i = 0; i < num_parameters; i++)
  {
    parameters[i].data_length = 0;
    parameters[i].data_type = playerfile_table[i].data_type;

    if (!strcmp(playerfile_table[i].column_name, "Ac"))
      parameters[i].int_data = (GET_AC(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Act_0"))
      parameters[i].int_data = (PLR_FLAGS(ch)[0]);
    else if (!strcmp(playerfile_table[i].column_name, "Act_1"))
      parameters[i].int_data = (PLR_FLAGS(ch)[1]);
    else if (!strcmp(playerfile_table[i].column_name, "Act_2"))
      parameters[i].int_data = (PLR_FLAGS(ch)[2]);
    else if (!strcmp(playerfile_table[i].column_name, "Act_3"))
      parameters[i].int_data = (PLR_FLAGS(ch)[3]);
    else if (!strcmp(playerfile_table[i].column_name, "Aff_0"))
      parameters[i].int_data = (AFF_FLAGS(ch)[0]);
    else if (!strcmp(playerfile_table[i].column_name, "Aff_1"))
      parameters[i].int_data = (AFF_FLAGS(ch)[1]);
    else if (!strcmp(playerfile_table[i].column_name, "Aff_2"))
      parameters[i].int_data = (AFF_FLAGS(ch)[2]);
    else if (!strcmp(playerfile_table[i].column_name, "Aff_3"))
      parameters[i].int_data = (AFF_FLAGS(ch)[3]);
    else if (!strcmp(playerfile_table[i].column_name, "Affs"))
    {
      parameter_buf[0] = '\0';
      struct affected_type *aff = NULL;
      aff = affects;
      while(aff)
      {
        if (aff->spell > 0) { 
          size_t len = snprintf(buf2, sizeof(buf2), "%d,%d,%d,%d,%d,%d,%d,%d:", aff->spell, aff->duration,
                 aff->modifier, aff->location, aff->bitvector[0], aff->bitvector[1], aff->bitvector[2], aff->bitvector[3]);

          size_t remaining = sizeof(parameter_buf) - strlen(parameter_buf) - 1; // Calculate remaining space

          if (remaining > 0) {
            strncat(parameter_buf, buf2, len);
          } else {
            log("MYSQL ERROR: Buffer overflow prevented.");
            break;
          }

          log("buf2 -> %s", buf2);
        }
        aff = aff->next;
      }
      log("MYSQLINFO: parameter affects data: %s", parameter_buf);
      parameters[i].string_data = strdup(parameter_buf[0] ? parameter_buf : "");
    }
    else if (!strcmp(playerfile_table[i].column_name, "Alin"))
      parameters[i].int_data = (GET_ALIGNMENT(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Badp"))
      parameters[i].int_data = (GET_BAD_PWS(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Bank"))
      parameters[i].int_data = (GET_BANK_GOLD(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Brth"))
      parameters[i].int_data = (ch->player.time.birth);
    else if (!strcmp(playerfile_table[i].column_name, "Cha"))
      parameters[i].int_data = (ch->real_abils.cha);
//    else if (!strcmp(playerfile_table[i].column_name, "Clan"))
//      parameters[i].int_data = (GET_CLAN(ch));
//    else if (!strcmp(playerfile_table[i].column_name, "ClanRank"))
//      parameters[i].int_data = (GET_CLAN_RANK(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Clas"))
      parameters[i].int_data = (GET_CLASS(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Con"))
      parameters[i].int_data = (ch->real_abils.con);
    else if (!strcmp(playerfile_table[i].column_name, "Descr"))
    {
      if (ch->player.description && *ch->player.description) {
        strcpy(parameter_buf, ch->player.description);
        strip_cr(parameter_buf);
        parameters[i].string_data = strdup(parameter_buf);
      }
      else
        parameters[i].string_data = strdup("Description was blank");
    }
    else if (!strcmp(playerfile_table[i].column_name, "Dex"))
      parameters[i].int_data = (ch->real_abils.dex);
    else if (!strcmp(playerfile_table[i].column_name, "Drnk"))
      parameters[i].int_data = (GET_COND(ch, DRUNK));
    else if (!strcmp(playerfile_table[i].column_name, "Drol"))
      parameters[i].int_data = (GET_DAMROLL(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Exp"))
      parameters[i].int_data = (GET_EXP(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Frez"))
      parameters[i].int_data = (GET_FREEZE_LEV(ch));
//    else if (!strcmp(playerfile_table[i].column_name, "God"))
//      parameters[i].int_data = (GET_GOD(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Gold"))
      parameters[i].int_data = (GET_GOLD(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Hit"))
      parameters[i].int_data = (GET_HIT(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Hite"))
      parameters[i].int_data = (GET_HEIGHT(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Host"))
      parameters[i].string_data = strdup(GET_HOST(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Hrol"))
      parameters[i].int_data = (GET_HITROLL(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Hung"))
      parameters[i].int_data = (GET_COND(ch, HUNGER));
    else if (!strcmp(playerfile_table[i].column_name, "ID"))
      parameters[i].int_data = GET_IDNUM(ch);
    else if (!strcmp(playerfile_table[i].column_name, "Intel"))
      parameters[i].int_data = (ch->real_abils.intel);
    else if (!strcmp(playerfile_table[i].column_name, "Invs"))
      parameters[i].int_data = (GET_INVIS_LEV(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Last"))
      parameters[i].int_data = (ch->player.time.logon);
    else if (!strcmp(playerfile_table[i].column_name, "Lern"))
      parameters[i].int_data = (GET_PRACTICES(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Levl"))
      parameters[i].int_data = (GET_LEVEL(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Lmot"))
      parameters[i].int_data = (GET_LAST_MOTD(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Lnew"))
      parameters[i].int_data = (GET_LAST_NEWS(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Mana"))
      parameters[i].int_data = (GET_MANA(ch));
    else if (!strcmp(playerfile_table[i].column_name, "MaxHit"))
      parameters[i].int_data = (GET_MAX_HIT(ch));
    else if (!strcmp(playerfile_table[i].column_name, "MaxMana"))
      parameters[i].int_data = (GET_MAX_MANA(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Move"))
      parameters[i].int_data = (GET_MOVE(ch));
    else if (!strcmp(playerfile_table[i].column_name, "MaxMove"))
      parameters[i].int_data = (GET_MAX_MOVE(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Name"))
      parameters[i].string_data = strdup(GET_PC_NAME(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Olc"))
      parameters[i].int_data = (GET_OLC_ZONE(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Page"))
      parameters[i].int_data = (GET_PAGE_LENGTH(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Pass"))
    {
      if(GET_PASSWD(ch)) parameters[i].string_data = strdup(GET_PASSWD(ch));
    }
    else if (!strcmp(playerfile_table[i].column_name, "Plyd"))
      parameters[i].int_data = (ch->player.time.played);
    else if (!strcmp(playerfile_table[i].column_name, "PfIn"))
    {
      if(POOFIN(ch))
        parameters[i].string_data = strdup(POOFIN(ch));
      else
        parameters[i].string_data = strdup("Poof in nothing");
    }
    else if (!strcmp(playerfile_table[i].column_name, "PfOt")) {
      if(POOFOUT(ch))
        parameters[i].string_data = strdup(POOFOUT(ch));
      else
        parameters[i].string_data = strdup("Poof out nothing");
    }
    else if (!strcmp(playerfile_table[i].column_name, "Pref_0"))
      parameters[i].int_data = (PRF_FLAGS(ch)[0]);
    else if (!strcmp(playerfile_table[i].column_name, "Pref_1"))
      parameters[i].int_data = (PRF_FLAGS(ch)[1]);
    else if (!strcmp(playerfile_table[i].column_name, "Pref_2"))
      parameters[i].int_data = (PRF_FLAGS(ch)[2]);
    else if (!strcmp(playerfile_table[i].column_name, "Pref_3"))
      parameters[i].int_data = (PRF_FLAGS(ch)[3]);
/*
    else if (!strcmp(playerfile_table[i].column_name, "PreTitle"))
    {
      if(GET_PRETITLE(ch))
          parameters[i].string_data = strdup(GET_PRETITLE(ch));
      else
         parameters[i].string_data = strdup("");
    }
*/
    else if (!strcmp(playerfile_table[i].column_name, "Qstp"))
      parameters[i].int_data = (GET_QUESTPOINTS(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Qcur"))
      parameters[i].int_data = (GET_QUEST(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Qcnt"))
      parameters[i].int_data = (GET_QUEST_COUNTER(ch));
//    else if (!strcmp(playerfile_table[i].column_name, "Race"))
//      parameters[i].int_data = (GET_RACE(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Room"))
      parameters[i].int_data = (GET_LOADROOM(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Sex"))
      parameters[i].int_data = (GET_SEX(ch));
    else if (!strcmp(playerfile_table[i].column_name, "ScrW"))
      parameters[i].int_data = (GET_SCREEN_WIDTH(ch) );
    else if (!strcmp(playerfile_table[i].column_name, "Str"))
      parameters[i].int_data = (ch->real_abils.str);
    else if (!strcmp(playerfile_table[i].column_name, "Thir"))
      parameters[i].int_data = (GET_COND(ch, THIRST));
    else if (!strcmp(playerfile_table[i].column_name, "Thr1"))
      parameters[i].int_data = (GET_SAVE(ch, 0));
    else if (!strcmp(playerfile_table[i].column_name, "Thr2"))
      parameters[i].int_data = (GET_SAVE(ch, 1));
    else if (!strcmp(playerfile_table[i].column_name, "Thr3"))
      parameters[i].int_data = (GET_SAVE(ch, 2));
    else if (!strcmp(playerfile_table[i].column_name, "Thr4"))
      parameters[i].int_data = (GET_SAVE(ch, 3));
    else if (!strcmp(playerfile_table[i].column_name, "Thr5"))
      parameters[i].int_data = (GET_SAVE(ch, 4));
    else if (!strcmp(playerfile_table[i].column_name, "Titl"))
    {
      if(GET_TITLE(ch))
          parameters[i].string_data = strdup(GET_TITLE(ch));
      else
         parameters[i].string_data = strdup("No title.");
    }
    else if (!strcmp(playerfile_table[i].column_name, "Wate"))
      parameters[i].int_data = (GET_WEIGHT(ch));
/*
    else if (!strcmp(playerfile_table[i].column_name, "WhoTitle"))
    {
      if(GET_WHOTITLE(ch))
          parameters[i].string_data = strdup(GET_WHOTITLE(ch));
      else
         parameters[i].string_data = strdup("");
    }
*/
    else if (!strcmp(playerfile_table[i].column_name, "Wimp"))
      parameters[i].int_data = (GET_WIMP_LEV(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Wis"))
      parameters[i].int_data = (ch->real_abils.wis);
    else if (i == (num_parameters - 1))
    {
      /* assign ID to last element */
      parameters[i].int_data = (ID);
      parameters[i].data_type = MYSQL_TYPE_LONG;
    }
    else
    {
      parameters[i].string_data = strdup("Unknown");
      parameters[i].data_type = MYSQL_TYPE_VAR_STRING;
      log("Unknown parameter %s in update_playerfile_mysql_by_ID", playerfile_table[i].column_name);
    }

    if(parameters[i].data_type == MYSQL_TYPE_VAR_STRING && parameters[i].string_data)
        parameters[i].data_length = strlen(parameters[i].string_data)+1;
    else
        parameters[i].data_length = 0;
  }

  query_stmt_mysql(conn, parameters, NULL, sql_buf, 0, num_parameters, NULL, ch, MYSQL_QUERY_UPDATE);

  for(i = 0; i < num_parameters; i++)
  {
    if(EXTRA_MYSQL_DEBUG_LOGS)
      log("MYSQLINFO: column: %s, Parameter %d, int_data: %d, string_data: %s", playerfile_table[i].column_name,
        i, parameters[i].int_data,
          parameters[i].data_type == MYSQL_TYPE_VAR_STRING ? (char*)parameters[i].string_data : "not string");
  }
  free_mysql_parameters(parameters, num_parameters);
}

int select_player_mysql_by_name(MYSQL *conn, const char *name, struct char_data *ch)
{
  int num_columns, num_parameters;
  char sql_buf[MAX_STRING_LENGTH];
  char buf[MAX_STRING_LENGTH];
  struct mysql_parameter *parameters;
  const struct mysql_column *pft = playerfile_table;

  snprintf(sql_buf, sizeof(sql_buf), "SELECT ");
  num_columns = get_column_sql(sql_buf, sizeof(sql_buf), playerfile_table);

  snprintf(buf, sizeof(buf), " FROM %s.%s WHERE Name = ?", MYSQL_DB, MYSQL_PLAYER_TABLE);

  size_t remaining = sizeof(sql_buf) - strlen(sql_buf) - 1;

  if (strlen(buf) >= remaining) {
    log("MYSQL ERROR: Buffer overflow prevented.");
    return 0;
  }

  strncat(sql_buf, buf, remaining);

  log("MYSQLINFO: %s", sql_buf);

  num_parameters = 1;
  //create a parameter list (which is just the character's name here)
  CREATE(parameters, struct mysql_parameter, num_parameters);
  parameters[0].string_data = strdup(name);
  parameters[0].data_type = MYSQL_TYPE_VAR_STRING;
  parameters[0].data_length = strlen(name);
  parameters[0].int_data = 0;

  query_stmt_mysql(conn, parameters, pft, sql_buf, num_columns, num_parameters, load_playerfile_mysql, ch, MYSQL_QUERY_SELECT);

  free_mysql_parameters(parameters, num_parameters);
  return 1;
}

/* load_char_mysql initializes the player saved structures and reads data from the mysql tables.
 * This calls functions to query and load player aliases, variables, triggers, and
 * arrays (skills/feats/kills, etc).
 * Returns TRUE if loaded, FALSE if not. */
int load_char_mysql(const char *name, struct char_data *ch)
{
  int id, i;

  if ((id = get_ptable_by_name(name)) < 0)
    return (-1);
  else {

     /* Character initializations. Necessary to keep some things straight. */
    ch->affected = NULL;
    for (i = 1; i <= MAX_SKILLS; i++)
      GET_SKILL(ch, i) = 0;
    GET_SEX(ch) = PFDEF_SEX;
    GET_CLASS(ch) = PFDEF_CLASS;
    GET_LEVEL(ch) = PFDEF_LEVEL;
    GET_HEIGHT(ch) = PFDEF_HEIGHT;
    GET_WEIGHT(ch) = PFDEF_WEIGHT;
    GET_ALIGNMENT(ch) = PFDEF_ALIGNMENT;
    for (i = 0; i < NUM_OF_SAVING_THROWS; i++)
      GET_SAVE(ch, i) = PFDEF_SAVETHROW;
    GET_LOADROOM(ch) = PFDEF_LOADROOM;
    GET_INVIS_LEV(ch) = PFDEF_INVISLEV;
    GET_FREEZE_LEV(ch) = PFDEF_FREEZELEV;
    GET_WIMP_LEV(ch) = PFDEF_WIMPLEV;
    GET_COND(ch, HUNGER) = PFDEF_HUNGER;
    GET_COND(ch, THIRST) = PFDEF_THIRST;
    GET_COND(ch, DRUNK) = PFDEF_DRUNK;
    GET_BAD_PWS(ch) = PFDEF_BADPWS;
    GET_PRACTICES(ch) = PFDEF_PRACTICES;
    GET_GOLD(ch) = PFDEF_GOLD;
    GET_BANK_GOLD(ch) = PFDEF_BANK;
    GET_EXP(ch) = PFDEF_EXP;
    GET_HITROLL(ch) = PFDEF_HITROLL;
    GET_DAMROLL(ch) = PFDEF_DAMROLL;
    GET_AC(ch) = PFDEF_AC;
    ch->real_abils.str = PFDEF_STR;
    ch->real_abils.dex = PFDEF_DEX;
    ch->real_abils.intel = PFDEF_INT;
    ch->real_abils.wis = PFDEF_WIS;
    ch->real_abils.con = PFDEF_CON;
    ch->real_abils.cha = PFDEF_CHA;
    GET_HIT(ch) = PFDEF_HIT;
    GET_MAX_HIT(ch) = PFDEF_MAXHIT;
    GET_MANA(ch) = PFDEF_MANA;
    GET_MAX_MANA(ch) = PFDEF_MAXMANA;
    GET_MOVE(ch) = PFDEF_MOVE;
    GET_MAX_MOVE(ch) = PFDEF_MAXMOVE;
//    GET_CLAN(ch) = PFDEF_CLAN;
//    GET_CLAN_RANK(ch) = PFDEF_CLANRANK;
//    GET_RACE(ch) = PFDEF_RACE;
//    GET_GOD(ch) = PFDEF_GOD;
    GET_OLC_ZONE(ch) = PFDEF_OLC;
    GET_PAGE_LENGTH(ch) = PFDEF_PAGELENGTH;
    GET_SCREEN_WIDTH(ch) = PFDEF_SCREENWIDTH;
    GET_ALIASES(ch) = NULL;
    SITTING(ch) = NULL;
    NEXT_SITTING(ch) = NULL;
    GET_QUESTPOINTS(ch) = PFDEF_QUESTPOINTS;
    GET_QUEST_COUNTER(ch) = PFDEF_QUESTCOUNT;
    GET_QUEST(ch) = PFDEF_CURRQUEST;
    GET_NUM_QUESTS(ch) = PFDEF_COMPQUESTS;
    GET_LAST_MOTD(ch) = PFDEF_LASTMOTD;
    GET_LAST_NEWS(ch) = PFDEF_LASTNEWS;

    for (i = 0; i < AF_ARRAY_MAX; i++)
      AFF_FLAGS(ch)[i] = PFDEF_AFFFLAGS;
    for (i = 0; i < PM_ARRAY_MAX; i++)
     PLR_FLAGS(ch)[i] = PFDEF_PLRFLAGS;
    for (i = 0; i < PR_ARRAY_MAX; i++)
      PRF_FLAGS(ch)[i] = PFDEF_PREFFLAGS;

    /* checking the database for the Name (this should be ID later, but currently the player
       id comes from the playerfile instead of the id in the index for some reason)
    */
    select_player_mysql_by_name(db_conn, name, ch);
    select_player_arrays_mysql(db_conn, ch, PLAYER_ARRAY_SKILLS);
    select_player_arrays_mysql(db_conn, ch, PLAYER_ARRAY_QUESTS);
    select_player_arrays_mysql(db_conn, ch, PLAYER_ARRAY_COOLDOWNS);
    select_player_arrays_mysql(db_conn, ch, PLAYER_ARRAY_KILLS);
//    select_player_arrays_mysql(db_conn, ch, PLAYER_ARRAY_FEATS);
//    select_player_arrays_mysql(db_conn, ch, PLAYER_ARRAY_MEMORIZATION);
    select_aliases_mysql(db_conn, ch);
    select_player_vars_mysql(db_conn, ch);

/* TODO:
        else if (!strcmp(tag, "Trig") && CONFIG_SCRIPT_PLAYERS) {
          if ((t_rnum = real_trigger(atoi(line))) != NOTHING) {
            t = read_trigger(t_rnum);
          if (!SCRIPT(ch))
            CREATE(SCRIPT(ch), struct script_data, 1);
          add_trigger(SCRIPT(ch), t, -1);
          }
         }
*/

  }

  affect_total(ch);

  /* initialization for imms */
  if (GET_LEVEL(ch) >= LVL_IMMORT) {
    for (i = 1; i <= MAX_SKILLS; i++)
      GET_SKILL(ch, i) = 100;
    GET_COND(ch, HUNGER) = -1;
    GET_COND(ch, THIRST) = -1;
    GET_COND(ch, DRUNK) = -1;
  }
  return(id);
}

void save_player_mysql(struct char_data * ch, struct affected_type *aff)
{
  MYSQL_RES *result;
  MYSQL_ROW row;
  char buf[MAX_STRING_LENGTH];

  /* checking the database for the character ... has to be name for now */
  snprintf(buf, sizeof(buf), "SELECT ID FROM %s.%s WHERE Name = '%s'", MYSQL_DB, MYSQL_PLAYER_TABLE, GET_NAME(ch));

  mysql_ping(db_conn);

  if (mysql_query(db_conn, buf))
  {
    close_mysqlcon_with_error(db_conn);
    log("MYSQLINFO: %s", buf);
    return;
  }

  result = mysql_store_result(db_conn);

  if (result == NULL)
  {
    close_mysqlcon_with_error(db_conn);
    log("MYSQLINFO: %s", buf);
    return;
  }

  row = mysql_fetch_row(result);

  if(EXTRA_MYSQL_DEBUG_LOGS) {
    snprintf(buf, sizeof(buf), "%ld mysql_numrows", (long)mysql_num_rows(result));
    log("MYSQLINFO: %s", buf);
  }

  /* if not found in the database then insert a new record with name and ID */
  if(mysql_num_rows(result) < 1)
  {
    //INSERT NEW Player File
    snprintf(buf, sizeof(buf), "INSERT INTO %s.%s (ID, Name) VALUES(%ld, '%s')", MYSQL_DB, MYSQL_PLAYER_TABLE, GET_IDNUM(ch), GET_NAME(ch));
    mysql_query(db_conn, buf);
    log("MYSQLINFO: New player: %s", buf);
  }
  mysql_free_result(result);

  update_playerfile_mysql_by_ID(db_conn, GET_IDNUM(ch), ch, aff);

  insert_player_vars_mysql(db_conn, ch);
  insert_aliases_mysql(db_conn, ch);
  insert_player_arrays_mysql(db_conn, ch, PLAYER_ARRAY_SKILLS);
  insert_player_arrays_mysql(db_conn, ch, PLAYER_ARRAY_QUESTS);
//  insert_player_arrays_mysql(db_conn, ch, PLAYER_ARRAY_COOLDOWNS);
//  insert_player_arrays_mysql(db_conn, ch, PLAYER_ARRAY_KILLS);
//  insert_player_arrays_mysql(db_conn, ch, PLAYER_ARRAY_FEATS);
//  insert_player_arrays_mysql(db_conn, ch, PLAYER_ARRAY_MEMORIZATION);
}

/* Write the vital data of a player to mysql playerfile table. */
void save_char_mysql(struct char_data * ch)
{
  int i, j;
  struct affected_type *aff;
  struct obj_data *char_eq[NUM_WEARS];

  if (IS_NPC(ch) || GET_PFILEPOS(ch) < 0)
    return;

  /* If ch->desc is not null, then update session data before saving. */
  if (ch->desc) {
    if (*ch->desc->host) {
      if (!GET_HOST(ch))
        GET_HOST(ch) = strdup(ch->desc->host);
      else if (GET_HOST(ch) && strcmp(GET_HOST(ch), ch->desc->host)) {
        free(GET_HOST(ch));
        GET_HOST(ch) = strdup(ch->desc->host);
      }
    }

    /* Only update the time.played and time.logon if the character is playing. */
    if (STATE(ch->desc) == CON_PLAYING) {
      ch->player.time.played += time(0) - ch->player.time.logon;
      ch->player.time.logon = time(0);
    }
  }

  /* Unaffect everything a character can be affected by. */
  for (i = 0; i < NUM_WEARS; i++) {
    if (GET_EQ(ch, i)) {
      char_eq[i] = unequip_char(ch, i);
#ifndef NO_EXTRANEOUS_TRIGGERS
      remove_otrigger(char_eq[i], ch);
#endif
    }
    else
      char_eq[i] = NULL;
  }

  struct affected_type *stored_aff = NULL;
  struct affected_type *stored_affs = NULL;
  struct affected_type *list_pointer = NULL;

  for(aff = ch->affected; aff; aff = aff->next)
  {
    CREATE(stored_aff, struct affected_type, 1);

    stored_aff->spell = aff->spell;
    stored_aff->duration = aff->duration;
    stored_aff->modifier = aff->modifier;
    stored_aff->location = aff->location;

    for(j = 0; j < AF_ARRAY_MAX; j++)
      stored_aff->bitvector[j] = aff->bitvector[j];

    stored_aff->next = NULL;

    if(stored_affs == NULL)
        stored_affs = stored_aff;
    else
    {
        list_pointer = stored_affs;
        while(list_pointer->next != NULL)
            list_pointer = list_pointer->next;
        list_pointer->next = stored_aff;
    }
  }

  /* Remove the affections so that the raw values are stored; otherwise the
   * effects are doubled when the char logs back in. */
  while (ch->affected)
    affect_remove(ch, ch->affected);

  if ((i >= MAX_AFFECT) && aff && aff->next)
    log("SYSERR: WARNING: OUT OF STORE ROOM FOR AFFECTED TYPES!!!");

  ch->aff_abils = ch->real_abils;
  /* end char_to_store code */

  save_player_mysql(ch, stored_affs);

// if (SCRIPT(ch)) {
//   for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next)
//   fprintf(fl, "Trig: %d\n",GET_TRIG_VNUM(t));
//}

  /* reaffect the stored affs */
  for(stored_aff = stored_affs; stored_aff; stored_aff = stored_aff->next)
  {
    if(stored_aff && stored_aff->spell)
    {
      log("Affecting back %d %d %d %d %d %d %d %d", stored_aff->spell,  stored_aff->duration, stored_aff->modifier, stored_aff->location,
          stored_aff->bitvector[0], stored_aff->bitvector[1], stored_aff->bitvector[2], stored_aff->bitvector[3]);
      affect_to_char(ch, stored_aff);
    }
  }
  //free temp storage
  list_pointer = stored_affs;
  while(stored_affs)
  {
     list_pointer = stored_affs;
     stored_affs = stored_affs->next;
     if(list_pointer)
       free(list_pointer);
  }

  for (i = 0; i < NUM_WEARS; i++) {
    if (char_eq[i])
#ifndef NO_EXTRANEOUS_TRIGGERS
        if (wear_otrigger(char_eq[i], ch, i))
#endif
    equip_char(ch, char_eq[i], i);
#ifndef NO_EXTRANEOUS_TRIGGERS
          else
          obj_to_char(char_eq[i], ch);
#endif
  }
  /* end char_to_store code */
}

/* Deletes a player of idnum from the db */
int delete_player_mysql(MYSQL *conn, int id)
{
  char sql_buf[MAX_STRING_LENGTH];
  struct mysql_parameter *parameters;
  int num_parameters = 1, result;

  snprintf(sql_buf, sizeof(sql_buf), "DELETE FROM %s.%s WHERE Id = ?", MYSQL_DB, MYSQL_PLAYER_TABLE);

  log("MYSQLINFO: %s | num_parameters:%d", sql_buf, num_parameters);

  //create a parameter list (which is just the character's name here)
  CREATE(parameters, struct mysql_parameter, num_parameters);
  parameters[0].data_length = 0;
  parameters[0].data_type = MYSQL_TYPE_LONG;
  parameters[0].int_data = id;

  result = query_stmt_mysql(conn, parameters, NULL, sql_buf, 0, num_parameters, NULL, NULL, MYSQL_QUERY_DELETE);

  free_mysql_parameters(parameters, num_parameters);
  return result;
}

/* TODO: unfinished conversion to mysql */
/* Stuff related to the player file cleanup system. */
/* remove_player() removes all files associated with a player who is self-deleted,
 * deleted by an immortal, or deleted by the auto-wipe system (if enabled). */
void remove_player_mysql(int pfilepos)
{
  char filename[MAX_STRING_LENGTH], timestr[25];
  int i, id;

  if (!*player_table[pfilepos].name)
    return;
  id = player_table[pfilepos].id;
  /* Unlink all player-owned files */
  for (i = 0; i < MAX_FILES; i++) {
    if (get_filename(filename, sizeof(filename), i, player_table[pfilepos].name))
      unlink(filename);
  }

  strftime(timestr, sizeof(timestr), "%c", localtime(&(player_table[pfilepos].last)));
  log("PCLEAN: %s Lev: %d Last: %s",
	player_table[pfilepos].name, player_table[pfilepos].level,
	timestr);
  player_table[pfilepos].name[0] = '\0';

  /* Update index table. */
  remove_player_from_index(pfilepos);

/*  TODO: need to make sure the db is updated
    we could set a deleted flag or move to a backup table or backup db
    or just delete - Cyric */

  delete_player_mysql(db_conn, id);
//  update_playerfile_mysql_by_ID(db_conn, player_table[pfilepos].id, ch, aff);
  delete_player_vars_mysql(db_conn, id);
  delete_aliases_mysql(db_conn, id);
  delete_player_arrays_mysql(db_conn, id, PLAYER_ARRAY_SKILLS);
  delete_player_arrays_mysql(db_conn, id, PLAYER_ARRAY_QUESTS);
//  delete_player_arrays_mysql(db_conn, id, PLAYER_ARRAY_COOLDOWNS);
//  delete_player_arrays_mysql(db_conn, id, PLAYER_ARRAY_KILLS);
//  delete_player_arrays_mysql(db_conn, id, PLAYER_ARRAY_FEATS);
//  delete_player_arrays_mysql(db_conn, id, PLAYER_ARRAY_MEMORIZATION);
//  delete_player_ojbects(GET_IDNUM(ch);
}

/* TODO: unfinished */
void clean_pfiles_mysql(void)
{
  int i, ci;

  for (i = 0; i <= top_of_p_table; i++) {
    /* We only want to go further if the player isn't protected from deletion
     * and hasn't already been deleted. */
    if (!IS_SET(player_table[i].flags, PINDEX_NODELETE) &&
        *player_table[i].name) {
      /* If the player is already flagged for deletion, then go ahead and get
       * rid of him. */
      if (IS_SET(player_table[i].flags, PINDEX_DELETED)) {
	remove_player(i);
      } else {
        /* Check to see if the player has overstayed his welcome based on level. */
	for (ci = 0; pclean_criteria[ci].level > -1; ci++) {
	  if (player_table[i].level <= pclean_criteria[ci].level &&
	      ((time(0) - player_table[i].last) >
	       (pclean_criteria[ci].days * SECS_PER_REAL_DAY))) {
	    remove_player(i);
	    break;
	  }
	}
        /* If we got this far and the players hasn't been kicked out, then he
	 * can stay a little while longer. */
      }
    }
  }
  /* After everything is done, we should rebuild player_index and remove the
   * entries of the players that were just deleted. */
}

/* load_affects function uses 8 arguments from a char pointer instead of a file now */
static void load_affects(char *affects_str, struct char_data *ch)
{
  int num = 0, num2 = 0, num3 = 0, num4 = 0, num5 = 0, num6 = 0, num7 = 0, num8 = 0, i, j, n_vars, aff_start, affects_str_len;
  char line[MAX_INPUT_LENGTH + 1];
  struct affected_type af;

  line[0] = '\0';
  log("MYSQLINFO: affects string: %s", affects_str);
  aff_start = 0;
  affects_str_len = strlen(affects_str);
  for(i = 0; i < affects_str_len; i++)
  {
    if(affects_str[i] == ':')
    {
      for(j = aff_start; j < i; j++)
      {
        line[j-aff_start] = affects_str[j];
      }
      aff_start = i+1;
      line[j+1] = '\0';
      new_affect(&af);
      n_vars = sscanf(line, "%d,%d,%d,%d,%d,%d,%d,%d", &num, &num2, &num3, &num4, &num5, &num6, &num7, &num8);
      if (num > 0)
      {
        af.spell = num;
        af.duration = num2;
        af.modifier = num3;
        af.location = num4;
        af.bitvector[0] = num5;
        af.bitvector[1] = num6;
        af.bitvector[2] = num7;
        af.bitvector[3] = num8;
        if(n_vars != 8)
          log("SYSERR: Invalid affects in database (%s), expecting 8 values", GET_NAME(ch));
        affect_to_char(ch, &af);
      }
    }
  }
}

const struct mysql_column player_arrays_table_index[] =
{
  { "PlayerId",       MYSQL_TYPE_LONG           },
  { "ArrayId",        MYSQL_TYPE_LONG           },
  { "Value",          MYSQL_TYPE_LONG           },
  { "Type",           MYSQL_TYPE_LONG           },
  { "\n",             MYSQL_TYPE_LONG           }
};

/* Deletes all player arrays for a character from the database, not memory */
void delete_player_arrays_mysql(MYSQL *conn, int id, int type)
{
  char sql_buf[MAX_STRING_LENGTH];
  struct mysql_parameter *parameters;
  int num_parameters = 2;

  snprintf(sql_buf, sizeof(sql_buf), "DELETE FROM %s.%s WHERE PlayerId = ? AND Type = ?", MYSQL_DB, MYSQL_PLAYER_ARRAYS_TABLE);

  log("MYSQLINFO: %s | num_parameters:%d", sql_buf, num_parameters);

  //create a parameter list (which is just the character's name here)
  CREATE(parameters, struct mysql_parameter, num_parameters);
  parameters[0].data_length = 0;
  parameters[0].data_type = MYSQL_TYPE_LONG;
  parameters[0].int_data = id;

  parameters[1].data_length = 0;
  parameters[1].data_type = MYSQL_TYPE_LONG;
  parameters[1].int_data = type;

  query_stmt_mysql(conn, parameters, NULL, sql_buf, 0, num_parameters, NULL, NULL, MYSQL_QUERY_DELETE);

  free_mysql_parameters(parameters, num_parameters);
}

/* save a character's variables to the database */
void insert_player_arrays_mysql(MYSQL *conn, struct char_data *ch, int type)
{
  char sql_buf[MAX_STRING_LENGTH];
  char value_buf[MAX_STRING_LENGTH] = "\0";
  char buf[MAX_STRING_LENGTH] = "\0";
  struct mysql_parameter *parameters;
  int num_parameters = 0, num_columns = 0, num_rows = 0, col_num, i = 0, j = 0;
  const struct mysql_column *col = player_arrays_table_index;
//  struct cooldown_node *cd;
//  struct memorize_node *mem;
//  struct kill_node *kill = NULL;

  /* we should never be called for an NPC, but just in case... */
  if (IS_NPC(ch)) return;

  delete_player_arrays_mysql(conn, GET_IDNUM(ch), type);

  num_columns = get_column_sql(buf, sizeof(buf), player_arrays_table_index);

  switch(type) {
  case PLAYER_ARRAY_SKILLS:
    if (GET_LEVEL(ch) < LVL_IMMORT) {
      for (i = 1; i <= MAX_SKILLS; i++) {
        if (GET_SKILL(ch, i))
          num_rows++;
      }
    }
    break;
  case PLAYER_ARRAY_QUESTS:
    num_rows = GET_NUM_QUESTS(ch);
    break;
/*
  case PLAYER_ARRAY_COOLDOWNS:
    for (cd = ch->cooldown; cd; cd = cd->next)
      num_rows++;
    break;
*/
/*
  case PLAYER_ARRAY_KILLS:
    for (kill = ch->kill_mem; kill; kill = kill->next)
      num_rows++;
    break;
*/
//  case PLAYER_ARRAY_FEATS:
    /* don't include FEAT_NONE */
//    for(i = 1; i < MAX_FEATS; i++)
//      if(GET_FEAT(ch, i) > 0) /* TODO: test more */
//        num_rows++;
//    break;
//  case PLAYER_ARRAY_MEMORIZATION:
//    for(i = 0; i < GET_MEMCURSOR(ch); i++)
//      if(GET_SPELLMEM(ch, i) > 0)
//        num_rows++;
//    for (mem = ch->memorized; mem; mem = mem->next)
//      num_rows++;
//    break;
  default:
    log("Unhandled case in insert_player_arrays_mysql");
    break;
  }
  if(EXTRA_MYSQL_DEBUG_LOGS)
    log("MYSQLINFO: num rows %d", num_rows);

  if(num_rows < 1)
    return;
  num_parameters = get_parameter_markers_sql(value_buf, sizeof(value_buf), num_columns, num_rows);

  snprintf(sql_buf, sizeof(sql_buf), "INSERT INTO %s.%s (%s) VALUES %s",
    MYSQL_DB, MYSQL_PLAYER_ARRAYS_TABLE, buf, value_buf);

  log("MYSQLINFO: %s | num_parameter:%d", sql_buf, num_parameters);

  CREATE(parameters, struct mysql_parameter, num_parameters);

  switch(type) {
  case PLAYER_ARRAY_SKILLS:
    j = 0;
    for(i = 0; i < num_parameters; i++)
    {
      col_num = i % num_columns;

      if(col_num == 0) {
        j++;
        while ((!GET_SKILL(ch, j)) && j <= MAX_SKILLS)  {
            j++;
        }
      }

      parameters[i].data_type = col[col_num].data_type;

      if(!strcmp(col[col_num].column_name, "PlayerId"))
        parameters[i].int_data = GET_IDNUM(ch);
      else if(!strcmp(col[col_num].column_name, "ArrayId"))
        parameters[i].int_data = j;
      else if(!strcmp(col[col_num].column_name, "Value"))
        parameters[i].int_data = GET_SKILL(ch, j);
      else if(!strcmp(col[col_num].column_name, "Type"))
        parameters[i].int_data = type;

      parameters[i].data_length = 0;

    }
    break;
  case PLAYER_ARRAY_QUESTS:
    j = 0;
    for(i = 0; i < num_parameters; i++)
    {
      col_num = i % num_columns;

      parameters[i].data_type = col[col_num].data_type;

      if(!strcmp(col[col_num].column_name, "PlayerId"))
        parameters[i].int_data = GET_IDNUM(ch);
      else if(!strcmp(col[col_num].column_name, "ArrayId"))
        parameters[i].int_data = ch->player_specials->saved.completed_quests[j];
      else if(!strcmp(col[col_num].column_name, "Value"))
        parameters[i].int_data = 1;
      else if(!strcmp(col[col_num].column_name, "Type"))
        parameters[i].int_data = type;

      parameters[i].data_length = 0;

      if(col_num == (num_columns - 1))
        j++;
    }
    break;
/*
  case PLAYER_ARRAY_COOLDOWNS:
    cd = ch->cooldown;

    for(i = 0; i < num_parameters; i++)
    {
      col_num = i % num_columns;

      parameters[i].data_type = col[col_num].data_type;

      if(!strcmp(col[col_num].column_name, "PlayerId"))
        parameters[i].int_data = GET_IDNUM(ch);
      else if(!strcmp(col[col_num].column_name, "ArrayId"))
        parameters[i].int_data = cd->spellnum;
      else if(!strcmp(col[col_num].column_name, "Value"))
        parameters[i].int_data = cd->timer;
      else if(!strcmp(col[col_num].column_name, "Type"))
        parameters[i].int_data = type;

      parameters[i].data_length = 0;

      if(col_num == (num_columns - 1))
        cd = cd->next;
    }
    break;
*/
/*
  case PLAYER_ARRAY_KILLS:
    kill = ch->kill_mem;

    for(i = 0; i < num_parameters; i++)
    {
      col_num = i % num_columns;

      parameters[i].data_type = col[col_num].data_type;

      if(!strcmp(col[col_num].column_name, "PlayerId"))
        parameters[i].int_data = GET_IDNUM(ch);
      else if(!strcmp(col[col_num].column_name, "ArrayId"))
        parameters[i].int_data = kill->vnum;
      else if(!strcmp(col[col_num].column_name, "Value"))
        parameters[i].int_data = kill->amount;
      else if(!strcmp(col[col_num].column_name, "Type"))
        parameters[i].int_data = type;

      parameters[i].data_length = 0;

      if(col_num == (num_columns - 1))
        kill = kill->next;
    }
    break;
*/
#ifdef PLAYER_FEATS
  case PLAYER_ARRAY_FEATS:
    j = 0;
    for(i = 0; i < num_parameters; i++)
    {
      col_num = i % num_columns;

      if(col_num == 0) {
        j++;
        while ((!GET_FEAT(ch, j)) && j <= MAX_FEATS)  {
            j++;
        }
      }

      parameters[i].data_type = col[col_num].data_type;

      if(!strcmp(col[col_num].column_name, "PlayerId"))
        parameters[i].int_data = GET_IDNUM(ch);
      else if(!strcmp(col[col_num].column_name, "ArrayId"))
        parameters[i].int_data = j;
      else if(!strcmp(col[col_num].column_name, "Value"))
        parameters[i].int_data = GET_FEAT(ch, j);
      else if(!strcmp(col[col_num].column_name, "Type"))
        parameters[i].int_data = type;

      parameters[i].data_length = 0;
    }
    break;
#endif
#ifdef PLAYER_MEMORIZATION
  case PLAYER_ARRAY_MEMORIZATION:
    j = -1;
    mem = ch->memorized;

    for(i = 0; i < num_parameters; i++)
    {
      col_num = i % num_columns;

      if(col_num == 0) {
        j++;
        while ((!GET_SPELLMEM (ch, j)) && j < GET_MEMCURSOR (ch))  {
            j++;
        }
      }

      parameters[i].data_type = col[col_num].data_type;

      if(!strcmp(col[col_num].column_name, "PlayerId"))
        parameters[i].int_data = GET_IDNUM(ch);
      else if(!strcmp(col[col_num].column_name, "ArrayId"))
        parameters[i].int_data = (j < GET_MEMCURSOR(ch)) ? GET_SPELLMEM(ch, j) : mem->spell;
      else if(!strcmp(col[col_num].column_name, "Value"))
        parameters[i].int_data = (j < GET_MEMCURSOR(ch)) ? 0 : mem->timer;
      else if(!strcmp(col[col_num].column_name, "Type"))
        parameters[i].int_data = type;

      parameters[i].data_length = 0;

      if(col_num == (num_columns - 1))
        if( j >= GET_MEMCURSOR(ch) )
          mem = mem->next;
    }
    break;
#endif
  default:
    log("Unhandled case in insert_player_arrays_mysql");
    break;
  }

  query_stmt_mysql(conn, parameters, NULL, sql_buf, 0, num_parameters, NULL, ch, MYSQL_QUERY_INSERT);

  free_mysql_parameters(parameters, num_parameters);
}

void select_player_arrays_mysql(MYSQL *conn, struct char_data *ch, int type)
{
  int num_parameters, num_columns;
  char sql_buf[MAX_STRING_LENGTH];
  char buf[MAX_STRING_LENGTH];
  const struct mysql_column *pvt = player_arrays_table_index;
  struct mysql_parameter *parameters;

  snprintf(sql_buf, sizeof(sql_buf), "SELECT ");
  num_columns = get_column_sql(sql_buf, sizeof(sql_buf), player_arrays_table_index);

  snprintf(buf, sizeof(buf) - 1, " FROM %s.%s WHERE PlayerId = ? AND Type = ?",
    MYSQL_DB, MYSQL_PLAYER_ARRAYS_TABLE);

  size_t remaining = sizeof(sql_buf) - strlen(sql_buf) - 1;
  
  if (strlen(buf) >= remaining) {
    log("MYSQL ERROR: Buffer overflow prevented.");
    return;
  }

  strncat(sql_buf, buf, remaining);

  log("MYSQLINFO: %s", sql_buf);

  num_parameters = 2;

  //create a parameter list (which is just the character's name here)
  CREATE(parameters, struct mysql_parameter, num_parameters);
  parameters[0].int_data = GET_IDNUM(ch);
  parameters[0].data_type = MYSQL_TYPE_LONG;
  parameters[0].data_length = 0;

  parameters[1].int_data = type;
  parameters[1].data_type = MYSQL_TYPE_LONG;
  parameters[1].data_length = 0;

  query_stmt_mysql(conn, parameters, pvt, sql_buf, num_columns, num_parameters, read_player_arrays_mysql, ch, MYSQL_QUERY_SELECT);

  free_mysql_parameters(parameters, num_parameters);
}

void read_player_arrays_mysql(struct mysql_bind_column *fields, int num_fields, int num_rows, void *v_ch, MYSQL_STMT *stmt)
{
  struct char_data *ch = v_ch;
  int i, value = 0, typeVar = 0, arrayId = 0;

  if (num_rows == 0) {
    return;
  }

  while (!mysql_stmt_fetch(stmt))
  {
    for(i = 0; i < num_fields; i++)
    {
      if(EXTRA_MYSQL_DEBUG_LOGS)
        log("MYSQLINFO: Field name: %s, int_buffer: %d, string_buffer: %s",
            fields[i].name, fields[i].col_int_buffer,
            fields[i].col_string_buffer);

      if (!strcmp(fields[i].name, "PlayerId"))
        continue;
      else if (!strcmp(fields[i].name, "ArrayId"))
         arrayId = fields[i].col_int_buffer;
      else if (!strcmp(fields[i].name, "Value"))
        value = fields[i].col_int_buffer;
      else if (!strcmp(fields[i].name, "Type"))
        typeVar = fields[i].col_int_buffer;
    }
    switch(typeVar) {
    case PLAYER_ARRAY_SKILLS:
      GET_SKILL(ch, arrayId) = value;
      break;
    case PLAYER_ARRAY_QUESTS:
      //value is unused, could be a quantity or something later
      add_completed_quest(ch, arrayId);
      break;
/*
    case PLAYER_ARRAY_COOLDOWNS:
      cooldown_add(ch, arrayId, value);
      break;
*/
//    case PLAYER_ARRAY_KILLS:
      /* 4th argument (end) is false so the list isn't reversed */
//      kill_add(ch, arrayId, value, FALSE);
//      break;
#ifdef PLAYER_FEATS
    case PLAYER_ARRAY_FEATS:
      GET_FEAT (ch, arrayId) = value;
      break;
#endif
#ifdef PLAYER_MEMORIZATION
    case PLAYER_ARRAY_MEMORIZATION:
      if(i == 0)
        GET_MEMCURSOR(ch) = 0;
      if (value == 0) {
        GET_SPELLMEM(ch, GET_MEMCURSOR(ch)) = arrayId;
        GET_MEMCURSOR(ch)++;
      } else
        memorize_add(ch, arrayId, value);
      break;
#endif
    default:
      log("Unhandled case in read_player_arrays_mysql");
      break;
    }
  }
}

const struct mysql_column player_vars_table_index[] =
{
  { "PlayerID",       MYSQL_TYPE_LONG           },
  { "Name",           MYSQL_TYPE_VAR_STRING     },
  { "Value",          MYSQL_TYPE_VAR_STRING     },
  { "Context",        MYSQL_TYPE_LONG           },
  { "\n",             MYSQL_TYPE_LONG           }
};

/* Deletes all player variables for a character from the database, not memory */
void delete_player_vars_mysql(MYSQL *conn, int id)
{
  char sql_buf[MAX_STRING_LENGTH];
  struct mysql_parameter *parameters;
  int num_parameters = 1;

  snprintf(sql_buf, sizeof(sql_buf), "DELETE FROM %s.%s WHERE PlayerId = ?", MYSQL_DB, MYSQL_PLAYER_VARS_TABLE);

  log("MYSQLINFO: %s | num_parameter:%d", sql_buf, num_parameters);

  //include the ID
  //create a parameter list (which is just the character's name here)
  CREATE(parameters, struct mysql_parameter, num_parameters);
  parameters[0].data_length = 0;
  parameters[0].data_type = MYSQL_TYPE_LONG;
  parameters[0].int_data = id;

  query_stmt_mysql(conn, parameters, NULL, sql_buf, 0, num_parameters, NULL, NULL, MYSQL_QUERY_DELETE);

  free_mysql_parameters(parameters, num_parameters);
}

/* save a character's variables to the database */
void insert_player_vars_mysql(MYSQL *conn, struct char_data *ch)
{
  struct trig_var_data *vars;
  char sql_buf[MAX_STRING_LENGTH];
  char value_buf[MAX_STRING_LENGTH] = "\0";
  char buf[MAX_STRING_LENGTH] = "\0";
  struct mysql_parameter *parameters;
  int num_parameters = 0, num_columns = 0, num_rows = 0, col_num, i = 0;
  const struct mysql_column *col = player_vars_table_index;

  /* Immediate return if no script (and therefore no variables) structure has
   * been created. this will happen when the player is logging in */
  if (SCRIPT(ch) == NULL) return;

  /* we should never be called for an NPC, but just in case... */
  if (IS_NPC(ch)) return;

  delete_player_vars_mysql(conn, GET_IDNUM(ch));

  /* make sure this char has global variables to save */
  if (ch->script->global_vars == NULL) return;

  num_columns = get_column_sql(buf, sizeof(buf), player_vars_table_index);

  for (vars = ch->script->global_vars;vars;vars = vars->next)
  {
   /* Note that currently, context will always be zero. This may change in the
    * future */
    /* Might need more testing? Worked on a sample script -Cyric */
    //if (*vars->name != '-')
      num_rows++;
  }

  num_parameters = get_parameter_markers_sql(value_buf, sizeof(value_buf), num_columns, num_rows);

  snprintf(sql_buf, sizeof(sql_buf), "INSERT INTO %s.%s (%s) VALUES %s",
    MYSQL_DB, MYSQL_PLAYER_VARS_TABLE, buf, value_buf);

  log("MYSQLINFO: %s | num_parameters:%d", sql_buf, num_parameters);

  CREATE(parameters, struct mysql_parameter, num_parameters);

  vars = ch->script->global_vars;

  for(i = 0; i < num_parameters; i++)
  {
    col_num = i % num_columns;

    parameters[i].data_type = col[col_num].data_type;

    if(!strcmp(col[col_num].column_name, "PlayerID"))
      parameters[i].int_data = GET_IDNUM(ch);
    else if(!strcmp(col[col_num].column_name, "Name"))
    {
      parameters[i].string_data = strdup(vars->name);
    }
    else if(!strcmp(col[col_num].column_name, "Value"))
      parameters[i].string_data = strdup(vars->value);
    else if(!strcmp(col[col_num].column_name, "Context"))
      parameters[i].int_data = vars->context;

    if(parameters[i].data_type == MYSQL_TYPE_VAR_STRING && parameters[i].string_data)
        parameters[i].data_length = strlen(parameters[i].string_data);
    else
        parameters[i].data_length = 0;

    if(col_num == (num_columns - 1))
      //while(vars && *vars->name != '-')
        vars = vars->next;
  }

  query_stmt_mysql(conn, parameters, NULL, sql_buf, 0, num_parameters, NULL, ch, MYSQL_QUERY_INSERT);

  free_mysql_parameters(parameters, num_parameters);
}

void select_player_vars_mysql(MYSQL *conn, struct char_data *ch)
{
  int num_parameters, num_columns;
  char sql_buf[MAX_STRING_LENGTH];
  char buf[MAX_STRING_LENGTH];
  const struct mysql_column *pvt = player_vars_table_index;
  struct mysql_parameter *parameters;

  snprintf(sql_buf, sizeof(sql_buf), "SELECT ");
  num_columns = get_column_sql(sql_buf, sizeof(sql_buf), player_vars_table_index);

  snprintf(buf, sizeof(buf), " FROM %s.%s WHERE PlayerID = ?", MYSQL_DB, MYSQL_PLAYER_VARS_TABLE);

  size_t remaining = sizeof(sql_buf) - strlen(sql_buf) - 1;

  if (strlen(buf) >= remaining) {
    log("MYSQL ERROR: Buffer overflow prevented.");
    return;
  }

  strncat(sql_buf, buf, remaining);

  log("MYSQLINFO: %s", sql_buf);

  num_parameters = 1;

  //create a parameter list (which is just the character's name here)
  CREATE(parameters, struct mysql_parameter, num_parameters);
  parameters[0].int_data = GET_IDNUM(ch);
  parameters[0].data_type = MYSQL_TYPE_LONG;
  parameters[0].data_length = 0;

  query_stmt_mysql(conn, parameters, pvt, sql_buf, num_columns, num_parameters, read_player_vars_mysql, ch, MYSQL_QUERY_SELECT);

  free_mysql_parameters(parameters, num_parameters);
}

void read_player_vars_mysql(struct mysql_bind_column *fields, int num_fields, int num_rows, void *v_ch, MYSQL_STMT *stmt)
{
  int i, context = 0;
  char buf[MAX_STRING_LENGTH];
  char *varname = NULL, *value = NULL;
  struct char_data *ch = v_ch;

  /* If getting to the menu from inside the game, the vars aren't removed. So
   * let's not allocate them again. */
  if (SCRIPT(ch))
    return;

  /* Create the space for the script structure which holds the vars. We need to
   * do this first, because later calls to 'remote' will need. A script already
   * assigned. */
  CREATE(SCRIPT(ch), struct script_data, 1);

  if (num_rows == 0) {
    return;
  }

  while (!mysql_stmt_fetch(stmt))
  {
    struct alias_data *temp;
    CREATE(temp, struct alias_data, 1);

    for(i = 0; i < num_fields; i++)
    {
      if(EXTRA_MYSQL_DEBUG_LOGS)
        log("MYSQLINFO: Field name: %s, int_buffer: %d, string_buffer: %s",
            fields[i].name, fields[i].col_int_buffer,
            fields[i].col_string_buffer);

      if (!strcmp(fields[i].name, "PlayerID"))
        continue;
      else if (!strcmp(fields[i].name, "Name"))
      {
        snprintf(buf, sizeof(buf), "%s", fields[i].col_string_buffer);
        varname = strdup(buf);
      }
      else if (!strcmp(fields[i].name, "Value"))
      {
        snprintf(buf, sizeof(buf), "%s", fields[i].col_string_buffer);
        value = strdup(buf);
      }
      else if (!strcmp(fields[i].name, "Context"))  
        context = fields[i].col_int_buffer;
    }
    add_var(&(SCRIPT(ch)->global_vars), varname, value, context);
    if(varname)
      free(varname);
    if(value)
      free(value);
  }
}

const struct mysql_column alias_table_index[] =
{
  { "PlayerID",       MYSQL_TYPE_LONG           },
  { "Alias",          MYSQL_TYPE_VAR_STRING     },
  { "Replacement",    MYSQL_TYPE_VAR_STRING     },
  { "Type",           MYSQL_TYPE_LONG           },
  { "\n",             MYSQL_TYPE_LONG           }
};

/* Deletes aliases for a character from the database, not memory */
void delete_aliases_mysql(MYSQL *conn, int id)
{
  char sql_buf[MAX_STRING_LENGTH];
  struct mysql_parameter *parameters;
  int num_parameters = 1;

  snprintf(sql_buf, sizeof(sql_buf), "DELETE FROM %s.%s WHERE PlayerId = ?", MYSQL_DB, MYSQL_ALIAS_TABLE);

  log("MYSQLINFO: %s | num_parameters:%d", sql_buf, num_parameters);

  //include the ID
  //create a parameter list (which is just the character's name here)
  CREATE(parameters, struct mysql_parameter, num_parameters);
  parameters[0].data_length = 0;
  parameters[0].data_type = MYSQL_TYPE_LONG;
  parameters[0].int_data = id;

  query_stmt_mysql(conn, parameters, NULL, sql_buf, 0, num_parameters, NULL, NULL, MYSQL_QUERY_DELETE);

  free_mysql_parameters(parameters, num_parameters);
}

static void insert_aliases_mysql(MYSQL *conn, struct char_data *ch)
{
  char sql_buf[MAX_STRING_LENGTH];
  char value_buf[MAX_STRING_LENGTH] = "\0";
  char buf[MAX_STRING_LENGTH] = "\0";
  struct mysql_parameter *parameters;
  int num_parameters = 0, num_columns = 0, num_rows = 0, col_num, i = 0;
  const struct mysql_column *col = alias_table_index;
  struct alias_data *temp;

  if (GET_ALIASES(ch) == NULL)
    return;

  num_columns = get_column_sql(buf, sizeof(buf), alias_table_index);

  for (temp = GET_ALIASES(ch); temp; temp = temp->next)
    num_rows++;

  num_parameters = get_parameter_markers_sql(value_buf, sizeof(value_buf), num_columns, num_rows);

  delete_aliases_mysql(conn, GET_IDNUM(ch));

  snprintf(sql_buf, sizeof(sql_buf), "INSERT INTO %s.%s (%s) VALUES %s",
    MYSQL_DB, MYSQL_ALIAS_TABLE, buf, value_buf);

  log("MYSQLINFO: %s | num_parameters:%d", sql_buf, num_parameters);

  CREATE(parameters, struct mysql_parameter, num_parameters);

  temp = GET_ALIASES(ch);
  for(i = 0; i < num_parameters; i++)
  {
    col_num = i % num_columns;

    parameters[i].data_type = col[col_num].data_type;

    if(!strcmp(col[col_num].column_name, "PlayerID"))
      parameters[i].int_data = GET_IDNUM(ch);
    else if(!strcmp(col[col_num].column_name, "Alias"))
    {
      parameters[i].string_data = strdup(temp->alias);
    }
    else if(!strcmp(col[col_num].column_name, "Replacement"))
      parameters[i].string_data = strdup(temp->replacement);
    else if(!strcmp(col[col_num].column_name, "Type"))
      parameters[i].int_data = temp->type;

    if(parameters[i].data_type == MYSQL_TYPE_VAR_STRING && parameters[i].string_data)
        parameters[i].data_length = strlen(parameters[i].string_data);
    else
        parameters[i].data_length = 0;

    if(col_num == (num_columns - 1))
      temp = temp->next;
  }

  query_stmt_mysql(conn, parameters, NULL, sql_buf, 0, num_parameters, NULL, ch, MYSQL_QUERY_INSERT);

  free_mysql_parameters(parameters, num_parameters);
}

static void select_aliases_mysql(MYSQL *conn, struct char_data *ch)
{
  int num_parameters, num_columns;
  char sql_buf[MAX_STRING_LENGTH];
  char buf[MAX_STRING_LENGTH];
  struct mysql_parameter *parameters;

  snprintf(sql_buf, sizeof(sql_buf), "SELECT ");

  num_columns = get_column_sql(sql_buf, sizeof(sql_buf), alias_table_index);

  snprintf(buf, sizeof(buf), " FROM %s.%s WHERE PlayerID = ?", MYSQL_DB, MYSQL_ALIAS_TABLE);

  size_t remaining = sizeof(sql_buf) - strlen(sql_buf) - 1;
  
  if (strlen(buf) >= remaining) {
    log("MYSQL ERROR: Buffer overflow prevented.");
    return;
  }

  strncat(sql_buf, buf, remaining);

  log("MYSQLINFO: %s", sql_buf);

  num_parameters = 1;

  //create a parameter list (which is just the character's name here)
  CREATE(parameters, struct mysql_parameter, num_parameters);
  parameters[0].int_data = GET_IDNUM(ch);
  parameters[0].data_type = MYSQL_TYPE_LONG;
  parameters[0].data_length = 0;

  query_stmt_mysql(conn, parameters, alias_table_index, sql_buf, num_columns, num_parameters, read_aliases_mysql, ch, MYSQL_QUERY_SELECT);

  free_mysql_parameters(parameters, num_parameters);
}

void read_aliases_mysql(struct mysql_bind_column *fields, int num_fields, int num_rows, void *v_ch, MYSQL_STMT *stmt)
{
  int i;
  char buf[MAX_STRING_LENGTH];
  struct char_data *ch = v_ch;

  if (num_rows == 0) {
    GET_ALIASES(ch) = NULL;
    return; /* No aliases in the list. */
  }

  while (!mysql_stmt_fetch(stmt))
  {
    struct alias_data *temp;
    CREATE(temp, struct alias_data, 1);

    for(i = 0; i < num_fields; i++)
    {
      if(EXTRA_MYSQL_DEBUG_LOGS)
        log("MYSQLINFO: Field name: %s, int_buffer: %d, string_buffer: %s",
            fields[i].name, fields[i].col_int_buffer,
            fields[i].col_string_buffer);

      if (!strcmp(fields[i].name, "PlayerID"))
        continue;
      else if (!strcmp(fields[i].name, "Alias"))
      {
        snprintf(buf, sizeof(buf), "%s", fields[i].col_string_buffer);
        temp->alias = strdup(buf);
      }
      else if (!strcmp(fields[i].name, "Replacement"))
      {
        snprintf(buf, sizeof(buf), "%s", fields[i].col_string_buffer);
        temp->replacement = strdup(buf);
      }
      else if (!strcmp(fields[i].name, "Type"))  temp->type = fields[i].col_int_buffer;
    }
    temp->next = GET_ALIASES(ch);
    GET_ALIASES(ch) = temp;
  }
}
