/**************************************************************************
*  File: players.c                                         Part of tbaMUD *
*  Usage: Player loading/saving and utility routines.                     *
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
#include "players.h"

#define LOAD_HIT	0
#define LOAD_MANA	1
#define LOAD_MOVE	2
#define LOAD_STRENGTH	3

#define PT_PNAME(i) (player_table[(i)].name)
#define PT_IDNUM(i) (player_table[(i)].id)
#define PT_LEVEL(i) (player_table[(i)].level)
#define PT_FLAGS(i) (player_table[(i)].flags)
#define PT_LLAST(i) (player_table[(i)].last)

/* local functions */
static void load_affects(FILE *fl, struct char_data *ch);
static void load_skills(FILE *fl, struct char_data *ch);
static void load_quests(FILE *fl, struct char_data *ch);
static void load_HMVS(struct char_data *ch, const char *line, int mode);
static void write_aliases_ascii(FILE *file, struct char_data *ch);
static void read_aliases_ascii(FILE *file, struct char_data *ch, int count);

/* global database connection - might end up removing this global connection
   originally had the idea that one connection could be used instead of repeated connections,
   but right now this is just used on initialization to see if the database is there
*/   
MYSQL *database_conn;
static int test_error(MYSQL *mysql, int status);
static int test_stmt_error(MYSQL_STMT *stmt, int status);
MYSQL *create_conn_to_mud_database(MYSQL *conn);
void load_playerfile_from_mysql(struct mysql_bind_column *fields, int num_fields, struct char_data *ch);
int query_stmt_mysql(MYSQL *conn, struct mysql_parameter_bind_adapter *parameters, struct mysql_column_bind_adapter *columns, char *statement,
                     int num_columns, int num_parameters, void (*load_function)(struct mysql_bind_column *, int, struct char_data *), 
                     struct char_data *ch, int querytype);


void get_mysql_database_conn()
{
  if(database_conn == NULL)
  {
    database_conn = mysql_init(NULL);

    /* ip address of mysql server, database, password, tablename */
    if( mysql_real_connect(database_conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASS, MYSQL_DB, 0, NULL, 0) )
      log("MYSQLINFO: Successfully connected to database.");
    else
      log("MYSQLINFO: Failure connecting to database.");
  }

  if (mysql_query(database_conn, CREATE_PLAYERFILE_TABLE))
  {
    log("MYSQLINFO: CREATE TABLE failed, %s\n", mysql_error(database_conn));
  }
}

/* New version to build player index for ASCII Player Files. Generate index
 * table for the player file. */
void build_player_index(void)
{
  int rec_count = 0, i;
  FILE *plr_index;
  char index_name[40], line[256], bits[64];
  char arg2[80];

  sprintf(index_name, "%s%s", LIB_PLRFILES, INDEX_FILE);
  if (!(plr_index = fopen(index_name, "r"))) {
    top_of_p_table = -1;
    log("No player index file!  First new char will be IMP!");
    return;
  }

  while (get_line(plr_index, line))
    if (*line != '~')
      rec_count++;
  rewind(plr_index);

  if (rec_count == 0) {
    player_table = NULL;
    top_of_p_table = -1;
    return;
  }

  CREATE(player_table, struct player_index_element, rec_count);
  for (i = 0; i < rec_count; i++) {
    get_line(plr_index, line);
    sscanf(line, "%ld %s %d %s %ld", &player_table[i].id, arg2,
      &player_table[i].level, bits, (long *)&player_table[i].last);
    CREATE(player_table[i].name, char, strlen(arg2) + 1);
    strcpy(player_table[i].name, arg2);
    player_table[i].flags = asciiflag_conv(bits);
    top_idnum = MAX(top_idnum, player_table[i].id);
  }

  fclose(plr_index);
  top_of_p_file = top_of_p_table = i - 1;
}

void build_player_index_mysql(void)
{
  int rec_count = 0, i;
  char index_name[40], line[256], bits[64];
  char arg2[80];
    
  //SELECT ID, Name, Last, Levl, Flags
    
/*  
  if(result < 1) {
    top_of_p_table = -1;
    player_table = NULL;
    log("No players found!  First new char will be IMP!");
    return;
  }
*/
/*
  while (results)
      rec_count++;
*/
    
  CREATE(player_table, struct player_index_element, rec_count);
/*
    for (i = 0; i < rec_count; i++) {
//    load another row here get name, id, level, flags, last
    CREATE(player_table[i].name, char, strlen(arg2) + 1);
    strcpy(player_table[i].name, arg2);
    player_table[i].flags = asciiflag_conv(bits);
    top_idnum = MAX(top_idnum, player_table[i].id);
  }
*/
  //close mysql stuff;
  top_of_p_file = top_of_p_table = i - 1;
}


/* Create a new entry in the in-memory index table for the player file. If the
 * name already exists, by overwriting a deleted character, then we re-use the
 * old position. */
int create_entry(char *name)
{
  int i, pos;

  if (top_of_p_table == -1) {	/* no table */
    pos = top_of_p_table = 0;
    CREATE(player_table, struct player_index_element, 1);
  } else if ((pos = get_ptable_by_name(name)) == -1) {	/* new name */
    i = ++top_of_p_table + 1;

    RECREATE(player_table, struct player_index_element, i);
    pos = top_of_p_table;
  }

  CREATE(player_table[pos].name, char, strlen(name) + 1);

  /* copy lowercase equivalent of name to table field */
  for (i = 0; (player_table[pos].name[i] = LOWER(name[i])); i++)
    /* Nothing */;

  /* clear the bitflag in case we have garbage data */
  player_table[pos].flags = 0;

  return (pos);
}


/* Remove an entry from the in-memory player index table.               *
 * Requires the 'pos' value returned by the get_ptable_by_name function */
static void remove_player_from_index(int pos)
{
  int i;

  if (pos < 0 || pos > top_of_p_table)
    return;

  /* We only need to free the name string */
  free(PT_PNAME(pos));

  /* Move every other item in the list down the index */
  for (i = pos+1; i <= top_of_p_table; i++) {
    PT_PNAME(i-1) = PT_PNAME(i);
    PT_IDNUM(i-1) = PT_IDNUM(i);
    PT_LEVEL(i-1) = PT_LEVEL(i);
    PT_FLAGS(i-1) = PT_FLAGS(i);
    PT_LLAST(i-1) = PT_LLAST(i);
  }
  PT_PNAME(top_of_p_table) = NULL;

  /* Reduce the index table counter */
  top_of_p_table--;

  /* And reduce the size of the table */
  if (top_of_p_table >= 0)
    RECREATE(player_table, struct player_index_element, (top_of_p_table+1));
  else {
    free(player_table);
    player_table = NULL;
  }
}

/* This function necessary to save a seperate ASCII player index */
void save_player_index(void)
{
  int i;
  char index_name[50], bits[64];
  FILE *index_file;

  sprintf(index_name, "%s%s", LIB_PLRFILES, INDEX_FILE);
  if (!(index_file = fopen(index_name, "w"))) {
    log("SYSERR: Could not write player index file");
    return;
  }

  for (i = 0; i <= top_of_p_table; i++)
    if (*player_table[i].name) {
      sprintascii(bits, player_table[i].flags);
      fprintf(index_file, "%ld %s %d %s %ld\n", player_table[i].id,
	player_table[i].name, player_table[i].level, *bits ? bits : "0",
        (long)player_table[i].last);
    }
  fprintf(index_file, "~\n");

  fclose(index_file);
}

void free_player_index(void)
{
  int tp;

  if (!player_table)
    return;

  for (tp = 0; tp <= top_of_p_table; tp++)
    if (player_table[tp].name)
      free(player_table[tp].name);

  free(player_table);
  player_table = NULL;
  top_of_p_table = 0;
}

long get_ptable_by_name(const char *name)
{
  int i;

  for (i = 0; i <= top_of_p_table; i++)
    if (!str_cmp(player_table[i].name, name))
      return (i);

  return (-1);
}

long get_id_by_name(const char *name)
{
  int i;

  for (i = 0; i <= top_of_p_table; i++)
    if (!str_cmp(player_table[i].name, name))
      return (player_table[i].id);

  return (-1);
}

char *get_name_by_id(long id)
{
  int i;

  for (i = 0; i <= top_of_p_table; i++)
    if (player_table[i].id == id)
      return (player_table[i].name);

  return (NULL);
}

cpp_extern struct mysql_column_bind_adapter playerfile_table[] = 
{
  { "Ac",             MYSQL_TYPE_LONG    }, 
  { "Act_0",          MYSQL_TYPE_LONG    }, // bitvector
  { "Act_1",          MYSQL_TYPE_LONG    }, // bitvector
  { "Act_2",          MYSQL_TYPE_LONG    }, // bitvector
  { "Act_3",          MYSQL_TYPE_LONG    }, // bitvector
  { "Aff_0",          MYSQL_TYPE_LONG    }, // bitvector
  { "Aff_1",          MYSQL_TYPE_LONG    }, // bitvector
  { "Aff_2",          MYSQL_TYPE_LONG    }, // bitvector
  { "Aff_3",          MYSQL_TYPE_LONG    }, // bitvector
//  { "AF_SPELL",       MYSQL_TYPE_LONG    },
//  { "AF_DURATION",    MYSQL_TYPE_LONG    },
//  { "AF_MODIFIER",    MYSQL_TYPE_LONG    },
//  { "AF_LOCATION",    MYSQL_TYPE_LONG    },
//  { "AF_BITVECTOR_0", MYSQL_TYPE_LONG    },
//  { "AF_BITVECTOR_1", MYSQL_TYPE_LONG    },
//  { "AF_BITVECTOR_2", MYSQL_TYPE_LONG    },
//  { "AF_BITVECTOR_3", MYSQL_TYPE_LONG    },
  { "Alin",           MYSQL_TYPE_LONG    },
    //  { "Alis",           MYSQL_TYPE_LONG, read_aliases_ascii(fl, ch, atoi(line) },
  { "Badp",           MYSQL_TYPE_LONG    },
  { "Bank",           MYSQL_TYPE_LONG    },
  { "Brth",           MYSQL_TYPE_LONG    },
  { "Cha",            MYSQL_TYPE_LONG    }, 
  { "Clas",           MYSQL_TYPE_LONG    },
  { "Con",            MYSQL_TYPE_LONG    },
  { "Descr",          MYSQL_TYPE_VAR_STRING   },  //Desc is a keyword
  { "Dex",            MYSQL_TYPE_LONG    },
  { "Drnk",           MYSQL_TYPE_LONG    },
  { "Drol",           MYSQL_TYPE_LONG    },
  { "Exp",            MYSQL_TYPE_LONG    },
  { "Frez",           MYSQL_TYPE_LONG    },
  { "Gold",           MYSQL_TYPE_LONG    },
  { "Hit",            MYSQL_TYPE_LONG    },
  { "Hite",           MYSQL_TYPE_LONG    },
  { "Host",           MYSQL_TYPE_VAR_STRING  },
  { "Hrol",           MYSQL_TYPE_LONG    },
  { "Hung",           MYSQL_TYPE_LONG    },
  { "ID",             MYSQL_TYPE_LONG    },
  { "Intel",          MYSQL_TYPE_LONG    },  //int is mysql keyword
  { "Invs",           MYSQL_TYPE_LONG    },
  { "Last",           MYSQL_TYPE_LONG    },
  { "Lern",           MYSQL_TYPE_LONG    },
  { "Levl",           MYSQL_TYPE_LONG    },
  { "Lmot",           MYSQL_TYPE_LONG    },
  { "Lnew",           MYSQL_TYPE_LONG    },
  { "Mana",           MYSQL_TYPE_LONG    },
  { "Move",           MYSQL_TYPE_LONG    },
  { "Name",           MYSQL_TYPE_VAR_STRING  },
  { "Olc",            MYSQL_TYPE_LONG    },
  { "Page",           MYSQL_TYPE_LONG    },
  { "Pass",           MYSQL_TYPE_VAR_STRING  },
  { "Plyd",           MYSQL_TYPE_LONG    },
  { "PfIn",           MYSQL_TYPE_VAR_STRING  },
  { "PfOt",           MYSQL_TYPE_VAR_STRING  },
  { "Pref_0",         MYSQL_TYPE_LONG    },
  { "Pref_1",         MYSQL_TYPE_LONG    },
  { "Pref_2",         MYSQL_TYPE_LONG    },
  { "Pref_3",         MYSQL_TYPE_LONG    },
  { "Qstp",           MYSQL_TYPE_LONG    },
  { "Qpnt",           MYSQL_TYPE_LONG    },
  { "Qcur",           MYSQL_TYPE_LONG    },
  { "Qcnt",           MYSQL_TYPE_LONG    },
 // { "Qest",           MYSQL_TYPE_LONG, load_quests(fl, ch)        },
  { "Room",           MYSQL_TYPE_LONG    },
  { "Sex",            MYSQL_TYPE_LONG    },
  { "ScrW",           MYSQL_TYPE_LONG    },
//      { "Skil",           MYSQL_TYPE_LONG, load_skills(fl, ch)            },
  { "Str",            MYSQL_TYPE_LONG     },
  { "Thir",           MYSQL_TYPE_LONG     },
  { "Thr1",           MYSQL_TYPE_LONG     },
  { "Thr2",           MYSQL_TYPE_LONG     },
  { "Thr3",           MYSQL_TYPE_LONG     },
  { "Thr4",           MYSQL_TYPE_LONG     },
  { "Thr5",           MYSQL_TYPE_LONG     },
  { "Titl",           MYSQL_TYPE_VAR_STRING   },
//      { "Trig",           MYSQL_TYPE_STRING, GET_TITLE(ch)                },
//      { "Vars",           MYSQL_TYPE_LONG           },
  { "Wate",           MYSQL_TYPE_LONG     },
  { "Wimp",           MYSQL_TYPE_LONG     },
  { "Wis",            MYSQL_TYPE_LONG     },      
  { "\n",              MYSQL_TYPE_LONG    }
};

void load_playerfile_from_mysql(struct mysql_bind_column *fields, int num_fields, struct char_data *ch)
{    
  int i;
    
  for(i = 0; i < num_fields; i++)
  {
    log("MYSQLINFO: field_name_meta: %s %d %s", fields[i].name, fields[i].col_int_buffer, fields[i].col_string_buffer == NULL ? "<NULL>": fields[i].col_string_buffer);
    if (!strcmp(fields[i].name, "Ac"))   GET_AC(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Act_0"))   PLR_FLAGS(ch)[0] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Act_1"))   PLR_FLAGS(ch)[1] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Act_2"))   PLR_FLAGS(ch)[2] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Act_3"))   PLR_FLAGS(ch)[3] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Aff_0"))   AFF_FLAGS(ch)[0] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Aff_1"))   AFF_FLAGS(ch)[1] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Aff_2"))   AFF_FLAGS(ch)[2] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Aff_3"))   AFF_FLAGS(ch)[3] = (fields[i].col_int_buffer);
//    else if (!strcmp(fields[i].name, "AF_BITVECTOR_0"))   af.bitvector[1] = (fields[i].col_int_buffer);
//    else if (!strcmp(fields[i].name, "AF_BITVECTOR_1"))   af.bitvector[2] = (fields[i].col_int_buffer);
//    else if (!strcmp(fields[i].name, "AF_BITVECTOR_2"))   af.bitvector[3] = (fields[i].col_int_buffer);
//    else if (!strcmp(fields[i].name, "AF_BITVECTOR_3"))   af.bitvector[4] = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Alin"))   GET_ALIGNMENT(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Badp"))   GET_BAD_PWS(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Bank"))   GET_BANK_GOLD(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Brth"))   ch->player.time.birth = (fields[i].col_int_buffer);  
    else if (!strcmp(fields[i].name, "Cha"))   ch->real_abils.cha = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Clas"))  GET_CLASS(ch)      = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Con"))   ch->real_abils.con = (fields[i].col_int_buffer);
  
//  { "Descr",          MYSQL_TYPE_VAR_STRING   },  //Desc is a keyword
    else if (!strcmp(fields[i].name, "Dex"))   ch->real_abils.dex = (fields[i].col_int_buffer);      
    else if (!strcmp(fields[i].name, "Drnk"))   GET_COND(ch, DRUNK) = (fields[i].col_int_buffer);      
    else if (!strcmp(fields[i].name, "Drol"))   GET_DAMROLL(ch) = (fields[i].col_int_buffer);      
    else if (!strcmp(fields[i].name, "Exp"))   GET_EXP(ch) = (fields[i].col_int_buffer);      
    else if (!strcmp(fields[i].name, "Frez"))   GET_FREEZE_LEV(ch) = (fields[i].col_int_buffer);      
    else if (!strcmp(fields[i].name, "Gold"))   GET_GOLD(ch) = (fields[i].col_int_buffer);      
    else if (!strcmp(fields[i].name, "Hit"))   GET_HIT(ch) = (fields[i].col_int_buffer);      
    else if (!strcmp(fields[i].name, "Hite"))   GET_HEIGHT(ch) = (fields[i].col_int_buffer);      
    else if (!strcmp(fields[i].name, "Host"))   GET_HOST(ch) = strdup(fields[i].col_string_buffer);

//  { "Hrol",           MYSQL_TYPE_LONG    },
//  { "Hung",           MYSQL_TYPE_LONG    },

    else if (!strcmp(fields[i].name, "ID"))   GET_IDNUM(ch) = (fields[i].col_int_buffer);
    else if (!strcmp(fields[i].name, "Intel"))   ch->real_abils.intel = (fields[i].col_int_buffer);
      
//  { "Invs",           MYSQL_TYPE_LONG    },
//  { "Last",           MYSQL_TYPE_LONG    },
//  { "Lern",           MYSQL_TYPE_LONG    },

    else if (!strcmp(fields[i].name, "Levl"))   GET_LEVEL(ch) = (fields[i].col_int_buffer);

//  { "Lmot",           MYSQL_TYPE_LONG    },
//  { "Lnew",           MYSQL_TYPE_LONG    },
//  { "Mana",           MYSQL_TYPE_LONG    },
//  { "Move",           MYSQL_TYPE_LONG    },
    else if (!strcmp(fields[i].name, "Name")) GET_PC_NAME(ch)	= strdup(fields[i].col_string_buffer);
//  { "Olc",            MYSQL_TYPE_LONG    },
//  { "Page",           MYSQL_TYPE_LONG    },

    else if (!strcmp(fields[i].name, "Pass")) strcpy(GET_PASSWD(ch), fields[i].col_string_buffer);
      
//  { "Plyd",           MYSQL_TYPE_LONG    },
//  { "PfIn",           MYSQL_TYPE_VAR_STRING  },
//  { "PfOt",           MYSQL_TYPE_VAR_STRING  },
//  { "Pref_0",         MYSQL_TYPE_LONG    },
//  { "Pref_1",         MYSQL_TYPE_LONG    },
//  { "Pref_2",         MYSQL_TYPE_LONG    },
//  { "Pref_3",         MYSQL_TYPE_LONG    },
//  { "Qstp",           MYSQL_TYPE_LONG    },
//  { "Qpnt",           MYSQL_TYPE_LONG    },
//  { "Qcur",           MYSQL_TYPE_LONG    },
//  { "Qcnt",           MYSQL_TYPE_LONG    },
//  { "Room",           MYSQL_TYPE_LONG    },

    else if (!strcmp(fields[i].name, "Sex"))   GET_SEX(ch) = (fields[i].col_int_buffer);

//  { "ScrW",           MYSQL_TYPE_LONG    },
      
    else if (!strcmp(fields[i].name, "Str"))   ch->real_abils.str = (fields[i].col_int_buffer);
      
//  { "Thir",           MYSQL_TYPE_LONG     },
//  { "Thr1",           MYSQL_TYPE_LONG     },
//  { "Thr2",           MYSQL_TYPE_LONG     },
//  { "Thr3",           MYSQL_TYPE_LONG     },
//  { "Thr4",           MYSQL_TYPE_LONG     },
//  { "Thr5",           MYSQL_TYPE_LONG     },
    else if(!strcmp(fields[i].name, "Titl")) GET_TITLE(ch) = strdup(fields[i].col_string_buffer == NULL ? "<NULL>": fields[i].col_string_buffer);
//  { "Wate",           MYSQL_TYPE_LONG     },
//  { "Wimp",           MYSQL_TYPE_LONG     },
      
    else if (!strcmp(fields[i].name, "Wis"))   ch->real_abils.wis = (fields[i].col_int_buffer);
      
  }
}

void update_playerfile_to_mysql_by_ID(MYSQL *conn, int ID, struct char_data *ch)
{
  int i, num_parameters;
  char buf[MAX_STRING_LENGTH];
  char parameter_buf[MAX_STRING_LENGTH];
  struct mysql_parameter_bind_adapter *parameters;
  struct mysql_column_bind_adapter *pft;

  pft = &playerfile_table;
    
  snprintf(buf, sizeof(buf)-1, "UPDATE strife_mud.playerfile SET ");
  i = 0;
  while(*playerfile_table[i].column_name != '\n')
  {
    strncat(buf, playerfile_table[i].column_name, sizeof(buf) - 1);
    if(*playerfile_table[i+1].column_name != '\n')
      strncat(buf, "= ?, ", sizeof(buf)-1);
    else
      strncat(buf, "= ? ", sizeof(buf)-1);
    i++;
  }
    
  strncat(buf, " WHERE ID = ?", sizeof(buf) - 1);
  log("MYSQLINFO: %s", buf);

  //include the ID
  num_parameters = (i+1);
  log("%d", num_parameters);
  //create a parameter list (which is just the character's name here)
  CREATE(parameters, struct mysql_parameter_bind_adapter, num_parameters);

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
    /* incorrect for af flags , fix later */
//    else if (!strcmp(playerfile_table[i].column_name, "AF_SPELL")) 
//      parameters[i].column_name = (int*)(af.spell);
//    else if (!strcmp(playerfile_table[i].column_name, "AF_DURATION")) 
//      parameters[i].column_name = (int*)(af.duration);
//    else if (!strcmp(playerfile_table[i].column_name, "AF_MODIFIER")) 
//      parameters[i].column_name = (int*)(af.modifier);
//    else if (!strcmp(playerfile_table[i].column_name, "AF_LOCATION")) 
//      parameters[i].column_name = (int*)(af.location);
//    else if (!strcmp(playerfile_table[i].column_name, "AF_BITVECTOR_0")) 
//      parameters[i].column_name = (int*)(af.bitvector[0]);
//    else if (!strcmp(playerfile_table[i].column_name, "AF_BITVECTOR_1")) 
//      parameters[i].column_name = (int*)(af.bitvector[1]);
//    else if (!strcmp(playerfile_table[i].column_name, "AF_BITVECTOR_2")) 
//      parameters[i].column_name = (int*)(af.bitvector[2]);
//    else if (!strcmp(playerfile_table[i].column_name, "AF_BITVECTOR_3")) 
//      parameters[i].column_name = (int*)(af.bitvector[3]);
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
    else if (!strcmp(playerfile_table[i].column_name, "Gold")) 
      parameters[i].int_data = (GET_GOLD(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Hit")) 
      parameters[i].int_data = (GET_HIT(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Hite")) 
      parameters[i].int_data = (GET_HEIGHT(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Host")) 
      parameters[i].string_data = GET_HOST(ch);   
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
      parameters[i].int_data = (ch->player.time.logon);      
    else if (!strcmp(playerfile_table[i].column_name, "Levl")) 
      parameters[i].int_data = (GET_LEVEL(ch)); 
    else if (!strcmp(playerfile_table[i].column_name, "Lmot")) 
      parameters[i].int_data = (GET_LAST_MOTD(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Lnew")) 
      parameters[i].int_data = (GET_LAST_NEWS(ch));
    else if (!strcmp(playerfile_table[i].column_name, "Mana")) 
      parameters[i].int_data = (GET_MANA(ch));      
    else if (!strcmp(playerfile_table[i].column_name, "Move")) 
      parameters[i].int_data = (GET_MOVE(ch));
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
      //if(POOFIN(ch)) parameters[i].string_data = strdup(POOFIN(ch)); else parameters[i].string_data = strdup("Poof in nothing");
        parameters[i].string_data = strdup("Poof in nothing");
    }
    else if (!strcmp(playerfile_table[i].column_name, "PfOt")) {
      //if(POOFOUT(ch)) parameters[i].string_data = strdup(POOFOUT(ch));
        parameters[i].string_data = strdup("Poof in nothing");
    }
    else if (!strcmp(playerfile_table[i].column_name, "Pref_0")) 
      parameters[i].int_data = (PRF_FLAGS(ch)[0]);  
    else if (!strcmp(playerfile_table[i].column_name, "Pref_1")) 
      parameters[i].int_data = (PRF_FLAGS(ch)[1]);  
    else if (!strcmp(playerfile_table[i].column_name, "Pref_2")) 
      parameters[i].int_data = (PRF_FLAGS(ch)[2]);  
    else if (!strcmp(playerfile_table[i].column_name, "Pref_3")) 
      parameters[i].int_data = (PRF_FLAGS(ch)[3]);
    else if (!strcmp(playerfile_table[i].column_name, "Qstp")) 
      parameters[i].int_data = (GET_QUESTPOINTS(ch));  
    else if (!strcmp(playerfile_table[i].column_name, "Qpnt")) 
      parameters[i].int_data = (GET_QUESTPOINTS(ch));  
    else if (!strcmp(playerfile_table[i].column_name, "Qcur")) 
      parameters[i].int_data = (GET_QUEST(ch));  
    else if (!strcmp(playerfile_table[i].column_name, "Qcnt")) 
      parameters[i].int_data = (GET_QUEST_COUNTER(ch));  
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
      log("Unknown parameter %s in update_playerfile_to_mysql_by_ID", playerfile_table[i].column_name);
    }
      
    if(parameters[i].data_type == MYSQL_TYPE_VAR_STRING && parameters[i].string_data)
        parameters[i].data_length = strlen(parameters[i].string_data);
    else 
        parameters[i].data_length = 0;
  }
  log("%d", num_parameters);
  log("MYSQLINFO: Updating Player");
  query_stmt_mysql(conn, parameters, NULL, buf, 0, num_parameters, NULL, ch, MYSQL_QUERY_UPDATE);
  
  for(i = 0; i < num_parameters; i++)
  {
    log("%s: parameters[%d].column_name - %d %s", playerfile_table[i].column_name,
        i, parameters[i].int_data, parameters[i].string_data == MYSQL_TYPE_VAR_STRING ? (char*)parameters[i].string_data : "not string");
    //if(parameters[i].string_data && parameters[i].data_type == MYSQL_TYPE_VAR_STRING)
    //  free(parameters[i].string_data);
  }
  free(parameters);
}

int select_player_from_mysql_by_name(MYSQL *conn, const char *name, struct char_data *ch)
{
  int i, num_parameters;
  char sql_buf[MAX_STRING_LENGTH];
  char buf[MAX_STRING_LENGTH];
  struct mysql_parameter_bind_adapter *parameters;
  struct mysql_column_bind_adapter *pft;

  pft = &playerfile_table;
    
  snprintf(buf, sizeof(buf)-1, "SELECT ");
  i = 0;
  while(*playerfile_table[i].column_name != '\n')
  {
    strncat(buf, playerfile_table[i].column_name, sizeof(buf) - 1);
    if(*playerfile_table[i+1].column_name != '\n')
      strncat(buf, ", ", sizeof(buf)-1);
    i++;
  }
  snprintf(sql_buf, sizeof(sql_buf) - 1, "%s FROM %s.%s WHERE Name = ?", buf,  MYSQL_DB, MYSQL_PLAYER_TABLE);
  log(sql_buf);
    
  num_parameters = 1;
  //create a parameter list (which is just the character's name here)
  CREATE(parameters, struct mysql_parameter_bind_adapter, num_parameters);
  parameters[0].string_data = strdup(name);
  parameters[0].data_type = MYSQL_TYPE_VAR_STRING;
  parameters[0].data_length = strlen(name);
    
  query_stmt_mysql(conn, parameters, pft, sql_buf, i, num_parameters, load_playerfile_from_mysql, ch, MYSQL_QUERY_SELECT);
    
  for(i = 0; i < num_parameters; i++)
    if(parameters[i].string_data)
      free(parameters[i].string_data);
  free(parameters);
  return 1;
}

int query_stmt_mysql(MYSQL *conn, struct mysql_parameter_bind_adapter *parameters, struct mysql_column_bind_adapter *columns, char *statement,
                     int num_columns, int num_parameters, void (*load_function)(struct mysql_bind_column *, int, struct char_data *), 
                     struct char_data *ch, int querytype)
{
  MYSQL_STMT *stmt;
  MYSQL_BIND param_bind[num_parameters], col_bind[num_columns]; /* variable sized array may cause a problem in certain compilers */
  int status, i;

  //struct that receives the column values from col_bind
  struct mysql_bind_column col_values[num_columns];

  /* initialize the prepared statement */
  stmt = mysql_stmt_init(conn);

  if (!stmt)
  {
    log("MYSQLINFO: Could not initialize statement\n");
    return -1;
  }
  log("MYSQLINFO: Initialized statement");

  /* prepare the statement */
  status = mysql_stmt_prepare(stmt, statement, strlen(statement));
  test_stmt_error(stmt, status);
  log("MYSQLINFO: prepared statement parameter count - %d", mysql_stmt_param_count(stmt));

  /* initialize the binding for the parameters (? in SQL statement) */
  memset(param_bind,0,sizeof(MYSQL_BIND)*num_parameters);

  for(i = 0; i < num_parameters; i++)
  {  
    if(parameters[i].data_type == MYSQL_TYPE_VAR_STRING)
    {
      param_bind[i].buffer_type = MYSQL_TYPE_VAR_STRING;
      param_bind[i].buffer=(char *) parameters[i].string_data;
      param_bind[i].buffer_length = 256;
      param_bind[i].is_null = 0;
      param_bind[i].length= &parameters[i].data_length;
    }
    else 
    {
      param_bind[i].buffer_type = MYSQL_TYPE_LONG;
      param_bind[i].buffer=(int *) &parameters[i].int_data;
      param_bind[i].is_null = 0;
    }
  }
    
  /* bind parameters */
  status = mysql_stmt_bind_param(stmt, param_bind);
  test_stmt_error(stmt, status);
    
  /* set the param_bind[i].length to the length of the column name (via the address of parameters[i].data_length)
     mysql wants this set after binding parameters
  */
  for(i = 0; i < num_parameters; i++)
  {
    if(parameters[i].data_type == MYSQL_TYPE_VAR_STRING)
    {
      parameters[i].data_length = strlen(parameters[i].string_data);
      log("Parameter string %ld %s", parameters[i].data_type, parameters[i].string_data); 
    }
    else
    {
      log("Parameter int %ld %d", parameters[i].data_type, parameters[i].int_data); 
    }
  }
  
  /* execute the statement now that it has its parameters defined */
  log("MYSQLINFO: stmt executing");
  status = mysql_stmt_execute(stmt); 
  test_stmt_error(stmt, status);

  log("MYSQLINFO: stmt executed");
  
  if(querytype == MYSQL_QUERY_SELECT)
  {
    /* prepare the col_bind to receive the values of the query */
    memset(col_bind,0,sizeof(MYSQL_BIND)*num_columns);
  
    /* point the col_bind to the col_values which is our own structure to store these values in
    */
    for(i = 0; i < num_columns; i++)
    {
      log("MYSQLINFO: Field - %s %s",columns[i].column_name, columns[i].data_type == MYSQL_TYPE_VAR_STRING ? "string": "not string");

      /* store our column_name in our col_value structure (since it is not available in the bind data) */
      strcpy(col_values[i].name, columns[i].column_name);
    
      /* right now just either mysql var string or longs  */
      if(columns[i].data_type == MYSQL_TYPE_VAR_STRING)
      {
        col_bind[i].buffer_type=MYSQL_TYPE_STRING;
        col_bind[i].buffer=(char *) (col_values[i].col_string_buffer);
        col_bind[i].buffer_length=150;
        col_bind[i].length = &col_values[i].buffer_length;
      }
      else
      {
        col_bind[i].buffer_type=MYSQL_TYPE_LONG;
        col_bind[i].buffer=(int *)(&col_values[i].col_int_buffer);
      }
      col_bind[i].is_null=(&col_values[i].is_null);
    }
    
    log("MYSQLINFO: binding resultset columns");
    /* now actually bind the result values of the query to col_bind which also fills col_values */
    if (mysql_stmt_bind_result(stmt, col_bind))
    {
      log("MYSQLINFO:  mysql_stmt_bind_result() failed");
      log(" %s\n", mysql_stmt_error(stmt));
      return -1;
    }
    
    /* Buffer all results to client */
    if (mysql_stmt_store_result(stmt))  
    {
      fprintf(stderr, " mysql_stmt_store_result() failed\n");
      fprintf(stderr, " %s\n", mysql_stmt_error(stmt));
      return (-1);
    }

    log("Rows: %d", mysql_stmt_num_rows(stmt));

    while (!mysql_stmt_fetch(stmt))
      load_function(col_values, num_columns, ch);
  }
  /* Close the statement */
  if (mysql_stmt_close(stmt))
  {
    /* mysql_stmt_close() invalidates stmt, so call          */
    /* mysql_error(mysql) rather than mysql_stmt_error(stmt) */
    test_error(conn, status);
  }
  log("MYSQLINFO: stmt closed");
  return 1;
}

static int test_error(MYSQL *mysql, int status)
{
  if (status)
  {
    log("MYSQLINFO: Error: %s (errno: %d)\n",
            mysql_error(mysql), mysql_errno(mysql));
    return -1;
  }
  return 1;
}

static int test_stmt_error(MYSQL_STMT *stmt, int status)
{
  if (status)
  {
    log("MYSQLINFO: Error: %s (errno: %d)\n",
            mysql_stmt_error(stmt), mysql_stmt_errno(stmt));
    return -1;
  }
  return 1;
}

/* Stuff related to the save/load player system. */
/* New load_char reads ASCII Player Files. Load a char, TRUE if loaded, FALSE
 * if not. */
int load_char(const char *name, struct char_data *ch)
{
  int id, i;
  FILE *fl;
  char filename[40];
  char buf[128], buf2[128], line[MAX_INPUT_LENGTH + 1], tag[6];
  char f1[128], f2[128], f3[128], f4[128];
  trig_data *t = NULL;
  trig_rnum t_rnum = NOTHING;

  if ((id = get_ptable_by_name(name)) < 0)
    return (-1);
  else {
    if (!get_filename(filename, sizeof(filename), PLR_FILE, player_table[id].name))
      return (-1);
    if (!(fl = fopen(filename, "r"))) {
      mudlog(NRM, LVL_GOD, TRUE, "SYSERR: Couldn't open player file %s", filename);
      return (-1);
    }

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
    ch->real_abils.str_add = PFDEF_STRADD;
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

#ifdef USING_MYSQL_DATABASE_FOR_PLAYERFILE

    /* begin test mysql load code */
    MYSQL *conn = NULL;

    if((conn = create_conn_to_mud_database(conn)) == NULL)
    {
      log("MYSQLINFO: Error connecting to database");
      return -1;
    }
  
    /* checking the database for the Name (this should be ID later, but currently the player 
       id comes from the playerfile instead of the id in the index for some reason)
    */
    select_player_from_mysql_by_name(conn, name, ch);
    mysql_close(conn);

  /* end of test mysql load code */
      
#else

    while (get_line(fl, line)) {
      tag_argument(line, tag);

      switch (*tag) {
      case 'A':
        if (!strcmp(tag, "Ac  "))	GET_AC(ch)		= atoi(line);
	else if (!strcmp(tag, "Act ")) {
         if (sscanf(line, "%s %s %s %s", f1, f2, f3, f4) == 4) {
          PLR_FLAGS(ch)[0] = asciiflag_conv(f1);
          PLR_FLAGS(ch)[1] = asciiflag_conv(f2);
          PLR_FLAGS(ch)[2] = asciiflag_conv(f3);
          PLR_FLAGS(ch)[3] = asciiflag_conv(f4);
        } else
          PLR_FLAGS(ch)[0] = asciiflag_conv(line);
      } else if (!strcmp(tag, "Aff ")) {
        if (sscanf(line, "%s %s %s %s", f1, f2, f3, f4) == 4) {
          AFF_FLAGS(ch)[0] = asciiflag_conv(f1);
          AFF_FLAGS(ch)[1] = asciiflag_conv(f2);
          AFF_FLAGS(ch)[2] = asciiflag_conv(f3);
          AFF_FLAGS(ch)[3] = asciiflag_conv(f4);
        } else
          AFF_FLAGS(ch)[0] = asciiflag_conv(line);
	}
	if (!strcmp(tag, "Affs")) 	load_affects(fl, ch);
        else if (!strcmp(tag, "Alin"))	GET_ALIGNMENT(ch)	= atoi(line);
	else if (!strcmp(tag, "Alis"))	read_aliases_ascii(fl, ch, atoi(line));
	break;

      case 'B':
	     if (!strcmp(tag, "Badp"))	GET_BAD_PWS(ch)		= atoi(line);
	else if (!strcmp(tag, "Bank"))	GET_BANK_GOLD(ch)	= atoi(line);
	else if (!strcmp(tag, "Brth"))	ch->player.time.birth	= atol(line);
	break;

      case 'C':
	     if (!strcmp(tag, "Cha "))	ch->real_abils.cha	= atoi(line);
	else if (!strcmp(tag, "Clas"))	GET_CLASS(ch)		= atoi(line);
	else if (!strcmp(tag, "Con "))	ch->real_abils.con	= atoi(line);
	break;

      case 'D':
	     if (!strcmp(tag, "Desc"))	ch->player.description	= fread_string(fl, buf2);
	else if (!strcmp(tag, "Dex "))	ch->real_abils.dex	= atoi(line);
	else if (!strcmp(tag, "Drnk"))	GET_COND(ch, DRUNK)	= atoi(line);
	else if (!strcmp(tag, "Drol"))	GET_DAMROLL(ch)		= atoi(line);
	break;

      case 'E':
	     if (!strcmp(tag, "Exp "))	GET_EXP(ch)		= atoi(line);
	break;

      case 'F':
	     if (!strcmp(tag, "Frez"))	GET_FREEZE_LEV(ch)	= atoi(line);
	break;

      case 'G':
	     if (!strcmp(tag, "Gold"))	GET_GOLD(ch)		= atoi(line);
	break;

      case 'H':
	     if (!strcmp(tag, "Hit "))	load_HMVS(ch, line, LOAD_HIT);
	else if (!strcmp(tag, "Hite"))	GET_HEIGHT(ch)		= atoi(line);
        else if (!strcmp(tag, "Host")) {
          if (GET_HOST(ch))
            free(GET_HOST(ch));
          GET_HOST(ch) = strdup(line);
        }
        else if (!strcmp(tag, "Hrol"))	GET_HITROLL(ch)		= atoi(line);
	else if (!strcmp(tag, "Hung"))	GET_COND(ch, HUNGER)	= atoi(line);
	break;

      case 'I':
	     if (!strcmp(tag, "Id  "))	GET_IDNUM(ch)		= atol(line);
	else if (!strcmp(tag, "Int "))	ch->real_abils.intel	= atoi(line);
	else if (!strcmp(tag, "Invs"))	GET_INVIS_LEV(ch)	= atoi(line);
	break;

      case 'L':
	     if (!strcmp(tag, "Last"))	ch->player.time.logon	= atol(line);
  else if (!strcmp(tag, "Lern"))	GET_PRACTICES(ch)	= atoi(line);
	else if (!strcmp(tag, "Levl"))	GET_LEVEL(ch)		= atoi(line);
        else if (!strcmp(tag, "Lmot"))   GET_LAST_MOTD(ch)   = atoi(line);
        else if (!strcmp(tag, "Lnew"))   GET_LAST_NEWS(ch)   = atoi(line);
	break;

      case 'M':
	     if (!strcmp(tag, "Mana"))	load_HMVS(ch, line, LOAD_MANA);
	else if (!strcmp(tag, "Move"))	load_HMVS(ch, line, LOAD_MOVE);
	break;

      case 'N':
	     if (!strcmp(tag, "Name"))	GET_PC_NAME(ch)	= strdup(line);
	break;

      case 'O':
       if (!strcmp(tag, "Olc "))  GET_OLC_ZONE(ch) = atoi(line);
  break;

      case 'P':
       if (!strcmp(tag, "Page"))  GET_PAGE_LENGTH(ch) = atoi(line);
	else if (!strcmp(tag, "Pass"))	strcpy(GET_PASSWD(ch), line);
	else if (!strcmp(tag, "Plyd"))	ch->player.time.played	= atoi(line);
	else if (!strcmp(tag, "PfIn"))	POOFIN(ch)		= strdup(line);
	else if (!strcmp(tag, "PfOt"))	POOFOUT(ch)		= strdup(line);
        else if (!strcmp(tag, "Pref")) {
          if (sscanf(line, "%s %s %s %s", f1, f2, f3, f4) == 4) {
            PRF_FLAGS(ch)[0] = asciiflag_conv(f1);
            PRF_FLAGS(ch)[1] = asciiflag_conv(f2);
            PRF_FLAGS(ch)[2] = asciiflag_conv(f3);
            PRF_FLAGS(ch)[3] = asciiflag_conv(f4);
          } else
	    PRF_FLAGS(ch)[0] = asciiflag_conv(f1);
	  }
        break;

      case 'Q':
	     if (!strcmp(tag, "Qstp"))  GET_QUESTPOINTS(ch)     = atoi(line);
       else if (!strcmp(tag, "Qpnt")) GET_QUESTPOINTS(ch) = atoi(line); /* Backward compatibility */
       else if (!strcmp(tag, "Qcur")) GET_QUEST(ch) = atoi(line);
       else if (!strcmp(tag, "Qcnt")) GET_QUEST_COUNTER(ch) = atoi(line);
       else if (!strcmp(tag, "Qest")) load_quests(fl, ch);
        break;

      case 'R':
	     if (!strcmp(tag, "Room"))	GET_LOADROOM(ch)	= atoi(line);
	break;

      case 'S':
	     if (!strcmp(tag, "Sex "))	GET_SEX(ch)		= atoi(line);
    else if (!strcmp(tag, "ScrW"))  GET_SCREEN_WIDTH(ch) = atoi(line);
	else if (!strcmp(tag, "Skil"))	load_skills(fl, ch);
	else if (!strcmp(tag, "Str "))	load_HMVS(ch, line, LOAD_STRENGTH);
	break;

      case 'T':
	     if (!strcmp(tag, "Thir"))	GET_COND(ch, THIRST)	= atoi(line);
	else if (!strcmp(tag, "Thr1"))	GET_SAVE(ch, 0)		= atoi(line);
	else if (!strcmp(tag, "Thr2"))	GET_SAVE(ch, 1)		= atoi(line);
	else if (!strcmp(tag, "Thr3"))	GET_SAVE(ch, 2)		= atoi(line);
	else if (!strcmp(tag, "Thr4"))	GET_SAVE(ch, 3)		= atoi(line);
	else if (!strcmp(tag, "Thr5"))	GET_SAVE(ch, 4)		= atoi(line);
	else if (!strcmp(tag, "Titl"))	GET_TITLE(ch)		= strdup(line);
        else if (!strcmp(tag, "Trig") && CONFIG_SCRIPT_PLAYERS) {
          if ((t_rnum = real_trigger(atoi(line))) != NOTHING) {
            t = read_trigger(t_rnum);
          if (!SCRIPT(ch))
            CREATE(SCRIPT(ch), struct script_data, 1);
          add_trigger(SCRIPT(ch), t, -1);
          }
         }
	break;

      case 'V':
	     if (!strcmp(tag, "Vars"))	read_saved_vars_ascii(fl, ch, atoi(line));
      break;

      case 'W':
	     if (!strcmp(tag, "Wate"))	GET_WEIGHT(ch)		= atoi(line);
	else if (!strcmp(tag, "Wimp"))	GET_WIMP_LEV(ch)	= atoi(line);
	else if (!strcmp(tag, "Wis "))	ch->real_abils.wis	= atoi(line);
	break;

      default:
	sprintf(buf, "SYSERR: Unknown tag %s in pfile %s", tag, name);
      }
    }
#endif
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
  fclose(fl);
  return(id);
}

/* should close the mysql connection and log an error */
void close_mysqlcon_with_error(MYSQL *conn)
{
  char buf[MAX_STRING_LENGTH];

  sprintf(buf, "%s", mysql_error(conn));
  log("MYSQLINFO: %s", buf);
  mysql_close(conn);
}

MYSQL *create_conn_to_mud_database(MYSQL *conn)
{
  char buf[MAX_STRING_LENGTH];

  conn = mysql_init(NULL);

  if (conn == NULL)
  {
      sprintf(buf, "mysql_init() failed\n");
      log("MYSQLINFO: %s", buf);
      return NULL;
  }

  if (mysql_real_connect(conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASS,
          MYSQL_DB, 0, NULL, 0) == NULL)
  {
      close_mysqlcon_with_error(conn);
      return NULL;
  }

  return conn;
}

void save_char_mysql(struct char_data * ch)
{
  MYSQL_RES *result;
  MYSQL_ROW row;
  char buf[MAX_STRING_LENGTH];
  char column_buf[MAX_STRING_LENGTH];
  MYSQL *conn = NULL;

  if((conn = create_conn_to_mud_database(conn)) == NULL)
  {
    log("MYSQLINFO: Error connecting to database");
    return;
  }

  /* checking the database for the character ... has to be name for now */
  sprintf(buf, "SELECT ID FROM %s.%s WHERE Name = '%s'", MYSQL_DB, MYSQL_PLAYER_TABLE, GET_NAME(ch));
  if (mysql_query(conn, buf))
  {
    close_mysqlcon_with_error(conn);
    log("MYSQLINFO: %s", buf);
    return;
  }

  result = mysql_store_result(conn);
  
  if (result == NULL) 
  {
    close_mysqlcon_with_error(conn);
    log("MYSQLINFO: %s", buf);
    return;
  }

  row = mysql_fetch_row(result);
  
  snprintf(buf, sizeof(buf), "MYSQLINFO: %ld mysql_numrows", (long)mysql_num_rows(result));
  log("MYSQLINFO: %s", buf);

  /* if not found in the database then insert a new record with name and ID */
  if(mysql_num_rows(result) < 1)
  {
    //INSERT NEW Player File
    log("MYSQLINFO: Inserting new player");
    snprintf(buf, sizeof(buf), "INSERT INTO %s.%s (ID, Name) VALUES(%ld, '%s')", MYSQL_DB, MYSQL_PLAYER_TABLE, GET_IDNUM(ch), GET_NAME(ch));
    mysql_query(database_conn, buf);
    log("MYSQLINFO: %s", buf);
  }

  //UPDATE player file
  log("MYSQLINFO: Updating Player");

  update_playerfile_to_mysql_by_ID(conn, GET_IDNUM(ch), ch);

}

/* Write the vital data of a player to the player file. */
/* This is the ASCII Player Files save routine. */
void save_char(struct char_data * ch)
{
  FILE *fl;
  char filename[40], buf[MAX_STRING_LENGTH], bits[127], bits2[127], bits3[127], bits4[127];
  int i, j, id, save_index = FALSE;
  struct affected_type *aff, tmp_aff[MAX_AFFECT];
  struct obj_data *char_eq[NUM_WEARS];
  trig_data *t;

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

  if (!get_filename(filename, sizeof(filename), PLR_FILE, GET_NAME(ch)))
    return;
  if (!(fl = fopen(filename, "w"))) {
    mudlog(NRM, LVL_GOD, TRUE, "SYSERR: Couldn't open player file %s for write", filename);
    return;
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

  for (aff = ch->affected, i = 0; i < MAX_AFFECT; i++) {
    if (aff) {
      tmp_aff[i] = *aff;
      for (j=0; j<AF_ARRAY_MAX; j++)
        tmp_aff[i].bitvector[j] = aff->bitvector[j];
      tmp_aff[i].next = 0;
      aff = aff->next;
    } else {
      new_affect(&(tmp_aff[i]));
      tmp_aff[i].next = 0;
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

  save_char_mysql(ch);

  if (GET_NAME(ch))				fprintf(fl, "Name: %s\n", GET_NAME(ch));
  if (GET_PASSWD(ch))				fprintf(fl, "Pass: %s\n", GET_PASSWD(ch));
  if (GET_TITLE(ch))				fprintf(fl, "Titl: %s\n", GET_TITLE(ch));
  if (ch->player.description && *ch->player.description) {
    strcpy(buf, ch->player.description);
    strip_cr(buf);
    fprintf(fl, "Desc:\n%s~\n", buf);
  }
  if (POOFIN(ch))				fprintf(fl, "PfIn: %s\n", POOFIN(ch));
  if (POOFOUT(ch))				fprintf(fl, "PfOt: %s\n", POOFOUT(ch));
  if (GET_SEX(ch)	     != PFDEF_SEX)	fprintf(fl, "Sex : %d\n", GET_SEX(ch));
  if (GET_CLASS(ch)	   != PFDEF_CLASS)	fprintf(fl, "Clas: %d\n", GET_CLASS(ch));
  if (GET_LEVEL(ch)	   != PFDEF_LEVEL)	fprintf(fl, "Levl: %d\n", GET_LEVEL(ch));

  fprintf(fl, "Id  : %ld\n", GET_IDNUM(ch));
  fprintf(fl, "Brth: %ld\n", (long)ch->player.time.birth);
  fprintf(fl, "Plyd: %d\n",  ch->player.time.played);
  fprintf(fl, "Last: %ld\n", (long)ch->player.time.logon);

  if (GET_LAST_MOTD(ch) != PFDEF_LASTMOTD)
    fprintf(fl, "Lmot: %d\n", (int)GET_LAST_MOTD(ch));
  if (GET_LAST_NEWS(ch) != PFDEF_LASTNEWS)
    fprintf(fl, "Lnew: %d\n", (int)GET_LAST_NEWS(ch));

  if (GET_HOST(ch))				fprintf(fl, "Host: %s\n", GET_HOST(ch));
  if (GET_HEIGHT(ch)	   != PFDEF_HEIGHT)	fprintf(fl, "Hite: %d\n", GET_HEIGHT(ch));
  if (GET_WEIGHT(ch)	   != PFDEF_WEIGHT)	fprintf(fl, "Wate: %d\n", GET_WEIGHT(ch));
  if (GET_ALIGNMENT(ch)  != PFDEF_ALIGNMENT)	fprintf(fl, "Alin: %d\n", GET_ALIGNMENT(ch));


  sprintascii(bits,  PLR_FLAGS(ch)[0]);
  sprintascii(bits2, PLR_FLAGS(ch)[1]);
  sprintascii(bits3, PLR_FLAGS(ch)[2]);
  sprintascii(bits4, PLR_FLAGS(ch)[3]);
  fprintf(fl, "Act : %s %s %s %s\n", bits, bits2, bits3, bits4);

  sprintascii(bits,  AFF_FLAGS(ch)[0]);
  sprintascii(bits2, AFF_FLAGS(ch)[1]);
  sprintascii(bits3, AFF_FLAGS(ch)[2]);
  sprintascii(bits4, AFF_FLAGS(ch)[3]);
  fprintf(fl, "Aff : %s %s %s %s\n", bits, bits2, bits3, bits4);

  sprintascii(bits,  PRF_FLAGS(ch)[0]);
  sprintascii(bits2, PRF_FLAGS(ch)[1]);
  sprintascii(bits3, PRF_FLAGS(ch)[2]);
  sprintascii(bits4, PRF_FLAGS(ch)[3]);
  fprintf(fl, "Pref: %s %s %s %s\n", bits, bits2, bits3, bits4);

 if (GET_SAVE(ch, 0)	   != PFDEF_SAVETHROW)	fprintf(fl, "Thr1: %d\n", GET_SAVE(ch, 0));
  if (GET_SAVE(ch, 1)	   != PFDEF_SAVETHROW)	fprintf(fl, "Thr2: %d\n", GET_SAVE(ch, 1));
  if (GET_SAVE(ch, 2)	   != PFDEF_SAVETHROW)	fprintf(fl, "Thr3: %d\n", GET_SAVE(ch, 2));
  if (GET_SAVE(ch, 3)	   != PFDEF_SAVETHROW)	fprintf(fl, "Thr4: %d\n", GET_SAVE(ch, 3));
  if (GET_SAVE(ch, 4)	   != PFDEF_SAVETHROW)	fprintf(fl, "Thr5: %d\n", GET_SAVE(ch, 4));

  if (GET_WIMP_LEV(ch)	   != PFDEF_WIMPLEV)	fprintf(fl, "Wimp: %d\n", GET_WIMP_LEV(ch));
  if (GET_FREEZE_LEV(ch)   != PFDEF_FREEZELEV)	fprintf(fl, "Frez: %d\n", GET_FREEZE_LEV(ch));
  if (GET_INVIS_LEV(ch)	   != PFDEF_INVISLEV)	fprintf(fl, "Invs: %d\n", GET_INVIS_LEV(ch));
  if (GET_LOADROOM(ch)	   != PFDEF_LOADROOM)	fprintf(fl, "Room: %d\n", GET_LOADROOM(ch));

  if (GET_BAD_PWS(ch)	   != PFDEF_BADPWS)	fprintf(fl, "Badp: %d\n", GET_BAD_PWS(ch));
  if (GET_PRACTICES(ch)	   != PFDEF_PRACTICES)	fprintf(fl, "Lern: %d\n", GET_PRACTICES(ch));

  if (GET_COND(ch, HUNGER)   != PFDEF_HUNGER && GET_LEVEL(ch) < LVL_IMMORT) fprintf(fl, "Hung: %d\n", GET_COND(ch, HUNGER));
  if (GET_COND(ch, THIRST) != PFDEF_THIRST && GET_LEVEL(ch) < LVL_IMMORT) fprintf(fl, "Thir: %d\n", GET_COND(ch, THIRST));
  if (GET_COND(ch, DRUNK)  != PFDEF_DRUNK  && GET_LEVEL(ch) < LVL_IMMORT) fprintf(fl, "Drnk: %d\n", GET_COND(ch, DRUNK));

  if (GET_HIT(ch)	   != PFDEF_HIT  || GET_MAX_HIT(ch)  != PFDEF_MAXHIT)  fprintf(fl, "Hit : %d/%d\n", GET_HIT(ch),  GET_MAX_HIT(ch));
  if (GET_MANA(ch)	   != PFDEF_MANA || GET_MAX_MANA(ch) != PFDEF_MAXMANA) fprintf(fl, "Mana: %d/%d\n", GET_MANA(ch), GET_MAX_MANA(ch));
  if (GET_MOVE(ch)	   != PFDEF_MOVE || GET_MAX_MOVE(ch) != PFDEF_MAXMOVE) fprintf(fl, "Move: %d/%d\n", GET_MOVE(ch), GET_MAX_MOVE(ch));

  if (GET_STR(ch)	   != PFDEF_STR  || GET_ADD(ch)      != PFDEF_STRADD)  fprintf(fl, "Str : %d/%d\n", GET_STR(ch),  GET_ADD(ch));


  if (GET_INT(ch)	   != PFDEF_INT)	fprintf(fl, "Int : %d\n", GET_INT(ch));
  if (GET_WIS(ch)	   != PFDEF_WIS)	fprintf(fl, "Wis : %d\n", GET_WIS(ch));
  if (GET_DEX(ch)	   != PFDEF_DEX)	fprintf(fl, "Dex : %d\n", GET_DEX(ch));
  if (GET_CON(ch)	   != PFDEF_CON)	fprintf(fl, "Con : %d\n", GET_CON(ch));
  if (GET_CHA(ch)	   != PFDEF_CHA)	fprintf(fl, "Cha : %d\n", GET_CHA(ch));

  if (GET_AC(ch)	   != PFDEF_AC)		fprintf(fl, "Ac  : %d\n", GET_AC(ch));
  if (GET_GOLD(ch)	   != PFDEF_GOLD)	fprintf(fl, "Gold: %d\n", GET_GOLD(ch));
  if (GET_BANK_GOLD(ch)	   != PFDEF_BANK)	fprintf(fl, "Bank: %d\n", GET_BANK_GOLD(ch));
  if (GET_EXP(ch)	   != PFDEF_EXP)	fprintf(fl, "Exp : %d\n", GET_EXP(ch));
  if (GET_HITROLL(ch)	   != PFDEF_HITROLL)	fprintf(fl, "Hrol: %d\n", GET_HITROLL(ch));
  if (GET_DAMROLL(ch)	   != PFDEF_DAMROLL)	fprintf(fl, "Drol: %d\n", GET_DAMROLL(ch));
  if (GET_OLC_ZONE(ch)     != PFDEF_OLC)        fprintf(fl, "Olc : %d\n", GET_OLC_ZONE(ch));
  if (GET_PAGE_LENGTH(ch)  != PFDEF_PAGELENGTH) fprintf(fl, "Page: %d\n", GET_PAGE_LENGTH(ch));
  if (GET_SCREEN_WIDTH(ch) != PFDEF_SCREENWIDTH) fprintf(fl, "ScrW: %d\n", GET_SCREEN_WIDTH(ch));
  if (GET_QUESTPOINTS(ch)  != PFDEF_QUESTPOINTS) fprintf(fl, "Qstp: %d\n", GET_QUESTPOINTS(ch));
  if (GET_QUEST_COUNTER(ch)!= PFDEF_QUESTCOUNT)  fprintf(fl, "Qcnt: %d\n", GET_QUEST_COUNTER(ch));
  if (GET_NUM_QUESTS(ch)   != PFDEF_COMPQUESTS) {
    fprintf(fl, "Qest:\n");
    for (i = 0; i < GET_NUM_QUESTS(ch); i++)
      fprintf(fl, "%d\n", ch->player_specials->saved.completed_quests[i]);
    fprintf(fl, "%d\n", NOTHING);
  }
  if (GET_QUEST(ch)        != PFDEF_CURRQUEST)  fprintf(fl, "Qcur: %d\n", GET_QUEST(ch));

 if (SCRIPT(ch)) {
   for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next)
   fprintf(fl, "Trig: %d\n",GET_TRIG_VNUM(t));
}

  /* Save skills */
  if (GET_LEVEL(ch) < LVL_IMMORT) {
    fprintf(fl, "Skil:\n");
    for (i = 1; i <= MAX_SKILLS; i++) {
     if (GET_SKILL(ch, i))
	fprintf(fl, "%d %d\n", i, GET_SKILL(ch, i));
    }
    fprintf(fl, "0 0\n");
  }

  /* Save affects */
  if (tmp_aff[0].spell > 0) {
    fprintf(fl, "Affs:\n");
    for (i = 0; i < MAX_AFFECT; i++) {
      aff = &tmp_aff[i];
      if (aff->spell)
		fprintf(fl, "%d %d %d %d %d %d %d %d\n", aff->spell, aff->duration,
          aff->modifier, aff->location, aff->bitvector[0], aff->bitvector[1], aff->bitvector[2], aff->bitvector[3]);
    }
    fprintf(fl, "0 0 0 0 0 0 0 0\n");
  }

  write_aliases_ascii(fl, ch);
  save_char_vars_ascii(fl, ch);

  fclose(fl);

  /* More char_to_store code to add spell and eq affections back in. */
  for (i = 0; i < MAX_AFFECT; i++) {
    if (tmp_aff[i].spell)
      affect_to_char(ch, &tmp_aff[i]);
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

  if ((id = get_ptable_by_name(GET_NAME(ch))) < 0)
    return;

  /* update the player in the player index */
  if (player_table[id].level != GET_LEVEL(ch)) {
    save_index = TRUE;
    player_table[id].level = GET_LEVEL(ch);
  }
  if (player_table[id].last != ch->player.time.logon) {
    save_index = TRUE;
    player_table[id].last = ch->player.time.logon;
  }
  i = player_table[id].flags;
  if (PLR_FLAGGED(ch, PLR_DELETED))
    SET_BIT(player_table[id].flags, PINDEX_DELETED);
  else
    REMOVE_BIT(player_table[id].flags, PINDEX_DELETED);
  if (PLR_FLAGGED(ch, PLR_NODELETE) || PLR_FLAGGED(ch, PLR_CRYO))
    SET_BIT(player_table[id].flags, PINDEX_NODELETE);
  else
    REMOVE_BIT(player_table[id].flags, PINDEX_NODELETE);

  if (PLR_FLAGGED(ch, PLR_FROZEN) || PLR_FLAGGED(ch, PLR_NOWIZLIST))
    SET_BIT(player_table[id].flags, PINDEX_NOWIZLIST);
  else
    REMOVE_BIT(player_table[id].flags, PINDEX_NOWIZLIST);

  if (player_table[id].flags != i || save_index)
    save_player_index();
}

/* Separate a 4-character id tag from the data it precedes */
void tag_argument(char *argument, char *tag)
{
  char *tmp = argument, *ttag = tag, *wrt = argument;
  int i;

  for (i = 0; i < 4; i++)
    *(ttag++) = *(tmp++);
  *ttag = '\0';

  while (*tmp == ':' || *tmp == ' ')
    tmp++;

  while (*tmp)
    *(wrt++) = *(tmp++);
  *wrt = '\0';
}

/* Stuff related to the player file cleanup system. */

/* remove_player() removes all files associated with a player who is self-deleted,
 * deleted by an immortal, or deleted by the auto-wipe system (if enabled). */
void remove_player(int pfilepos)
{
  char filename[MAX_STRING_LENGTH], timestr[25];
  int i;

  if (!*player_table[pfilepos].name)
    return;

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

  save_player_index();
}

void clean_pfiles(void)
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

/* load_affects function now handles both 32-bit and
   128-bit affect bitvectors for backward compatibility */
static void load_affects(FILE *fl, struct char_data *ch)
{
  int num = 0, num2 = 0, num3 = 0, num4 = 0, num5 = 0, num6 = 0, num7 = 0, num8 = 0, i, n_vars;
  char line[MAX_INPUT_LENGTH + 1];
  struct affected_type af;

  i = 0;
  do {
    new_affect(&af);
    get_line(fl, line);
    n_vars = sscanf(line, "%d %d %d %d %d %d %d %d", &num, &num2, &num3, &num4, &num5, &num6, &num7, &num8);
    if (num > 0) {
      af.spell = num;
      af.duration = num2;
      af.modifier = num3;
      af.location = num4;
      if (n_vars == 8) {              /* New 128-bit version */
          af.bitvector[0] =  num5;
          af.bitvector[1] =  num6;
          af.bitvector[2] =  num7;
          af.bitvector[3] =  num8;
      } else if (n_vars == 5) {       /* Old 32-bit conversion version */
        if (num5 > 0 && num5 <= NUM_AFF_FLAGS)  /* Ignore invalid values */
          SET_BIT_AR(af.bitvector, num5);
      } else {
        log("SYSERR: Invalid affects in pfile (%s), expecting 5 or 8 values", GET_NAME(ch));
      }
      affect_to_char(ch, &af);
      i++;
    }
  } while (num != 0);
}

static void load_skills(FILE *fl, struct char_data *ch)
{
  int num = 0, num2 = 0;
  char line[MAX_INPUT_LENGTH + 1];

  do {
    get_line(fl, line);
    sscanf(line, "%d %d", &num, &num2);
      if (num != 0)
	GET_SKILL(ch, num) = num2;
  } while (num != 0);
}

void load_quests(FILE *fl, struct char_data *ch)
{
  int num = NOTHING;
  char line[MAX_INPUT_LENGTH + 1];

  do {
    get_line(fl, line);
    sscanf(line, "%d", &num);
    if (num != NOTHING)
      add_completed_quest(ch, num);
  } while (num != NOTHING);
}

static void load_HMVS(struct char_data *ch, const char *line, int mode)
{
  int num = 0, num2 = 0;

  sscanf(line, "%d/%d", &num, &num2);

  switch (mode) {
  case LOAD_HIT:
    GET_HIT(ch) = num;
    GET_MAX_HIT(ch) = num2;
    break;

  case LOAD_MANA:
    GET_MANA(ch) = num;
    GET_MAX_MANA(ch) = num2;
    break;

  case LOAD_MOVE:
    GET_MOVE(ch) = num;
    GET_MAX_MOVE(ch) = num2;
    break;

  case LOAD_STRENGTH:
    ch->real_abils.str = num;
    ch->real_abils.str_add = num2;
    break;
  }
}

static void write_aliases_ascii(FILE *file, struct char_data *ch)
{
  struct alias_data *temp;
  int count = 0;

  if (GET_ALIASES(ch) == NULL)
    return;

  for (temp = GET_ALIASES(ch); temp; temp = temp->next)
    count++;

  fprintf(file, "Alis: %d\n", count);

  for (temp = GET_ALIASES(ch); temp; temp = temp->next)
    fprintf(file, " %s\n"   /* Alias: prepend a space in order to avoid issues with aliases beginning
                             * with * (get_line treats lines beginning with * as comments and ignores them */
                  "%s\n"    /* Replacement: always prepended with a space in memory anyway */
                  "%d\n",   /* Type */
                  temp->alias,
                  temp->replacement,
                  temp->type);
}

static void read_aliases_ascii(FILE *file, struct char_data *ch, int count)
{
  int i;

  if (count == 0) {
    GET_ALIASES(ch) = NULL;
    return; /* No aliases in the list. */
  }

  /* This code goes both ways for the old format (where alias and replacement start at the
   * first character on the line) and the new (where they are prepended by a space in order
   * to avoid the possibility of a * at the start of the line */
  for (i = 0; i < count; i++) {
    char abuf[MAX_INPUT_LENGTH+1], rbuf[MAX_INPUT_LENGTH+1], tbuf[MAX_INPUT_LENGTH];

    /* Read the aliased command. */
    get_line(file, abuf);

    /* Read the replacement. This needs to have a space prepended before placing in
     * the in-memory struct. The space may be there already, but we can't be certain! */
    rbuf[0] = ' ';
    get_line(file, rbuf+1);

    /* read the type */
    get_line(file, tbuf);

    if (abuf[0] && rbuf[1] && *tbuf) {
      struct alias_data *temp;
      CREATE(temp, struct alias_data, 1);
      temp->alias       = strdup(abuf[0] == ' ' ? abuf+1 : abuf);
      temp->replacement = strdup(rbuf[1] == ' ' ? rbuf+1 : rbuf);
      temp->type        = atoi(tbuf);
      temp->next        = GET_ALIASES(ch);
      GET_ALIASES(ch)   = temp;
    }
  }
}
