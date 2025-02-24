#ifndef _MYSQL_PLAYERS_H_
#define _MYSQL_PLAYERS_H_

void mysql_build_player_index();
int select_player_index_mysql(MYSQL *conn);
void read_aliases_mysql(struct mysql_bind_column *fields, int num_fields, int num_rows, void *v_ch, MYSQL_STMT *stmt);
void read_player_vars_mysql(struct mysql_bind_column *fields, int num_fields, int num_rows, void *v_ch, MYSQL_STMT *stmt);
void read_player_arrays_mysql(struct mysql_bind_column *fields, int num_fields, int num_rows, void *v_ch, MYSQL_STMT *stmt);
void load_playerfile_index_mysql(struct mysql_bind_column *fields, int num_fields, int num_rows, void *v_ch, MYSQL_STMT *stmt);
void select_player_arrays_mysql(MYSQL *conn, struct char_data *ch, int type);
void select_player_vars_mysql(MYSQL *conn, struct char_data *ch);
void insert_player_arrays_mysql(MYSQL *conn, struct char_data *ch, int type);
int load_char_mysql(const char *name, struct char_data *ch);
void save_char_mysql(struct char_data * ch);
void remove_player_mysql(int pfilepos);
void delete_player_arrays_mysql(MYSQL *conn, int id, int type);
void delete_player_vars_mysql(MYSQL *conn, int id);
void delete_aliases_mysql(MYSQL *conn, int id);
int delete_player_mysql(MYSQL *conn, int id);

/* players.c */
void remove_player_from_index(int pos);
#endif
