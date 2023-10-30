/*
******************************************************************************************
This program is a server which can handle multiple clients and execute commands from them.
******************************************************************************************
 source file:
    shell.c

 compile:
    make

 usage:
  ./shell <Port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <wait.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <stdbool.h>

#define MAX_CONNECTION 1024
#define BUFFER_SIZE 1 << 15
#define ARG_SIZE 32

int PORT = 0;
int connection_idx = -1;
int thread_pid[MAX_CONNECTION];

//-----------------------------------------helper functions----------------------------------------------
// turn commands into more readable format
void trim(char *str) {
    //remove leading and trailing spaces
    int idx = 0;
    int begin = 0;
    while (str[idx] == ' ') {
        idx++;
    }
    begin = idx;
    while (str[idx] != '\0') {
        idx++;
    }
    idx--;
    while (str[idx] == ' ') {
        idx--;
    }
    for (int j = begin; j <= idx; j++) {
        str[j - begin] = str[j];
    }
    str[idx - begin + 1] = '\0';

    //add space between '|' and other characters
    char temp[1024];
    int count = 0;
    for (int i = 0; i < strlen(str); i++) {
        if (str[i] == '|') {
            temp[count] = ' ';
            count++;
            temp[count] = '|';
            count++;
            temp[count] = ' ';
            count++;
        } else {
            temp[count] = str[i];
            count++;
        }
    }
    temp[count] = '\0';
    strcpy(str, temp);

    //remove one more space
    count = 0;
    for (int i = 0; i < strlen(str); i++) {
        if (str[i] == ' ' && str[i + 1] == ' ') {
            continue;
        } else {
            temp[count] = str[i];
            count++;
        }
    }
    temp[count] = '\0';
    strcpy(str, temp);
}
// parse commands into args array
int parseLine(char *line, char *command_array[]) {
    line[strlen(line) - 2] = '\0';
    char *p;
    int count = 0;
    p = strtok(line, " ");
    while (p) {
        command_array[count] = p;
        count++;
        p = strtok(NULL, " ");
    }
    return count;
}
// convert string to int
int stoi(char *str) {
    int num = 0;
    for (int i = 0; i < strlen(str); i++) {
        num = num * 10 + (str[i] - '0');
    }
    return num;
}
// print file name with color and add ' if it contains spaces
void print_fixed_filename(char *filename, char type) {
    //add ' to filename if it contains spaces
    char temp[4096];
    int has_space = 0;
    for (int i = 0; i < strlen(filename); i++) {
        if (filename[i] == ' ') {
            has_space = 1;
            break;
        }
    }
    if (has_space) {
        temp[0] = '\'';
        for (int i = 0; i < strlen(filename); i++) {
            temp[i + 1] = filename[i];
        }
        temp[strlen(filename) + 1] = '\'';
        temp[strlen(filename) + 2] = '\0';
    } else {
        strcpy(temp, filename);
    }

    //if file is a directory, print blue color
    if(type == 'd') {
        printf("\033[1;34m%s\033[0m", temp);
    } else {
        printf("%s", temp);
    }

    fflush(stdout);
}
//--------------------------------graceful shutdown helper------------------------------------------------
bool m_kbhit() {
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
    return (FD_ISSET(0, &fds));
}

//------------------------------------------shell functions-----------------------------------------------
// exit shell
int shell_exit(int argc, char *argv[]) {
    if (argc != 1) {
        printf("\033[1;31mexit: too many arguments\n\033[0m");
        return 0;
    }
    return 1;
}
// change directory
void shell_cd(int argc, char *argv[]) {
    if (argc > 2) {
        printf("\033[1;31mcd: too many arguments\n\033[0m");
        return;
    }
    static char prev_dir[BUFFER_SIZE];
    char temp[BUFFER_SIZE];
    if(argc == 1) {
        chdir(getenv("HOME"));
    } else {
        getcwd(prev_dir, BUFFER_SIZE);
        if (strcmp(argv[1], "~") == 0) {
            chdir(getenv("HOME"));
        } else {
            chdir(argv[1]);
        }
        getcwd(temp, BUFFER_SIZE);
        if (strcmp(prev_dir, temp) == 0 && strcmp(argv[1], "~") != 0 && strcmp(argv[1], ".") != 0) {
            printf("\033[1;31m cd: no such file or directory\n\033[0m");
        }
    }

}
// print current directory
void shell_pwd(int argc, char *argv[]) {
    if (argc != 1) {
        printf("\033[1;31m pwd: too many arguments\n\033[0m");
        return;
    }
    char output[BUFFER_SIZE];
    getcwd(output, BUFFER_SIZE);
    printf("%s", output);
    printf("\n");
}
// list files in current directory
void shell_ls(int argc, char *argv[]) {
    DIR *dir;
    struct dirent *ent;
    int mode = 0;   //0: default, 1: -a, 2: -l
    int dic_begin = 0;  //0: '.'
    int loop = 0;

    //set mode
    if (argc == 1) {
        mode = 0;
    } else if (argv[1][0] == '-') {
        if (strcmp(argv[1], "-a") == 0) {
            mode = 1;
        } else if (strcmp(argv[1], "-l") == 0) {
            mode = 2;
        } else {
            printf("\033[1;31mls: invalid option -- '%c'\n\033[0m", argv[1][1]);
            return;
        }
    } else {
        mode = 0;
    }

    //set dic_begin and loop
    if(argc == 1) {
        dic_begin = 0;
        loop = 1;
    } else if (argc == 2) {
        if(mode == 0) {
            dic_begin = 1;
            loop = 1;
        } else {
            dic_begin = 0;
            loop = 1;
        }
    } else {
        if(mode == 0) {
            dic_begin = 1;
            loop = argc - 1;
        } else {
            dic_begin = 2;
            loop = argc - 2;
        }
    }

    //read all arguments
    for(int i = 0; i < loop; i++) {
        char path[BUFFER_SIZE];

        //set path
        if (dic_begin == 0) {
            getcwd(path, BUFFER_SIZE);
        } else {
            strcpy(path, argv[dic_begin + i]);
        }

        //print dic name if there are more than one dic
        if (loop > 1) {
            printf("%s:\n", path);
        }

        if((dir = opendir(path)) != NULL) {
            while ((ent = readdir(dir)) != NULL) {
                struct stat st;
                stat(ent->d_name, &st);

                char type;
                if (S_ISREG(st.st_mode))
                    type = '-';
                else if (S_ISDIR(st.st_mode))
                    type = 'd';
                else if (S_ISCHR(st.st_mode))
                    type = 'c';
                else if (S_ISBLK(st.st_mode))
                    type = 'b';
                else if (S_ISFIFO(st.st_mode))
                    type = 'p';
                else if (S_ISLNK(st.st_mode))
                    type = 'l';
                else type = 's';

                char mod[10] = "---------";
                if (st.st_mode & S_IRUSR) mod[0] = 'r';
                if (st.st_mode & S_IWUSR) mod[1] = 'w';
                if (st.st_mode & S_IXUSR) mod[2] = 'x';
                if (st.st_mode & S_IRGRP) mod[3] = 'r';
                if (st.st_mode & S_IWGRP) mod[4] = 'w';
                if (st.st_mode & S_IXGRP) mod[5] = 'x';
                if (st.st_mode & S_IROTH) mod[6] = 'r';
                if (st.st_mode & S_IWOTH) mod[7] = 'w';
                if (st.st_mode & S_IXOTH) mod[8] = 'x';

                switch (mode){
                case 0:
                    //ignore ".*"
                    if (ent->d_name[0] != '.') {
                        print_fixed_filename(ent->d_name, type);
                        printf("  ");
                        fflush(stdout);
                    }
                    break;
                 case 1:
                    print_fixed_filename(ent->d_name, type);
                    printf("  ");
                    fflush(stdout);
                    break;
                case 2:
                    //ignore "." and ".."
                    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
                        continue;
                    }
                    //output
                    char temp[BUFFER_SIZE];
                    sprintf(temp, "%c%s %lu %s %s %-6ld %s", type, mod, st.st_nlink,
                            getpwuid(st.st_uid)->pw_name, getgrgid(st.st_gid)->gr_name, st.st_size,
                            ctime(&st.st_mtime));
                    //clean newline
                    temp[strlen(temp) - 1] = '\0';
                    printf("%s ", temp);
                    print_fixed_filename(ent->d_name, type);
                    printf("\n");
                    fflush(stdout);
                    break;
                }
            }
            closedir(dir);
            printf("\n");
            fflush(stdout);
        } else {
            if(mode == 0)
                printf("\033[1;31mls: cannot access '%s': No such file or directory\n\033[0m", argv[i + 1]);
            else
                printf("\033[1;31mls: cannot access '%s': No such file or directory\n\033[0m", argv[i + 2]);
        }

        //print newline if there are more than one dic
        if (loop > 1) {
            printf("\n");
            fflush(stdout);
        }
    }
}
// print file
void shell_cat(int argc, char *argv[]) {
    if (argc == 1) {
        //read from stdin
        char temp[BUFFER_SIZE];
        while (fgets(temp, BUFFER_SIZE, stdin) != NULL) {
            printf("%s", temp);
        }
    } else {
        FILE *src;
        for(int i = 1; i < argc; i++) {
            src = fopen(argv[i], "r1");
            char output[BUFFER_SIZE];
            bzero(output, BUFFER_SIZE);

            if (src == NULL) {
                printf("\033[1;31mcat: cannot open file: %s\n\033[0m", argv[i]);
            } else {
                char temp[BUFFER_SIZE];
                while (fgets(temp, BUFFER_SIZE, src) != NULL) {
                    strcat(output, temp);
                }
                fclose(src);
            }

            printf("%s", output);
            fflush(stdout);
        }
    }
}
// count line, word, byte
void shell_wc(int argc, char *argv[]) {
    unsigned long total_line = 0;
    unsigned long total_word = 0;
    unsigned long total_byte = 0;

    if (argc == 1) {
        //read from stdin
        char temp[BUFFER_SIZE];
        unsigned long line = 0;
        unsigned long word = 0;
        unsigned long byte = 0;

        //read from stdin
        while (fgets(temp, BUFFER_SIZE, stdin) != NULL) {
            line++;
            byte += strlen(temp);
            char *token = strtok(temp, " ");
            while (token != NULL) {
                word++;
                token = strtok(NULL, " ");
            }
        }
        //output
        printf("%-6lu %-6lu %-6lu\n", line, word, byte);
    } else {
        for (int i = 1; i < argc; i++) {
            char temp[BUFFER_SIZE];
            unsigned long line = 0;
            unsigned long word = 0;
            unsigned long byte = 0;
            FILE *src;

            src = fopen(argv[i], "r1");
            if (src == NULL) {
                printf("\033[1;31mwc: cannot open file\n\033[0m");
            } else {
                while (fgets(temp, BUFFER_SIZE, src) != NULL) {
                    line++;
                    byte += strlen(temp);
                    char *token = strtok(temp, " ");
                    while (token != NULL) {
                        word++;
                        token = strtok(NULL, " ");
                    }
                }
                fclose(src);
                printf("%-6lu %-6lu %-6lu %s\n", line, word, byte, argv[i]);
                fflush(stdout);
                total_line += line;
                total_word += word;
                total_byte += byte;
            }
        }
        if (argc > 2) {
            //output green total
            printf("\033[1;32m%-6lu %-6lu %-6lu total\033[0m\n", total_line, total_word, total_byte);
            fflush(stdout);
        }
    }
}
// [most important]parse command and execute it
int shell_exec(int argc, char* argv[]) {
    fflush(stdout);
    if (argv[0] == NULL || argv[0][0] == 0) {
        return 0;
    }
    if (strcmp(argv[0], "exit") == 0) {
        return shell_exit(argc, argv);
    }
    else if (strcmp(argv[0], "cd")  == 0)   shell_cd(argc, argv);
    else if (strcmp(argv[0], "pwd") == 0)   shell_pwd(argc, argv);
    else if (strcmp(argv[0], "ls")  == 0)   shell_ls(argc, argv);
    else if (strcmp(argv[0], "cat") == 0)   shell_cat(argc, argv);
    else if (strcmp(argv[0], "wc")  == 0)   shell_wc(argc, argv);
    else printf("\033[1;31mError executing %s: command not found\033[0m\n", argv[0]);

    return 0;
}
//----------------------------


int main(int port_argc, char *port_argv[]) {
    if(port_argc != 2) {
        printf("Error: invalid number of arguments\n");
        printf("Usage: ./shell <port>\n");
        exit(1);
    }
    for(int i = 0; i < strlen(port_argv[1]); i++) {
        if(port_argv[1][i] < '0' || port_argv[1][i] > '9') {
            printf("Error: invalid port number\n");
            printf("Usage: ./shell <port>\n");
            exit(1);
        }
    }

    printf("--------------------\n");
    printf("Welcome to the shell\n");
    printf("--------------------\n");
    //get port number
    PORT = stoi(port_argv[1]);
    printf("Port number: %d\n", PORT);

    //create socket
    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error opening socket");
        exit(2);
    }

    //bind socket
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT);
    //bind and check error
    int bind_status = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (bind_status < 0) {
        perror("Error binding socket");
        exit(2);
    }

    //listen
    int listen_status = listen(sockfd, 5);
    if (listen_status < 0) {
        perror("Error listening");
        exit(2);
    }

    //create a child process when a client connects
    while (1) {

        //accept
        struct sockaddr_in cli_addr;
        socklen_t clilen = sizeof(cli_addr);
        int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) {
            perror("Error accepting");
            exit(2);
        }

        //graceful shutdown
        if(m_kbhit()) {
            char c;
            //read from stdin
            while (read(STDIN_FILENO, &c, 1) == 1) {
                if(c == 'q' || c == '\t' || c == '\n') break;
            }
            //if q is pressed, wait for all clients exiting
            if(c == 'q') {
                getchar();  //consume the newline
                printf("\033[1;34mThe server will be shutdown, waiting for all clients exiting\033[0m\n");
                for (int i = 0; i < MAX_CONNECTION; i++) {
                    if (thread_pid[i] != 0) {
                        waitpid(thread_pid[i], NULL, 0);
                    }
                }
                exit(0);
            }
        }

        //fork
        connection_idx++;
        pid_t pid = fork();
        thread_pid[connection_idx] = pid;
        if (pid < 0) {
            perror("Error forking");
            exit(2);
        }

        //child process
        if (pid == 0) {
            //close the listening socket
            close(sockfd);
            printf("Connection %d established\n", connection_idx);

            //read and write, exit when done
            char input[BUFFER_SIZE];
            char output[BUFFER_SIZE];
            int exit_signal = 0;
            int total_argc;
            char *argv[ARG_SIZE];
            int check_code;

            while (!exit_signal) {
                //init
                bzero(input, BUFFER_SIZE);
                bzero(output, BUFFER_SIZE);
                bzero(argv, ARG_SIZE);

                //output cyan prompt
                strcpy(output, "\033[1;36m");
                getcwd(output + strlen(output), BUFFER_SIZE);
                strcat(output, "\033[0m$ ");
                write(newsockfd, output, BUFFER_SIZE);
                bzero(output, BUFFER_SIZE);

                //read
                check_code = read(newsockfd, input, BUFFER_SIZE);
                if (check_code < 0) {
                    perror("Error reading from socket");
                    exit(2);
                }

                //parse and execute command
                trim(input);
                total_argc = parseLine(input, argv);

                //find all pipes
                int pipe_index[ARG_SIZE];
                int pipe_count = 0;
                for (int i = 0; i < total_argc; i++) {
                    if (strcmp(argv[i], "|") == 0) {
                        pipe_index[pipe_count] = i;
                        pipe_count++;
                    }
                }

                //check
                if (pipe_count == 0) {
                    //no pipe
                    dup2(newsockfd, STDOUT_FILENO);
                    exit_signal = shell_exec(total_argc, argv);
                }  else {
                    // has pipe
                    if (pipe_index[0] == 0 || pipe_index[pipe_count - 1] == total_argc - 1) {
                        //send error to client
                        char error[BUFFER_SIZE];
                        bzero(error, BUFFER_SIZE);
                        sprintf(error, "\033[1;31mError parsing: pipe at the beginning or end\n\033[0m");
                        write(newsockfd, error, BUFFER_SIZE);
                        continue;
                    }

                    int error_flag = 0;
                    for (int i = 0; i < pipe_count - 1; i++) {
                        if (pipe_index[i + 1] - pipe_index[i] == 1) {
                            error_flag = 1;
                            break;
                        }
                    }

                    if(error_flag) {
                        char error[BUFFER_SIZE];
                        bzero(error, BUFFER_SIZE);
                        sprintf(error, "\033[1;31mError parsing: pipe with no command in between\n\033[0m");
                        write(newsockfd, error, BUFFER_SIZE);
                        continue;
                    }

                    //create pipes and forks
                    int pipefd[pipe_count][2];
                    for (int i = 0; i < pipe_count; i++) {
                        check_code = pipe(pipefd[i]);
                        if(check_code < 0) {
                            perror("Error creating pipe");
                            exit(2);
                        }
                    }

                    //fork
                    pid_t pid[pipe_count + 1];
                    for (int i = 0; i < pipe_count + 1; i++) {
                        pid[i] = fork();
                        if (pid[i] < 0) {
                            perror("Error forking");
                            exit(2);
                        }

                        if (pid[i] == 0) {
                            //child
                            if (i == 0) {
                                //first child
                                close(pipefd[i][0]);
                                dup2(pipefd[i][1], STDOUT_FILENO);
                                exit_signal = shell_exec(pipe_index[i], argv);
                                close(pipefd[i][1]);
                                exit(1);
                            } else if (i == pipe_count) {
                                //last child
                                close(pipefd[i - 1][1]);
                                dup2(pipefd[i - 1][0], STDIN_FILENO);
                                dup2(newsockfd, STDOUT_FILENO);

                                char *new_argv[ARG_SIZE];
                                int new_argc;

                                //parse
                                new_argc = total_argc - pipe_index[i - 1] - 1;
                                for (int j = 0; j < new_argc; j++) {
                                    new_argv[j] = argv[pipe_index[i - 1] + j + 1];
                                }

                                //exec
                                shell_exec(new_argc, new_argv);

                                close(pipefd[i - 1][0]);
                                exit(1);
                            } else {
                                //middle child
                                close(pipefd[i - 1][1]);
                                dup2(pipefd[i - 1][0], STDIN_FILENO);
                                close(pipefd[i][0]);
                                dup2(pipefd[i][1], STDOUT_FILENO);

                                char *new_argv[ARG_SIZE];
                                int new_argc;

                                //parse
                                new_argc = pipe_index[i] - pipe_index[i - 1] - 1;
                                for (int j = 0; j < new_argc; j++) {
                                    new_argv[j] = argv[pipe_index[i - 1] + j + 1];
                                }

                                //exec
                                shell_exec(new_argc, new_argv);

                                close(pipefd[i - 1][0]);
                                close(pipefd[i][1]);
                                exit(1);
                            }
                        } else {
                            //parent
                            //close pipes
                            if (i == 0) {
                                close(pipefd[i][1]);
                            } else if (i == pipe_count) {
                                close(pipefd[i - 1][0]);
                            } else {
                                close(pipefd[i - 1][0]);
                                close(pipefd[i][1]);
                            }

                            //wait
                            waitpid(pid[i], NULL, 0);
                        }
                    }
                }
            }

            //close the socket
            close(newsockfd);
            exit(0);
        } else {
            close(newsockfd);
        }
    }
}
