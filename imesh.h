#ifndef IMESH_H
#define IMESH_H

#include <stddef.h>

char* get_user_name(void);
int get_host_name(char* buffer, size_t size);
char* get_current_dir(char* buffer, size_t size);
int print_prompt_line(void);
char* handle_pwd_command(void);
char* handle_date_command(void);
void handle_kill_command(char* cmd);
int handle_execute(char *path, char **args);

#endif