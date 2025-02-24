/*************************
*  File:   mysql_db.c
*  Author: Cyric/strifemud
**************************/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "mysql_db.h"

/* Global database connection */
MYSQL *db_conn = NULL;
struct mysql_connection_strings mysql_connection_strings;

void free_mysql_parameters(struct mysql_parameter *p, int num_parameters)
{
  int i;

  for(i = 0; i < num_parameters; i++)
  {
    if(p[i].string_data)
      free(p[i].string_data);
  }
  free(p);
}

void free_mysql_columns(struct mysql_column *col, int num_columns)
{
  int i;

  for(i = 0; i < num_columns; i++)
  {
    if(col[i].column_name)
      free(col[i].column_name);
  }
  free(col);
}

/*
   *buf should be initialized with '\0' or generally 'SELECT ' or the first part of the
     sql statement
   *cb should contain a pointer to the mysql_column with the column names with the final
     row containing '\n'
   creates the list of column names delimited by commas (except the last one) as required
     in a select, or insert (not UPDATE) statement
   returns the number of columns and appends buf with the sql
*/
int get_column_sql(char *buf, size_t buf_size, const struct mysql_column *cb)
{
    int i = 0;
    size_t len = strlen(buf);

    while (cb[i].column_name != NULL && *cb[i].column_name != '\n')
    {
        // Ensure there's enough space in the buffer
        int written = snprintf(buf + len, buf_size - len, "%s", cb[i].column_name);
        if (written < 0 || (size_t)written >= buf_size - len) {
            break;  // Prevent buffer overflow
        }
        len += written;

        // Add a comma unless it's the last column
        if (cb[i + 1].column_name != NULL && *cb[i + 1].column_name != '\n')
        {
            written = snprintf(buf + len, buf_size - len, ", ");
            if (written < 0 || (size_t)written >= buf_size - len) {
                break;
            }
            len += written;
        }

        i++;
    }
    return i;
}

/* *buf should be initialized with '\0' or generally 'UPDATE db.table SET ' or the first part of the
     sql statement
   *cb should contain a pointer to the mysql_column with the column names with the final
     row containing '\n'
   creates the list of  set column names delimited by commas in addition to parameter markers
     (except the last one) as required
     in an update statement in the form colname = ?, colname2 = ? ...
   returns the number of columns and appends buf with the sql */
int get_column_update_sql(char *buf, size_t buf_size, const struct mysql_column *cb)
{
  int i = 0;

  while (cb[i].column_name != NULL && *cb[i].column_name != '\n')
  {
    strncat(buf, cb[i].column_name, buf_size - strlen(buf) - 1);
    if (cb[i + 1].column_name != NULL && *cb[i + 1].column_name != '\n')
      strncat(buf, "= ?, ", buf_size - strlen(buf) - 1);
    else
      strncat(buf, "= ? ", buf_size - strlen(buf) - 1);
    i++;
  }
  return i;
}
/* Creates parameter markers in the form (?,?,?,?), (?,?,?,?), (?,?,?,?)
   where num_columns determines the numbers of ? markers in each parenthesis and
   num_rows determines the numbers of parenthetical groupings.
   returns the total number of parameter markers and appends buf with the marker sql */
int get_parameter_markers_sql(char *buf, size_t buf_size, int num_columns, int num_rows)
{
  int i, j, num_parameters = 0;

  for(i = 0; i < num_rows; i++)
  {
    strncat(buf, " (", buf_size - strlen(buf) - 1);
    for(j = 0; j < num_columns; j++)
    {
      strncat(buf, "?", buf_size - strlen(buf) - 1);
      if(j!= (num_columns-1))
        strncat(buf, ", ", buf_size - strlen(buf) - 1);
    }
    strncat(buf, ")", buf_size - strlen(buf) - 1);
    if(i != (num_rows-1))
      strncat(buf, ", ", buf_size - strlen(buf) - 1);
    num_parameters += num_columns;
  }
  return num_parameters;
}

/*  for assigning column definitions to a parameter list in an insert or update statement */
//void assign_mysql_bind_parameters(struct mysql_parameters_bind_adapter *p,
//  struct mysql_column *col, int num_parameters)
//{
//  int i;
//  CREATE(p, struct mysql_parameter, num_parameters);

//  i = 0;
//  while(*col[i].column_name != '\n')
//  {
//    p[i].data_type = *col[i].data_type;
//  }
//}

/* should close the mysql connection and log an error */
void close_mysqlcon_with_error(MYSQL *conn)
{
  log("MYSQLINFO: %s", mysql_error(conn));
  mysql_close(conn);
}

int connect_primary_mysql_db()
{
  /* first time connect */
  if(create_mysql_conn(MYSQL_HOST, MYSQL_USER, MYSQL_PASS, MYSQL_DB))
    /* ip address of mysql server, database, password, tablename */
      log("MYSQLINFO: Successfully connected to database %s@%s.%s.", MYSQL_USER, MYSQL_DB, MYSQL_HOST);
  else
  {
    log("MYSQLINFO: Failure connecting to database %s@%s.%s", MYSQL_USER, MYSQL_DB, MYSQL_HOST);
    exit(0);
  }
  return TRUE;
}

void ping_mysql_database()
{
  mysql_ping(db_conn);
}

int create_mysql_conn(char *host, char *user, char *pass, char *db)
{
  char buf[MAX_STRING_LENGTH];

  if (!(db_conn = mysql_init(NULL)))
  {
      snprintf(buf, sizeof(buf), "mysql_init() failed\n");
      log("MYSQLINFO: %s", buf);
      return FALSE;
  }

  /* This doesn't seem to work to reconnect if a mysql_ping is called
   * before a mysql_query if disconnected (default wait_timeout is 28800 seconds
   * or 8 hours if you run the mysql query: show variables like '%timeout%').
   * What I do is call ping_mysql_database() every 5 minutes to keep
   * it from timing out in the first place. -Cyric */
  bool bool_arg = 1;
  mysql_options(db_conn, MYSQL_OPT_RECONNECT, &bool_arg);

  if (!mysql_real_connect(db_conn, host, user, pass, db, 0, NULL, 0))
  {
    close_mysqlcon_with_error(db_conn);
    return FALSE;
  }

  return TRUE;
}

void disconnect_primary_mysql_db()
{
  mysql_close(db_conn);
}

void bind_parameter(MYSQL_BIND *param_bind, struct mysql_parameter *param) {
  if(param->data_type == MYSQL_TYPE_VAR_STRING) {
    param_bind->buffer_type = MYSQL_TYPE_VAR_STRING;
    param_bind->buffer = (char *) param->string_data;
    param_bind->buffer_length = strlen(param->string_data) + 1;
    param_bind->is_null = 0;
    param_bind->length = (long unsigned *)&param->data_length;
  } else if (param->data_type == MYSQL_TYPE_LONG) {
    param_bind->buffer_type = MYSQL_TYPE_LONG;
    param_bind->buffer = (int *) &param->int_data;
    param_bind->is_null = 0;
  }
  // Add additional data types as needed
}

/* TODO: add the option to COMMIT/ROLLBACK multiple queries as a transaction */
int query_stmt_mysql(MYSQL *conn, struct mysql_parameter *parameters, const struct mysql_column *columns,
  char *statement, int num_columns, int num_parameters,
  void (*load_function)(struct mysql_bind_column *, int, int, void *, MYSQL_STMT *stmt),
  void *ch, int querytype)
{
  MYSQL_STMT *stmt;
  MYSQL_BIND *param_bind;
  MYSQL_BIND *col_bind;
  int status, i, num_rows;

  // Allocate memory for the bindings
  param_bind = (MYSQL_BIND *)malloc(sizeof(MYSQL_BIND) * num_parameters);
  col_bind = (MYSQL_BIND *)malloc(sizeof(MYSQL_BIND) * num_columns);
  //struct that receives the column values from col_bind
  struct mysql_bind_column *col_values = (struct mysql_bind_column *)malloc(sizeof(struct mysql_bind_column) * num_columns);


  if (!param_bind || !col_bind) {
    log("Memory allocation error\n");
    free(param_bind);
    free(col_bind);
    free(col_values);
    return -1;
  }

  /* initialize the prepared statement */
  stmt = mysql_stmt_init(conn);

  if (!stmt)
  {
    log("MYSQLINFO: Could not initialize statement\n");
    free(param_bind);
    free(col_bind);
    free(col_values);
    return -1;
  }

  /* prepare the statement */
  status = mysql_stmt_prepare(stmt, statement, strlen(statement));
  test_stmt_error(stmt, status);

  if(EXTRA_MYSQL_DEBUG_LOGS)
    log("MYSQLINFO: prepared statement parameter count - %lu", mysql_stmt_param_count(stmt));

  /* initialize the binding for the parameters (? in SQL statement) */
  memset(param_bind, 0, sizeof(MYSQL_BIND) * num_parameters);

  for(i = 0; i < num_parameters; i++) {
    bind_parameter(&param_bind[i], &parameters[i]);
  }

  status = mysql_stmt_bind_param(stmt, param_bind);
  test_stmt_error(stmt, status);

  /* set the param_bind[i].length to the length of the column name (via the address of parameters[i].data_length)
     mysql wants this set after binding parameters */
  for(i = 0; i < num_parameters; i++)
  {
    if(parameters[i].data_type == MYSQL_TYPE_VAR_STRING)
      parameters[i].data_length = strlen(parameters[i].string_data);
  }

  /* execute the statement now that it has its parameters defined */
  status = mysql_stmt_execute(stmt);
  test_stmt_error(stmt, status);

  if(querytype == MYSQL_QUERY_SELECT)
  {
    /* prepare the col_bind to receive the values of the query */
    memset(col_bind,0,sizeof(MYSQL_BIND)*num_columns);
   /* point the col_bind to the col_values which is our own structure to store these values in
    */
    for(i = 0; i < num_columns; i++)
    {
      /* store our column_name in our col_value structure (since it is not available in the bind data) */
      strcpy(col_values[i].name, columns[i].column_name);

      /* right now just either mysql var string or longs  */
      if(columns[i].data_type == MYSQL_TYPE_VAR_STRING)
      {
        col_bind[i].buffer_type=MYSQL_TYPE_STRING;
        col_bind[i].buffer=(char *) (col_values[i].col_string_buffer);
        col_bind[i].buffer_length=4097;
        col_bind[i].length = &col_values[i].buffer_length;
      }
      else
      {
        col_bind[i].buffer_type=MYSQL_TYPE_LONG;
        col_bind[i].buffer=(int *)(&col_values[i].col_int_buffer);
      }
      col_bind[i].is_null = (bool *)(&col_values[i].is_null);
      col_values[i].data_type = columns[i].data_type;
    }

    /* now actually bind the result values of the query to col_bind which also fills col_values */
    if (mysql_stmt_bind_result(stmt, col_bind))
    {
      log("MYSQLINFO: mysql_stmt_bind_result() failed %s\n", mysql_stmt_error(stmt));
      free(param_bind);
      free(col_bind);
      free(col_values);
      return -1;
    }

    /* Buffer all results to client */
    if (mysql_stmt_store_result(stmt))
    {
      fprintf(stderr, " mysql_stmt_store_result() failed\n%s\n", mysql_stmt_error(stmt));
      free(param_bind);
      free(col_bind);
      free(col_values);
      return (-1);
    }
    num_rows = mysql_stmt_num_rows(stmt);

    if(EXTRA_MYSQL_DEBUG_LOGS)
      log("MYSQLINFO: Rows selected: %d", num_rows);

    load_function(col_values, num_columns, num_rows, ch, stmt);

    if (mysql_stmt_free_result(stmt))
    {
      fprintf(stderr, " mysql_stmt_free_result() failed!\n%s\n", mysql_stmt_error(stmt));
      free(param_bind);
      free(col_bind);
      free(col_values);
      return (-1);
    }
  }

  /* Close the statement */
  if (mysql_stmt_close(stmt))
  {
    /* mysql_stmt_close() invalidates stmt, so call          */
    /* mysql_error(mysql) rather than mysql_stmt_error(stmt) */
    test_error(conn, status);
  }
  free(param_bind);
  free(col_bind);
  free(col_values);
  return 1;
}

int test_error(MYSQL *mysql, int status)
{
  if (status)
  {
    log("MYSQLINFO: Error: %s (errno: %d)\n",
            mysql_error(mysql), mysql_errno(mysql));
    return -1;
  }
  return 1;
}

int test_stmt_error(MYSQL_STMT *stmt, int status)
{
  if (status)
  {
    log("MYSQLINFO: Error: %s (errno: %d)\n",
            mysql_stmt_error(stmt), mysql_stmt_errno(stmt));
    return -1;
  }
  return 1;
}

void create_mud_db_tables()
{
  char buf[MAX_STRING_LENGTH];

  mysql_ping(db_conn);

  struct mysql_table_script
  {
    const char *table;
    const char *script;
  };

  #define NUM_MYSQL_TABLES 12
  const struct mysql_table_script mysql_tables[NUM_MYSQL_TABLES] = {
    { MYSQL_PLAYER_TABLE,         CREATE_PLAYERFILE_TABLE     },
    { MYSQL_PLAYER_ARRAYS_TABLE,  CREATE_PLAYER_ARRAYS_TABLE  },
    { MYSQL_ALIAS_TABLE,          CREATE_ALIAS_TABLE          },
    { MYSQL_PLAYER_VARS_TABLE,    CREATE_PLAYER_VARS_TABLE    },
    { MYSQL_PLAYER_OBJECTS_TABLE, CREATE_PLAYER_OBJECTS_TABLE },
    { MYSQL_ROOM_TABLE,           CREATE_ROOMS_TABLE          },
    { MYSQL_ROOM_DIR_TABLE,       CREATE_ROOM_DIRS_TABLE      },
//    { MYSQL_PLAYER_CLANS_TABLE,   CREATE_PLAYER_CLANS_TABLE   },
    { MYSQL_MOBILE_TABLE,         CREATE_MOBILES_TABLE        },
    { MYSQL_OBJECT_TABLE,         CREATE_OBJECTS_TABLE        },
    { MYSQL_ZONE_TABLE,           CREATE_ZONES_TABLE          },
    { MYSQL_ZONE_COMMANDS_TABLE,  CREATE_ZONE_COMMANDS_TABLE  },
    { MYSQL_TRIGGER_TABLE,        CREATE_TRIGGERS_TABLE       }
  };

  for(int i = 0; i < NUM_MYSQL_TABLES; i++)
  {
    snprintf(buf, sizeof(buf), mysql_tables[i].script, MYSQL_DB, mysql_tables[i].table);
    if (mysql_query(db_conn, buf))
    {
      log("MYSQLINFO: CREATE TABLE (%s) failed, %s\n", mysql_tables[i].table, mysql_error(db_conn));
    }
  }
}
