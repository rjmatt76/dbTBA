/**************************************************************************
*  File: genwld.c                                          Part of tbaMUD *
*  Usage: Generic OLC Library - Rooms.                                    *
*                                                                         *
*  By Levork. Copyright 1996 by Harvey Gilpin, 1997-2001 by George Greer. *
**************************************************************************/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "db.h"
#include "handler.h"
#include "comm.h"
#include "genolc.h"
#include "genwld.h"
#include "genzon.h"
#include "shop.h"
#include "dg_olc.h"
#include "mud_event.h"
#include "mysql_db.h"


void delete_rooms_mysql(MYSQL *conn, zone_rnum rzone);
int save_rooms_mysql(MYSQL *conn, zone_rnum rzone);
void delete_exits_mysql(MYSQL *conn, zone_rnum rzone);
int save_room_exits_mysql(MYSQL *conn, zone_rnum rzone);

/* This function will copy the strings so be sure you free your own copies of
 * the description, title, and such. */
room_rnum add_room(struct room_data *room)
{
  struct char_data *tch;
  struct obj_data *tobj;
  int j, found = FALSE;
  room_rnum i;

  if (room == NULL)
    return NOWHERE;

  if ((i = real_room(room->number)) != NOWHERE) {
    if (SCRIPT(&world[i]))
      extract_script(&world[i], WLD_TRIGGER);
    tch = world[i].people;
    tobj = world[i].contents;
    copy_room(&world[i], room);
    world[i].people = tch;
    world[i].contents = tobj;
    add_to_save_list(zone_table[room->zone].number, SL_WLD);
    log("GenOLC: add_room: Updated existing room #%d.", room->number);
    return i;
  }

  RECREATE(world, struct room_data, top_of_world + 2);
  top_of_world++;

  for (i = top_of_world; i > 0; i--) {
    if (room->number > world[i - 1].number) {
      world[i] = *room;
      copy_room_strings(&world[i], room);
      found = i;
      break;
    } else {
      /* Copy the room over now. */
      world[i] = world[i - 1];
      update_wait_events(&world[i], &world[i-1]);

      /* People in this room must have their in_rooms moved up one. */
      for (tch = world[i].people; tch; tch = tch->next_in_room)
	IN_ROOM(tch) += (IN_ROOM(tch) != NOWHERE);

      /* Move objects too. */
      for (tobj = world[i].contents; tobj; tobj = tobj->next_content)
	IN_ROOM(tobj) += (IN_ROOM(tobj) != NOWHERE);
    }
  }
  if (!found) {
    world[0] = *room;	/* Last place, in front. */
    copy_room_strings(&world[0], room);
  }

  log("GenOLC: add_room: Added room %d at index #%d.", room->number, found);
  /* found is equal to the array index where we added the room. */

  /* Find what zone that room was in so we can update the loading table. */
  for (i = room->zone; i <= top_of_zone_table; i++)
    for (j = 0; ZCMD(i, j).command != 'S'; j++)
      switch (ZCMD(i, j).command) {
      case 'M':
      case 'O':
      case 'T':
      case 'V':
	ZCMD(i, j).arg3 += (ZCMD(i, j).arg3 != NOWHERE && ZCMD(i, j).arg3 >= found);
	break;
      case 'D':
      case 'R':
	ZCMD(i, j).arg1 += (ZCMD(i, j).arg1 != NOWHERE && ZCMD(i, j).arg1 >= found);
      case 'G':
      case 'P':
      case 'E':
      case '*':
	/* Known zone entries we don't care about. */
        break;
      default:
        mudlog(BRF, LVL_GOD, TRUE, "SYSERR: GenOLC: add_room: Unknown zone entry found!");
      }

  /* Update the loadroom table. Adds 1 or 0. */
  r_mortal_start_room += (r_mortal_start_room >= found);
  r_immort_start_room += (r_immort_start_room >= found);
  r_frozen_start_room += (r_frozen_start_room >= found);

  /* Update world exits. */
  i = top_of_world + 1;
  do {
    i--;
    for (j = 0; j < DIR_COUNT; j++)
      if (W_EXIT(i, j) && W_EXIT(i, j)->to_room != NOWHERE)
	W_EXIT(i, j)->to_room += (W_EXIT(i, j)->to_room >= found);
  } while (i > 0);

  add_to_save_list(zone_table[room->zone].number, SL_WLD);

  /* Return what array entry we placed the new room in. */
  return found;
}

int delete_room(room_rnum rnum)
{
  room_rnum i;
  int j;
  struct char_data *ppl, *next_ppl;
  struct obj_data *obj, *next_obj;
  struct room_data *room;

  if (rnum <= 0 || rnum > top_of_world)	/* Can't delete void yet. */
    return FALSE;

  room = &world[rnum];

  add_to_save_list(zone_table[room->zone].number, SL_WLD);

  /* This is something you might want to read about in the logs. */
  log("GenOLC: delete_room: Deleting room #%d (%s).", room->number, room->name);

  if (r_mortal_start_room == rnum) {
    log("WARNING: GenOLC: delete_room: Deleting mortal start room!");
    r_mortal_start_room = 0;	/* The Void */
  }
  if (r_immort_start_room == rnum) {
    log("WARNING: GenOLC: delete_room: Deleting immortal start room!");
    r_immort_start_room = 0;	/* The Void */
  }
  if (r_frozen_start_room == rnum) {
    log("WARNING: GenOLC: delete_room: Deleting frozen start room!");
    r_frozen_start_room = 0;	/* The Void */
  }

  /* Dump the contents of this room into the Void.  We could also just extract 
   * the people, mobs, and objects here. */
  for (obj = world[rnum].contents; obj; obj = next_obj) {
    next_obj = obj->next_content;
    obj_from_room(obj);
    obj_to_room(obj, 0);
  }
  for (ppl = world[rnum].people; ppl; ppl = next_ppl) {
    next_ppl = ppl->next_in_room;
    char_from_room(ppl);
    char_to_room(ppl, 0);
  }

  free_room_strings(room);
  if (SCRIPT(room))
    extract_script(room, WLD_TRIGGER);
  free_proto_script(room, WLD_TRIGGER);

  if (room->events != NULL) {
	  if (room->events->iSize > 0) {
		struct event * pEvent;

		while ((pEvent = simple_list(room->events)) != NULL)
		  event_cancel(pEvent);
	  }
	  free_list(room->events);
    room->events = NULL;
  }

  /* Change any exit going to this room to go the void. Also fix all the exits 
   * pointing to rooms above this. */
  i = top_of_world + 1;
  do {
    i--;
    for (j = 0; j < DIR_COUNT; j++) {
      if (W_EXIT(i, j) == NULL)
        continue;
      else if (W_EXIT(i, j)->to_room > rnum)
        W_EXIT(i, j)->to_room -= (W_EXIT(i, j)->to_room != NOWHERE); /* with unsigned NOWHERE > any rnum */
      else if (W_EXIT(i, j)->to_room == rnum) {
      	if ((!W_EXIT(i, j)->keyword || !*W_EXIT(i, j)->keyword) &&
      	    (!W_EXIT(i, j)->general_description || !*W_EXIT(i, j)->general_description)) {
          /* no description, remove exit completely */
          if (W_EXIT(i, j)->keyword)
            free(W_EXIT(i, j)->keyword);
          if (W_EXIT(i, j)->general_description)
            free(W_EXIT(i, j)->general_description);
          free(W_EXIT(i, j));
          W_EXIT(i, j) = NULL;
        } else {
          /* description is set, just point to nowhere */
          W_EXIT(i, j)->to_room = NOWHERE;
        }
      }
    }
  } while (i > 0);

  /* Find what zone that room was in so we can update the loading table. */
  for (i = 0; i <= top_of_zone_table; i++)
    for (j = 0; ZCMD(i , j).command != 'S'; j++)
      switch (ZCMD(i, j).command) {
      case 'M':
      case 'O':
      case 'T':
      case 'V':
	if (ZCMD(i, j).arg3 == rnum)
	  ZCMD(i, j).command = '*';	/* Cancel command. */
	else if (ZCMD(i, j).arg3 > rnum)
	  ZCMD(i, j).arg3 -= (ZCMD(i, j).arg3 != NOWHERE); /* with unsigned NOWHERE > any rnum */
	break;
      case 'D':
      case 'R':
	if (ZCMD(i, j).arg1 == rnum)
	  ZCMD(i, j).command = '*';	/* Cancel command. */
	else if (ZCMD(i, j).arg1 > rnum)
	  ZCMD(i, j).arg1 -= (ZCMD(i, j).arg1 != NOWHERE); /* with unsigned NOWHERE > any rnum */
      case 'G':
      case 'P':
      case 'E':
      case '*':
        /* Known zone entries we don't care about. */
        break;
      default:
        mudlog(BRF, LVL_GOD, TRUE, "SYSERR: GenOLC: delete_room: Unknown zone entry found!");
      }

  /* Remove this room from all shop lists. */
  for (i = 0; i <= top_shop; i++) {
    for (j = 0;SHOP_ROOM(i, j) != NOWHERE;j++) {
      if (SHOP_ROOM(i, j) == world[rnum].number)
        SHOP_ROOM(i, j) = 0; /* set to the void */
    }
  }
  /* Now we actually move the rooms down. */
  for (i = rnum; i < top_of_world; i++) {
    world[i] = world[i + 1];
    update_wait_events(&world[i], &world[i+1]);

    for (ppl = world[i].people; ppl; ppl = ppl->next_in_room)
      IN_ROOM(ppl) -= (IN_ROOM(ppl) != NOWHERE);	/* Redundant check? */

    for (obj = world[i].contents; obj; obj = obj->next_content)
      IN_ROOM(obj) -= (IN_ROOM(obj) != NOWHERE);	/* Redundant check? */
  }

  top_of_world--;
  RECREATE(world, struct room_data, top_of_world + 1);

  return TRUE;
}

int save_rooms(zone_rnum rzone)
{
  int i, zrn;
  struct room_data *room;
  FILE *sf;
  char filename[128];
  char buf[MAX_STRING_LENGTH];
  char buf1[MAX_STRING_LENGTH];
  char buf2[MAX_STRING_LENGTH];

  /* if you wanted to save all zones at once
  for (zrn = 0; zrn <= top_of_zone_table; zrn++)
  {
    save_rooms_mysql(db_conn, zrn);
    save_room_exits_mysql(db_conn, zrn);
  }*/

  save_rooms_mysql(db_conn, rzone);
  save_room_exits_mysql(db_conn, rzone);

#if CIRCLE_UNSIGNED_INDEX
  if (rzone == NOWHERE || rzone > top_of_zone_table) {
#else
  if (rzone < 0 || rzone > top_of_zone_table) {
#endif
    log("SYSERR: GenOLC: save_rooms: Invalid zone number %d passed! (0-%d)", rzone, top_of_zone_table);
    return FALSE;
  }

  log("GenOLC: save_rooms: Saving rooms in zone #%d (%d-%d).",
	zone_table[rzone].number, genolc_zone_bottom(rzone), zone_table[rzone].top);

  snprintf(filename, sizeof(filename), "%s/%d.new", WLD_PREFIX, zone_table[rzone].number);
  if (!(sf = fopen(filename, "w"))) {
    perror("SYSERR: save_rooms");
    return FALSE;
  }

  for (i = genolc_zone_bottom(rzone); i <= zone_table[rzone].top; i++) {
    room_rnum rnum;

    if ((rnum = real_room(i)) != NOWHERE) {
      int j;

      room = (world + rnum);

      /* Copy the description and strip off trailing newlines. */
      strncpy(buf, room->description ? room->description : "Empty room.", sizeof(buf)-1 );
      strip_cr(buf);

      /* Save the numeric and string section of the file. */
      int n = snprintf(buf2, MAX_STRING_LENGTH, "#%d\n"
			"%s%c\n"
			"%s%c\n"
			"%d %d %d %d %d %d\n",
	room->number,
	room->name ? room->name : "Untitled", STRING_TERMINATOR,
	buf, STRING_TERMINATOR,
	zone_table[room->zone].number, room->room_flags[0], room->room_flags[1], room->room_flags[2], 
	  room->room_flags[3], room->sector_type 
      );
      
      if(n >= MAX_STRING_LENGTH) {
        mudlog(BRF,LVL_BUILDER,TRUE,
               "SYSERR: Could not save room #%d due to size (%d > maximum of %d).",
               room->number, n, MAX_STRING_LENGTH);
        continue;
      }

  fprintf(sf, "%s", convert_from_tabs(buf2));
 
      /* Now you write out the exits for the room. */
      for (j = 0; j < DIR_COUNT; j++) {
	if (R_EXIT(room, j)) {
	  int dflag;
	  if (R_EXIT(room, j)->general_description) {
	    strncpy(buf, R_EXIT(room, j)->general_description, sizeof(buf)-1);
	    strip_cr(buf);
	  } else
	    *buf = '\0';

	  /* Figure out door flag. */
	  if (IS_SET(R_EXIT(room, j)->exit_info, EX_ISDOOR)) {
	    if (IS_SET(R_EXIT(room, j)->exit_info, EX_PICKPROOF))
	      dflag = 2;
	    else
	      dflag = 1;
	      
	   if (IS_SET(R_EXIT(room, j)->exit_info, EX_HIDDEN))
          dflag += 2;   
          
	  } else
	    dflag = 0;

	  if (R_EXIT(room, j)->keyword)
	    strncpy(buf1, R_EXIT(room, j)->keyword, sizeof(buf1)-1 );
	  else
	    *buf1 = '\0';

	  /* Now write the exit to the file. */
	  fprintf(sf,	"D%d\n"
			"%s~\n"
			"%s~\n"
			"%d %d %d\n", j, buf, buf1, dflag,
		R_EXIT(room, j)->key != NOTHING ? R_EXIT(room, j)->key : -1,
		R_EXIT(room, j)->to_room != NOWHERE ? world[R_EXIT(room, j)->to_room].number : -1);

	}
      }

      if (room->ex_description) {
        struct extra_descr_data *xdesc;

	for (xdesc = room->ex_description; xdesc; xdesc = xdesc->next) {
	  strncpy(buf, xdesc->description, sizeof(buf) - 1);
	  buf[sizeof(buf) - 1] = '\0';
	  strip_cr(buf);
	  fprintf(sf,	"E\n"
			"%s~\n"
			"%s~\n", xdesc->keyword, buf);
	}
      }
      fprintf(sf, "S\n");
      script_save_to_disk(sf, room, WLD_TRIGGER);
    }
  }

  /* Write the final line and close it. */
  fprintf(sf, "$~\n");
  fclose(sf);

  /* Old file we're replacing. */
  snprintf(buf, sizeof(buf), "%s/%d.wld", WLD_PREFIX, zone_table[rzone].number);

  remove(buf);
  rename(filename, buf);

  if (in_save_list(zone_table[rzone].number, SL_WLD))
    remove_from_save_list(zone_table[rzone].number, SL_WLD);
  return TRUE;
}

int copy_room(struct room_data *to, struct room_data *from)
{
  free_room_strings(to);
  *to = *from;
  copy_room_strings(to, from);
  to->events = from->events;

  /* Don't put people and objects in two locations. Should this be done here? */
  from->people = NULL;
  from->contents = NULL;
  from->events = NULL;

  return TRUE;
}

/* Copy strings over so bad things don't happen.  We do not free the existing 
 * strings here because copy_room() did a shallow copy previously and we'd be 
 * freeing the very strings we're copying.  If this function is used elsewhere,
 * be sure to free_room_strings() the 'dest' room first. */
int copy_room_strings(struct room_data *dest, struct room_data *source)
{
  int i;

  if (dest == NULL || source == NULL) {
    log("SYSERR: GenOLC: copy_room_strings: NULL values passed.");
    return FALSE;
  }

  dest->description = str_udup(source->description);
  dest->name = str_udup(source->name);

  for (i = 0; i < DIR_COUNT; i++) {
    if (!R_EXIT(source, i))
      continue;

    CREATE(R_EXIT(dest, i), struct room_direction_data, 1);
    *R_EXIT(dest, i) = *R_EXIT(source, i);
    if (R_EXIT(source, i)->general_description)
      R_EXIT(dest, i)->general_description = strdup(R_EXIT(source, i)->general_description);
    if (R_EXIT(source, i)->keyword)
      R_EXIT(dest, i)->keyword = strdup(R_EXIT(source, i)->keyword);
  }

  if (source->ex_description)
    copy_ex_descriptions(&dest->ex_description, source->ex_description);

  return TRUE;
}

int free_room_strings(struct room_data *room)
{
  int i;

  /* Free descriptions. */
  if (room->name)
    free(room->name);
  if (room->description)
    free(room->description);
  if (room->ex_description)
    free_ex_descriptions(room->ex_description);

  /* Free exits. */
  for (i = 0; i < DIR_COUNT; i++) {
    if (room->dir_option[i]) {
      if (room->dir_option[i]->general_description)
        free(room->dir_option[i]->general_description);

      if (room->dir_option[i]->keyword)
        free(room->dir_option[i]->keyword);

      free(room->dir_option[i]);
      room->dir_option[i] = NULL;
    }
  }

  return TRUE;
}

const struct mysql_column room_table_index[] =
{
  { "Vnum",           MYSQL_TYPE_LONG           },
  { "Zone",           MYSQL_TYPE_LONG           },
  { "Name",           MYSQL_TYPE_VAR_STRING     },
  { "Description",    MYSQL_TYPE_VAR_STRING     },
  { "SectorType",     MYSQL_TYPE_LONG           },
  { "RoomFlags_0",    MYSQL_TYPE_LONG           },
  { "RoomFlags_1",    MYSQL_TYPE_LONG           },
  { "RoomFlags_2",    MYSQL_TYPE_LONG           },
  { "RoomFlags_3",    MYSQL_TYPE_LONG           },
  { "\n",             MYSQL_TYPE_LONG           }
};

const struct mysql_column room_dir_table_index[] =
{
  { "Vnum",           MYSQL_TYPE_LONG           },
  { "DirectionNum",   MYSQL_TYPE_LONG           },
  { "Keyword",        MYSQL_TYPE_VAR_STRING     },
  { "Description",    MYSQL_TYPE_VAR_STRING     },
  { "ExitInfo",       MYSQL_TYPE_LONG           },
  { "KeyNum",         MYSQL_TYPE_LONG           },
  { "ToRoom",         MYSQL_TYPE_LONG           },
  { "\n",             MYSQL_TYPE_LONG           }
};

void delete_rooms_mysql(MYSQL *conn, zone_rnum rzone)
{
  char sql_buf[MAX_STRING_LENGTH];
  struct mysql_parameter *parameters;
  int num_parameters = 1;

  snprintf(sql_buf, sizeof(sql_buf)-1, "DELETE FROM %s.%s WHERE Zone = ?", MYSQL_DB, MYSQL_ROOM_TABLE);

  log("MYSQLINFO: %s | parameters: %d", sql_buf, num_parameters);

  /* zone number */
  CREATE(parameters, struct mysql_parameter, num_parameters);
  parameters[0].data_length = 0;
  parameters[0].data_type = MYSQL_TYPE_LONG;
  parameters[0].int_data = rzone;

  query_stmt_mysql(conn, parameters, NULL, sql_buf, 0, num_parameters, NULL, NULL, MYSQL_QUERY_DELETE);

  free_mysql_parameters(parameters, num_parameters);
}

int save_rooms_mysql(MYSQL *conn, zone_rnum rzone)
{
  struct mysql_column *col = room_table_index;
  char sql_buf[MAX_STRING_LENGTH];
  char value_buf[MAX_STRING_LENGTH] = "\0";
  char buf[MAX_STRING_LENGTH] = "\0";
  struct mysql_parameter *parameters;
  int num_parameters = 0, num_columns = 0, col_num, num_rows = 0, i = 0;
  struct room_data *room;

  num_columns = get_column_sql(buf, sizeof(buf), room_table_index);

  for (i = genolc_zone_bottom(rzone); i <= zone_table[rzone].top; i++)
  {
    room_rnum r_num;

    if((r_num = real_room(i)) != NOWHERE)
    {
      num_rows++;
    }
  }

  num_parameters = get_parameter_markers_sql(value_buf, sizeof(value_buf), num_columns, num_rows);

  snprintf(sql_buf, sizeof(sql_buf)-1, "INSERT INTO %s.%s (%s) VALUES %s",
     MYSQL_DB, MYSQL_ROOM_TABLE, buf, value_buf);

  log("MYSQLINFO: %s | parameters: %d", sql_buf, num_parameters);

  CREATE(parameters, struct mysql_parameter, num_parameters);

  int parameter_index = 0;
  for (parameter_index=0,i = genolc_zone_bottom(rzone); i <= zone_table[rzone].top; i++) {
    room_rnum rnum;

    if ((rnum = real_room(i)) != NOWHERE) {

      room = (world + rnum);

      /* Copy the description and strip off trailing newlines. */
      strncpy(buf, room->description ? room->description : "Empty room.", sizeof(buf)-1 );
      strip_cr(buf);

      for(col_num = 0; col_num < num_columns; col_num++, parameter_index++)
      {
        parameters[parameter_index].data_type = col[col_num].data_type;

        if(!strcmp(col[col_num].column_name, "Vnum"))
          parameters[parameter_index].int_data = room->number;
        else if(!strcmp(col[col_num].column_name, "Zone"))
          parameters[parameter_index].int_data = zone_table[room->zone].number;
        else if(!strcmp(col[col_num].column_name, "Name"))
          parameters[parameter_index].string_data = strdup(room->name?room->name:"Untitled");
        else if(!strcmp(col[col_num].column_name, "Description"))
          parameters[parameter_index].string_data = strdup(buf);
        else if(!strcmp(col[col_num].column_name, "SectorType"))
          parameters[parameter_index].int_data = room->sector_type;
        else if(!strcmp(col[col_num].column_name, "RoomFlags_0"))
          parameters[parameter_index].int_data = room->room_flags[0];
        else if(!strcmp(col[col_num].column_name, "RoomFlags_1"))
          parameters[parameter_index].int_data = room->room_flags[1];
        else if(!strcmp(col[col_num].column_name, "RoomFlags_2"))
          parameters[parameter_index].int_data = room->room_flags[2];
        else if(!strcmp(col[col_num].column_name, "RoomFlags_3"))
          parameters[parameter_index].int_data = room->room_flags[3];

        if(parameters[parameter_index].data_type == MYSQL_TYPE_VAR_STRING && parameters[parameter_index].string_data)
          parameters[parameter_index].data_length = strlen(parameters[parameter_index].string_data);
        else
          parameters[parameter_index].data_length = 0;
      }
    }
  }
  delete_rooms_mysql(conn, zone_table[rzone].number);
  query_stmt_mysql(conn, parameters, NULL, sql_buf, 0, num_parameters, NULL, NULL, MYSQL_QUERY_INSERT);

  free_mysql_parameters(parameters, num_parameters);
}

void delete_exits_mysql(MYSQL *conn, zone_rnum rzone)
{
  char sql_buf[MAX_STRING_LENGTH];
  struct mysql_parameter *parameters;
  int num_parameters = 2;

  snprintf(sql_buf, sizeof(sql_buf)-1, "DELETE FROM %s.%s WHERE Vnum >= ? AND VNUM < ?", MYSQL_DB, MYSQL_ROOM_DIR_TABLE);

  log("MYSQLINFO: %s | parameters: %d", sql_buf, num_parameters);

  /* zone number */
  CREATE(parameters, struct mysql_parameter, num_parameters);
  parameters[0].data_length = 0;
  parameters[0].data_type = MYSQL_TYPE_LONG;
  parameters[0].int_data = rzone*100;

  //end of zone
  parameters[1].data_length = 0;
  parameters[1].data_type = MYSQL_TYPE_LONG;
  parameters[1].int_data = rzone*100+100;

  query_stmt_mysql(conn, parameters, NULL, sql_buf, 0, num_parameters, NULL, NULL, MYSQL_QUERY_DELETE);

  free_mysql_parameters(parameters, num_parameters);
}

int save_room_exits_mysql(MYSQL *conn, zone_rnum rzone)
{
  struct mysql_column *col = room_dir_table_index;
  char sql_buf[MAX_STRING_LENGTH];
  char value_buf[MAX_STRING_LENGTH] = "\0";
  char buf[MAX_STRING_LENGTH] = "\0";
  char buf1[MAX_STRING_LENGTH];
  struct mysql_parameter *parameters;
  int num_parameters = 0, num_columns = 0, num_rows = 0, col_num, i = 0, j = 0, dflag;
  struct room_data *room;

  num_columns = get_column_sql(buf, sizeof(buf), room_dir_table_index);

  for (i = genolc_zone_bottom(rzone); i <= zone_table[rzone].top; i++)
  {
    room_rnum r_num;

    if((r_num = real_room(i)) != NOWHERE)
    {
      room = (world + r_num);

      for(j = 0; j < DIR_COUNT; j++) {
        if (R_EXIT(room, j))
          num_rows++;
      }
    }
  }

  num_parameters = get_parameter_markers_sql(value_buf, sizeof(value_buf), num_columns, num_rows);

  snprintf(sql_buf, sizeof(sql_buf)-1, "INSERT INTO %s.%s (%s) VALUES %s",
     MYSQL_DB, MYSQL_ROOM_DIR_TABLE, buf, value_buf);

//  log("MYSQLINFO: %s | parameters: %d", sql_buf, num_parameters);

  CREATE(parameters, struct mysql_parameter, num_parameters);

  int parameter_index = 0;
  for (parameter_index=0,i = genolc_zone_bottom(rzone); i <= zone_table[rzone].top; i++) {
    room_rnum rnum;

    if ((rnum = real_room(i)) != NOWHERE) {

      room = (world + rnum);

      for(j = 0; j < DIR_COUNT; j++) {

        if (R_EXIT(room, j)) {

          if (R_EXIT(room, j)->general_description) {
            strncpy(buf, R_EXIT(room, j)->general_description, sizeof(buf)-1);
            strip_cr(buf);
          } else
            *buf = '\0';

          /* Figure out door flag. */
          if (IS_SET(R_EXIT(room, j)->exit_info, EX_ISDOOR)) {
            if (IS_SET(R_EXIT(room, j)->exit_info, EX_PICKPROOF))
              dflag = 2;
            else
              dflag = 1;

            if (IS_SET(R_EXIT(room, j)->exit_info, EX_HIDDEN))
              dflag += 2;
          } else
            dflag = 0;

          if (R_EXIT(room, j)->keyword)
            strncpy(buf1, R_EXIT(room, j)->keyword, sizeof(buf1)-1 );
          else
            *buf1 = '\0';

          for(col_num = 0; col_num < num_columns; col_num++, parameter_index++)
          {
            parameters[parameter_index].data_type = col[col_num].data_type;

            if(!strcmp(col[col_num].column_name, "Vnum"))
              parameters[parameter_index].int_data = room->number;
            if(!strcmp(col[col_num].column_name, "DirectionNum"))
              parameters[parameter_index].int_data = j;
            else if(!strcmp(col[col_num].column_name, "Keyword"))
              parameters[parameter_index].string_data = strdup(buf1);
            else if(!strcmp(col[col_num].column_name, "Description"))
              parameters[parameter_index].string_data = strdup(buf);
            else if(!strcmp(col[col_num].column_name, "ExitInfo"))
              parameters[parameter_index].int_data = dflag;
            else if(!strcmp(col[col_num].column_name, "KeyNum"))
              parameters[parameter_index].int_data = R_EXIT(room, j)->key != NOTHING ? R_EXIT(room, j)->key : -1;
            else if(!strcmp(col[col_num].column_name, "ToRoom"))
              parameters[parameter_index].int_data = R_EXIT(room, j)->to_room != NOWHERE ? world[R_EXIT(room, j)->to_room].number : -1;

            if(parameters[parameter_index].data_type == MYSQL_TYPE_VAR_STRING && parameters[parameter_index].string_data)
              parameters[parameter_index].data_length = strlen(parameters[parameter_index].string_data);
            else
              parameters[parameter_index].data_length = 0;
          }
        }
      }
    }
  }
  delete_exits_mysql(conn, zone_table[room->zone].number);
  query_stmt_mysql(conn, parameters, NULL, sql_buf, 0, num_parameters, NULL, NULL, MYSQL_QUERY_INSERT);

  free_mysql_parameters(parameters, num_parameters);
}
