#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAXLINE 514

int is_adv = 0, tmp = -1;
char *filename;

//print
void myPrint(char *msg)
{
  write(STDOUT_FILENO, msg, strlen(msg));
}

//print with newline
void myPrintln(char *msg) {
  strcat(msg, "\n");
  write(STDOUT_FILENO, msg, strlen(msg));
}

//raise the only error
void error() {
  char error_message[30] = "An error has occurred\n";
  write(STDOUT_FILENO, error_message, strlen(error_message));
}

//for whitespace line edge case
int is_whitespace(const char *s){
  while(isspace(*s))
    s++;
  return *s == '\0' ? 1: 0;
}

//implement builtin command, return 1 if it is a builtin, 0 if not
int builtin_command(char *argv[], int is_redir) {
  if (!strcmp(argv[0], "exit")) {
    if(is_redir){
      error();
      return 1;
    }
    if(argv[1]){
      error();
      return 1;
    }
    exit(0);
  }
  if (!strcmp(argv[0], "cd")) {
    if(is_redir){
      error();
      return 1;
    }
    if (!argv[1]) {
      argv[1] = getenv("HOME");
    }
    if(argv[2]){ //more than 2 argument is error
      error();
      return 1;
    }
    int val = chdir(argv[1]);
    if (val) {
      error();
    }
    return 1;
  }
  if (!strcmp(argv[0], "pwd")) {
    if(is_redir){
      error();
      return 1;
    }
    if(argv[1]){ 
      error();
      return 1;
    }
    char buf[MAXLINE];
    char *cwd = getcwd(buf, MAXLINE);
    if (cwd) {
      myPrintln(cwd);
    }
    return 1;
  }
  else
    return 0;
}

//check if there are more than one ">"
int check_extra_redir(char* cmdline) {
  char *p;
  if((p = strchr(cmdline, '>'))){
    if(strchr(++p, '>')){
      return 1;
    }
  }
  return 0;
}


void copy_files(int src_fd, int dst_fd){
  char buf[MAXLINE];
  int err = 0, n = 1;
   while (1) {
        err = read(src_fd, buf, MAXLINE);
        if (err == -1) {
            error();
            exit(1);
        }
        n = err;

        if (n == 0) return;

        err = write(dst_fd, buf, n);
        if (err == -1) {
          error();
            exit(1);
        }
    }
}


// parse simple commands, return 1 to evaluate, 0 not to evaluate
int parse_line(char *cmdline, char *argv[]) {
  const char *redir = ">";
  //check this first
  if(check_extra_redir(cmdline)){
    error();
    return 0;
  }
  //edge case for dangling ">"
  char save[MAXLINE];
  strcpy(save, cmdline);
  int is_redir = 0;
  char *cmd = strtok(cmdline, redir);
  if(!cmd){
    error();
    return 0;
  }
  if (strcmp(cmd, save)){
    is_redir = 1;
  }
  if (is_redir && is_whitespace(cmd)){
    error();
    return 0;
  }

  //separate the simple command and fill them in the array
  const char *spc = " ";
  int argc = 0;
  char *dst = strtok(0, redir);
  char *token;
  token = strtok(cmd, spc);
  if(!token) return 0;
  while (token != NULL) {
    argv[argc++] = token;
    token = strtok(NULL, spc);
  }
  argv[argc] = 0;

  //builtin command?
  if(builtin_command(argv, is_redir)){
    return 0;
  }

  //edge case for a ">" with no target
  if(is_redir && dst == NULL){
    error();
    return 0;
  }

  if (dst){
    // is advanced redirection?
    //int is_adv = 0; 
    if(dst[0] == '+'){
      if(dst[1] == 0){
        error();
        return 0;
      }
      dst++; // discard the '+'
      is_adv = 1;
    }
    dst = strtok(dst, spc);
    if(strtok(NULL, spc)){
      error(); // deal with edge cases after > 
      return 0;
    }
    int fd;
    if(is_adv && (tmp = open(dst, O_RDWR, 0640)) >= 0){


      fd = open(".tmp", O_WRONLY | O_APPEND | O_CREAT , 0640);
      strcpy(filename, dst);

    } else {
      is_adv = 0;
    if ((fd = open(dst, O_WRONLY | O_CREAT | O_EXCL, 0640)) < 0) {
      error();
      return 0;
    }
  }
    dup2(fd, STDOUT_FILENO);
    close(fd);
  }
  return 1;
}

// evaluate command
void eval(char *cmdline) {
  pid_t pid;
  int status;
  char* argv[MAXLINE];
  if(!parse_line(cmdline, argv)) return;
    pid = fork();
    if (pid == 0) {
      if (execvp(argv[0], argv) < 0) {
        error();
        exit(1);
      }
    } else {
      while (wait(&status) != pid) {}
    }
}

//deal with semicolons
void parse_semicolon(char *cmd) {
  char *token, *saveptr;
  const char *delim = ";\n\t";
  token = strtok_r(cmd, delim, &saveptr);
  while (token != NULL) {
    eval(token);
    token = strtok_r(NULL, delim, &saveptr);
  }
}

int main(int argc, char *argv[])
{
  //dup2(1, 2);
  char *cmd_buf = NULL;
  ssize_t rd;
  size_t len = 0;
  int saved_stdout = dup(STDOUT_FILENO);
  filename = (char*)malloc(MAXLINE);
  if (argc == 1) { // cmdline mode
    while (1) {
      dup2(saved_stdout, 1);
      myPrint("myshell> ");
      if ((rd = getline(&cmd_buf, &len, stdin))== -1) {
        if (feof(stdin)) {
          exit(0);
        }
        error();
      }
       if(strlen(cmd_buf )> MAXLINE){ //too long input
        myPrint(cmd_buf);
        error();
        continue;
      }
      parse_semicolon(cmd_buf);
      if(is_adv){
        copy_files(tmp, 1);
        close(tmp);
        remove(filename);
        rename(".tmp", filename);
      }
    }
  } else if (argc == 2) { // batch mode
    char* file = argv[1];
    FILE *f = fopen(file, "r");
    if (!f) {
      error();
      exit(1);
    }
    while (1) {
      dup2(saved_stdout, 1);
      if ((rd = getline(&cmd_buf, &len, f))== -1) {
        if (feof(f)) {
          exit(0);
        }
        error();
      }
      if(strlen(cmd_buf)>MAXLINE){ //too long input
        myPrint(cmd_buf);
        error();
        continue;
      }
      if (!(cmd_buf[0]) || is_whitespace(cmd_buf)) { // handle blank line
        continue;
      }
      myPrint(cmd_buf);
      parse_semicolon(cmd_buf);
      if(is_adv){
        copy_files(tmp, 1);
        close(tmp);
        remove(filename);
        rename(".tmp", filename);
      }
    }
    free(filename);
    free(cmd_buf);
    fclose(f);
  } else {
    error();
    exit(0);
  }
  return 0;
}
