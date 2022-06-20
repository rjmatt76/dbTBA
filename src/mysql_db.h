#ifndef _MYSQL_DB_H_
#define _MYSQL_DB_H_

#include <mysql.h>

void get_mysql_database_conn();

typedef enum
{
  MYSQL_QUERY_SELECT,
  MYSQL_QUERY_UPDATE,
  MYSQL_QUERY_DELETE,
  MYSQL_QUERY_INSERT
} MySqlQueryType;

#define MYSQL_PLAYER_TABLE "playerfile"
#define MYSQL_ALIAS_TABLE "player_alias"
#define MYSQL_PLAYER_OBJECTS_TABLE "player_objects"
#define MYSQL_PLAYER_ARRAYS_TABLE "player_arrays"
//dg trigger variables saved
#define MYSQL_PLAYER_VARS_TABLE "player_vars"

#define MYSQL_ROOM_TABLE "rooms"
#define MYSQL_ROOM_DIR_TABLE "room_direction_data"
#define MYSQL_OBJECT_TABLE "objects"
#define MYSQL_MOBILE_TABLE "mobiles"
#define MYSQL_ZONE_TABLE "zones"
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
  const char *column_name;
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
  bool is_null;
  unsigned long col_length;
  unsigned long buffer_length;
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

/* global database connection - might end up removing this global connection
   originally had the idea that one connection could be used instead of repeated connections,
   but right now this is just used on initialization to see if the database is there
*/   
extern MYSQL *database_conn;
void close_mysqlcon_with_error(MYSQL *conn);
MYSQL *create_conn_to_mud_database(MYSQL *conn);


#endif /* _MYSQL_DB_H */
