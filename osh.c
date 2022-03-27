#include <stdio.h>
#include <unistd.h>
#include <string.h> // for string parsing
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h> // for O_RDWR
#include <sys/stat.h>
#include <sys/resource.h>

#define MAX_LINE 80 /* The maximum length command */
#define READ 0
#define WRITE 1
#define MAXBUF 1024

// define prototypes of functions
int takeInput(char **arr);
void check_extension(char **args);
void default_exec(char **args); // + int redirect 인수 추가
void ampersand_exec(char **args); // + int redirect 인수 추가

// global variables
int arr[2]; // placeholder for return values of check_extension

int main()
{
    char *args[MAX_LINE/2 + 1]; // array of char pointer(string)
    
    int code = 0; // default : 0 / "&" included (background) : 1 / "exit" : 2
    int should_run = 1;

    /**
     * @brief 
     * After reading user input, the steps are:
     * (1) fork a child process using fork()
     * (2) the child process will invoke execvp()
     * (3) parent will invoke wait() unlless command included &
     */
    while (should_run) {
        memset(args, '\0', sizeof(args)); // initialize the args
        code = takeInput(args); // take command from user with return code
        
        switch (code)
        {
            case 0:
                default_exec(args);
                break;
            case 1:
                ampersand_exec(args);
                break;
            case 2: // exit
                should_run = 0;
                break;
            default:
                break;
        }

    }

    for (int i=0; i<41; i++) {
        free(args[i]);
    }
    
    return 0;
}

/**
 * @brief 
 * Function that takes commands from user.
 * @param arr array for parsed string
 * @return integer code (default : 0 / "&" included (background) : 1 / "exit" : 2)
 */
int takeInput(char **arr)
{
    int code = 0; // default : 0, "&" included (background) : 1, "exit" : 2
    char line[MAX_LINE]; // Input string
    char *str; // placeholder for parsed string
    printf("osh> ");
    fgets(line, MAX_LINE, stdin);
    line[strlen(line)-1]='\0'; // remove '\n' from input so that the command works

    char *parsed = strtok(line, " "); // Parsing using delimeter " "
    int i = 0;
    while (parsed != NULL) {
        // printf("takeInput: %s", parsed);
        if (strcmp(parsed, "&") == 0) // if there is "&" symbol, the child process will run concurrently
            code = 1;
        else if (strcmp(parsed, "exit") == 0) {
            code = 2;
        }
        str = (char *) malloc(sizeof(char) * (strlen(parsed) + 1)); // memory allocation
        strcpy(str, parsed); // String copy
        arr[i] = str;
        i++;
        parsed = strtok(NULL, " ");
    }
    arr[i] = 0; // end of array


    return code;

}

void check_extension(char **args)
{
    int ex_code = 0;
    int i;
    for (i=0; i<sizeof(args) && args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            ex_code = 1;
            break;
        }
        else if (strcmp(args[i], "<") == 0) {
            ex_code = 2;
            break;
        }
        else if (strcmp(args[i], "|") == 0) {
            ex_code = 3;
            break;
        }
    }

    arr[0] = i;
    arr[1] = ex_code;

}

void default_exec(char **args)
{
    int idx, ex_code, fd;
    pid_t pid, ppid;
    int file_desc[2];
    char *buf[MAX_LINE];

    check_extension(args); // check if there's redirect or pipe
    idx = arr[0]; // where below symbols appear
    ex_code = arr[1]; // default : 0, ">" : 1, "<" : 2, "|" : 3    

    pid = fork(); // create new process

    if (pid < 0) { // error
        fprintf(stderr, "Fork Failed\n");
        return;
    }
    else if (pid == 0) { // child process
        switch (ex_code)
        {
        case 0:
            execvp(args[0], args);
            fprintf(stdout, "error\n");
            break;
        case 1:
            fd = open(args[idx+1], O_CREAT | O_RDWR, 0666);
            dup2(fd, 1); // replace fd as stdout
            close(fd);
            args[idx] = NULL;
            execvp(args[0], args);
            break;
        case 2:
            if (fd = open(args[idx+1], O_RDONLY, 0666) < 0) {
                printf("There's no such file");
            }
            dup2(fd, 0); // replace fd as stdin
            close(fd);
            memmove(args[idx], args[idx+1], sizeof(args[idx+1]));
            execvp(args[0], args);
            break;
        case 3:
            if (pipe(file_desc) < 0) {
                fprintf(stderr, "pipe error");
                return;
            }

            ppid = fork(); // new process again

            if (ppid < 0) {
                fprintf(stderr, "fork error");
                return;
            }
            else if (ppid == 0) { // grandchild process
                close(file_desc[READ]);
                dup2(file_desc[WRITE], 1);
                args[idx] = NULL; // "|"
                execvp(args[0], args);
                fprintf(stderr, "execution failed");
                return;
            }
            else { // child process
                close(file_desc[WRITE]);
                dup2(file_desc[READ], 0);
                // read(file_desc[READ], buf, MAXBUF);
                // for (int i = 0; i<MAXBUF && buf[i] != NULL; i++) {
                //     printf("buffer : %s ,", buf[i]);
                // }
                execvp(args[idx+1], &args[idx+1]);
                fprintf(stdout, "execution failed");
                return;
            }
            waitpid(ppid, NULL, 0);
            break;
        default:
            break;
        }
    }
    else { // grandparent process
        waitpid(pid, NULL, 0);
    }
}

void ampersand_exec(char **args) // 문제 있는지 확인 필요 (fork 한번 더 할지?) 그리고 좀비 프로세스 해결
{
    pid_t pid, ppid;
    pid_t sid = 0;

    struct rlimit rl;
    struct sigaction sa;

    pid = fork(); // create new process

    if (pid < 0) { // error
        fprintf(stderr, "Fork Failed\n");
        return;
    }
    else if (pid == 0) { // child process
        umask(0);
        setsid();
        chdir("/");    
        signal(SIGHUP, SIG_IGN);
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        int i;
        for (i=0; i<sizeof(args) && strcmp(args[i], "&") != 0; i++) {}
        args[i] = NULL;
        execvp(args[0], args);
    }
    else if (pid > 0) { // parent process
        waitpid(pid, NULL, WNOHANG);
        printf("[%d] Run in background\n", pid);
        return;
    }
}