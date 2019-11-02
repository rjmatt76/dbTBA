#ifndef _MYSQL_DB_H_
#define _MYSQL_DB_H_

#include <mysql.h>

void get_mysql_database_conn();

#define MYSQL_HOST "localhost"
#define MYSQL_USER "strifemud"
/* change this later */
#define MYSQL_PASS "MYSQLPASSWORD"
#define MYSQL_DB "strife_mud"

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

int query_stmt_mysql(MYSQL *conn, struct mysql_parameter_bind_adapter *parameters, struct mysql_column_bind_adapter *columns,
  char *statement, int num_columns, int num_parameters,
  void (*load_function)(struct mysql_bind_column *, int, int, void *, MYSQL_STMT *stmt),
  void *ch, int querytype);
static int test_error(MYSQL *mysql, int status);
static int test_stmt_error(MYSQL_STMT *stmt, int status);

/* global database connection - might end up removing this global connection
   originally had the idea that one connection could be used instead of repeated connections,
   but right now this is just used on initialization to see if the database is there
*/   
MYSQL *database_conn;
void close_mysqlcon_with_error(MYSQL *conn);
MYSQL *create_conn_to_mud_database(MYSQL *conn);


#endif /* _MYSQL_DB_H */
