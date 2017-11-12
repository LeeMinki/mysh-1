#ifndef MYSH_COMMANDS_H_
#define MYSH_COMMANDS_H_

struct single_command
{
  int argc;
  char** argv;
};

int evaluate_command(int n_commands, struct single_command (*commands)[512], int* back);

void do_single(struct single_command (*com), int* back);

void do_pipe(int n_commands, struct single_command (*com), int* back);

int is_background(struct single_command(*com));

void free_commands(int n_commands, struct single_command (*commands)[512]);

int ch_path(char* commands);

void *server_thread(void *com);
#endif // MYSH_COMMANDS_H_
