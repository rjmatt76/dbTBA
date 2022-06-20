/*************************
*  File:  mysql_db.c
*
**************************/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "mysql_db.h"

MYSQL *database_conn;
struct mysql_connection_strings mysql_connection_strings;

void free_mysql_parameters(struct mysql_parameter *p, int num_parameters)
{
  int i;

  for(i = 0; i < num_parameters; i++)
  {
//    if(p[i].string_data)
//      free(p[i].string_data);
  }
  free(p);
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

  while(*cb[i].column_name != '\n')
  {
    strncat(buf, cb[i].column_name, buf_size - 1);
    if(*cb[i+1].column_name != '\n')
      strncat(buf, ", ", buf_size);
    i++;
  }
  return i;
}

/*
   *buf should be initialized with '\0' or generally 'UPDATE db.table SET ' or the first part of the
     sql statement
   *cb should contain a pointer to the mysql_column with the column names with the final
     row containing '\n'
   creates the list of  set column names delimited by commas in addition to parameter markers
     (except the last one) as required
     in an update statement in the form colname = ?, colname2 = ? ...  
   returns the number of columns and appends buf with the sql
*/
int get_column_update_sql(char *buf, size_t buf_size, const struct mysql_column *cb)
{
  int i = 0;

  while(*cb[i].column_name != '\n')
  {
    strncat(buf, cb[i].column_name, buf_size - 1);
    if(*cb[i+1].column_name != '\n')
      strncat(buf, "= ?, ", buf_size);
    else
      strncat(buf, "= ? ", buf_size);
    i++;
  }
  return i;
}
/*
   creates parameter markers in the form (?,?,?,?), (?,?,?,?), (?,?,?,?)
     where num_columns determines the numbers of ? markers in each parenthesis and
     num_rows determines the numbers of parenthetical groupings.
   returns the total number of parameter markers and appends buf with the marker sql
*/
int get_parameter_markers_sql(char *buf, size_t buf_size, int num_columns, int num_rows)
{
  int i, j, num_parameters = 0;

  for(i = 0; i < num_rows; i++)
  {
    strncat(buf, " (", buf_size-1);
    for(j = 0; j < num_columns; j++)
    {     
      strncat(buf, "?", buf_size-1);
      if(j!= (num_columns-1))
        strncat(buf, ", ", buf_size-1);
    }
    strncat(buf, ")", buf_size-1);
    if(i != (num_rows-1))
      strncat(buf, ", ", buf_size-1);
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

int query_stmt_mysql(MYSQL *conn, struct mysql_parameter *parameters, const struct mysql_column *columns,
  char *statement, int num_columns, int num_parameters,
  void (*load_function)(struct mysql_bind_column *, int, int, void *, MYSQL_STMT *stmt),
  void *ch, int querytype)
{
  MYSQL_STMT *stmt;
  MYSQL_BIND param_bind[num_parameters], col_bind[num_columns]; /* variable sized array may cause a problem in certain compilers */
  int status, i, num_rows;

  //struct that receives the column values from col_bind
  struct mysql_bind_column col_values[num_columns];

  /* initialize the prepared statement */
  stmt = mysql_stmt_init(conn);

  if (!stmt)
  {
    log("MYSQLINFO: Could not initialize statement\n");
    return -1;
  }

  /* prepare the statement */
  status = mysql_stmt_prepare(stmt, statement, strlen(statement));
  test_stmt_error(stmt, status);
  log("MYSQLINFO: prepared statement parameter count - %ld", mysql_stmt_param_count(stmt));

  /* initialize the binding for the parameters (? in SQL statement) */
  memset(param_bind,0,sizeof(MYSQL_BIND)*num_parameters);

  for(i = 0; i < num_parameters; i++)
  {
    if(parameters[i].data_type == MYSQL_TYPE_VAR_STRING)
    {
      param_bind[i].buffer_type = MYSQL_TYPE_VAR_STRING;
      param_bind[i].buffer = (char *) parameters[i].string_data;
      param_bind[i].buffer_length = strlen(parameters[i].string_data)+1;
      param_bind[i].is_null = 0;
      param_bind[i].length = (long unsigned *)&parameters[i].data_length;
    }
    else
    {
      param_bind[i].buffer_type = MYSQL_TYPE_LONG;
      param_bind[i].buffer= (int *) &parameters[i].int_data;
      param_bind[i].is_null = 0;
    }
  }
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
      //log("MYSQLINFO: Parameter %-3d(string): %s, string_length: %d", i, parameters[i].string_data,
      //        (strlen(parameters[i].string_data)+1));
    }
    else
    {
      //log("MYSQLINFO: Parameter %-3d(int) datatype: %ld, int_data: %d", i, parameters[i].data_type, parameters[i].int_data); 
    }
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
      //log("MYSQLINFO: Field - %s %s",columns[i].column_name, columns[i].data_type == MYSQL_TYPE_VAR_STRING ? "string": "not string");

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
      col_bind[i].is_null=(&col_values[i].is_null);
    }
    
    /* now actually bind the result values of the query to col_bind which also fills col_values */
    if (mysql_stmt_bind_result(stmt, col_bind))
    {
      log("MYSQLINFO: mysql_stmt_bind_result() failed");
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
    num_rows = mysql_stmt_num_rows(stmt);
    log("MYSQLINFO: Rows selected: %d", num_rows);

    load_function(col_values, num_columns, num_rows, ch, stmt);
  }
  
  /* Close the statement */
  if (mysql_stmt_close(stmt))
  {
    /* mysql_stmt_close() invalidates stmt, so call          */
    /* mysql_error(mysql) rather than mysql_stmt_error(stmt) */
    test_error(conn, status);
  }
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

