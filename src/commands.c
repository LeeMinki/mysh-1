#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "commands.h"
#include "built_in.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/un.h>


#define  BUFF_SIZE   1024
#define  FILE_SERVER "/tmp/test_server"

static struct built_in_command built_in_commands[] = 
{
  { "cd", do_cd, validate_cd_argv },
  { "pwd", do_pwd, validate_pwd_argv },
  { "fg", do_fg, validate_fg_argv }
};

static int is_built_in_command(const char* command_name)
{
  static const int n_built_in_commands = sizeof(built_in_commands) / sizeof(built_in_commands[0]);

  for (int i = 0; i < n_built_in_commands; ++i) 
  {
    if (strcmp(command_name, built_in_commands[i].command_name) == 0) 
    {
      return i;
    }
  }

  return -1; // Not found
}

/*
 * Description: Currently this function only handles single built_in commands. 
 * You should modify this structure to launch process and offer pipeline functionality.
 */
char g_path[5][50] = {
  "/usr/local/bin/",
  "/usr/bin/",
  "/bin/",
  "/usr/sbin/",
  "/sbin/"
};

int evaluate_command(int n_commands, struct single_command (*commands)[512], int* back)
{
  struct single_command* com = (*commands);

  if(strcmp(com->argv[0], "exit") == 0)
    return 1;
  
  else if(n_commands == 1)
  {
    do_single(com, back);
    return 0;
  }
  else if(n_commands > 1)
  {
    do_pipe(n_commands, com, back);
    return 0;
  }
}

int is_background(struct single_command(*com))
{
  if(strcmp(com->argv[com->argc-1], "&") == 0)
  {
    com->argv[com->argc-1] = '\0';
    com->argc-=1;
    return 1;
  }
  return -1;
}
void do_single(struct single_command (*com), int* back)
{
  int status;
  int is_back;
  assert(com->argc != 0); // com->argc is never 0! if so, exit.
  is_back = is_background(com);
  int built_in_pos = is_built_in_command(com->argv[0]);
  if (built_in_pos != -1) 
    { // argv is cd or pwd or fg. -> 0, 1, 2
    if (built_in_commands[built_in_pos].command_validate(com->argc, com->argv)) 
      { // verifying
        if (built_in_commands[built_in_pos].command_do(com->argc, com->argv, back) != 0) 
          { // if so, do! but if something errors occurs,
            fprintf(stderr, "%s: Error occurs\n", com->argv[0]);
          }
      } 
    else 
      { // ex) cd asdfasdf, cd /bin/ls
        fprintf(stderr, "%s: Invalid arguments\n", com->argv[0]);
        return;
      }
    }
  else if (strcmp(com->argv[0], "") == 0) // first parsing command is empty
    return;
  // set path to absolute
  else 
  {
    int ret = ch_path(com -> argv[0]);
    if(ret == 1)
    {
      if(is_back == 1)
      {
	      setpgid(0, 0);
        tcsetpgrp(0, getpid());
        int pid = fork();
        *back = pid;
        if(pid == 0)
        {
          setpgid(0, 0);
          setpgid(0, getppid());
          execv(com->argv[0], com->argv);
          exit(-1);
        }
        printf("%d\n", *back);
      }
      else
      {
        if(fork() == 0)
          execv(com->argv[0], com->argv);
        wait(&status);
      }
    }
    // There is nothing to do
    else 
    {
      fprintf(stderr, "%s: command not found\n", com->argv[0]);
      return;
    }
   }
  }
  
// I won't touch this.
void free_commands(int n_commands, struct single_command (*commands)[512])
{
  for (int i = 0; i < n_commands; ++i) {
    struct single_command *com = (*commands) + i;
    int argc = com->argc;
    char** argv = com->argv;

    for (int j = 0; j < argc; ++j) {
      free(argv[j]);
    }

    free(argv);
  }

  memset((*commands), 0, sizeof(struct single_command) * n_commands);
}

int ch_path(char* commands)
{
  struct stat buf;
  stat(commands, &buf);
  char path[5][50];
  for(int i = 0; i < 5; i ++)
    strncpy(path[i], g_path[i], sizeof(path[i]));
  for(int i = 0; i< 5; i++)
    strcat(path[i], commands);
  if((!S_ISDIR(buf.st_mode))&&access(commands, R_OK) == 0)
    return 1;
  else
    {
      for(int i = 0; i < 5; i ++)
      {
        stat(path[i], &buf);
        if((!S_ISDIR(buf.st_mode))&&access(path[i], R_OK) == 0)
        {
          for(int j = strlen(path[i]); j < strlen(commands); j++)
            commands[j] = '\0';
          strcpy(commands, path[i]);
          return 1;
        }
      }
      return -1;
    }
  }

void do_pipe(int n_commands, struct single_command (*com), int* back)
{
  int status;
  pthread_t tid;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_create(&tid, &attr, server_thread, com);
  
  if(fork() == 0)
  {
    int client_socket;
    struct sockaddr_un server_addr;
    char buff[BUFF_SIZE+5];
    client_socket  = socket(PF_FILE, SOCK_STREAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family  = AF_UNIX;
    strcpy(server_addr.sun_path, FILE_SERVER);
    while(-1 == connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)));
    if (fork() == 0)
    {
      close(0);
      close(1);
      int ret = dup2(client_socket, 1);
      assert(ret > 0);
      do_single(com, back);
      exit(0);
    }
    wait(&status);
    pthread_join(tid, NULL);
    close(client_socket);
    exit(0);
  }
  wait(&status);
}

  void *server_thread(void *com)
  {
    int back;
    struct single_command *com2 = (struct single_command *)com;
    int status;
    int server_socket;
    int client_socket;
    int client_addr_size;
    int option;
    int size;
    struct sockaddr_un   server_addr;
    struct sockaddr_un   client_addr;
    char buff_rcv[BUFF_SIZE+5];
    char buff_snd[BUFF_SIZE+5];
    if (0 == access(FILE_SERVER, F_OK))
       unlink(FILE_SERVER);
    server_socket  = socket(PF_FILE, SOCK_STREAM, 0);
    memset( &server_addr, 0, sizeof( server_addr));
    server_addr.sun_family  = AF_UNIX;
    strcpy( server_addr.sun_path, FILE_SERVER);
    int b;
    
    while(1)
    {
      b = bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
      assert(b != -1);
      listen(server_socket, 5);
      client_addr_size = sizeof(client_addr);
      client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_size);
      assert(client_socket != -1);
      if(fork() == 0)
      {
        close(0);
        dup2(client_socket, 0);
        do_single(com2+1, &back);
        close(server_socket);
        close(client_socket);
        exit(-1);
      }
      wait(&status);
      close(server_socket);
      close(client_socket);
      pthread_exit(0);
    }
  }