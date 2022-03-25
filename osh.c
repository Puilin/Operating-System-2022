#include <stdio.h>
#include <unistd.h>
#include <string.h> // for string parsing
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h> // for O_RDWR
#include <sys/stat.h>
#include <sys/resource.h>

#define MAX_LINE 80 /* The maximum length command */

// define prototypes of functions
int takeInput(char **arr);
void default_exec(char **args); // + int redirect 인수 추가
void ampersand_exec(char **args); // + int redirect 인수 추가
void redirect_exec(char **args);
void pipe_exec(char **args);

int main()
{
    char *args[MAX_LINE/2 + 1]; // array of char pointer(string)
    
    int code = 0; // default : 0 / "&" included (background) : 1 / "exit" : 2

    

    /**
     * @brief 
     * After reading user input, the steps are:
     * (1) fork a child process using fork()
     * (2) the child process will invoke execvp()
     * (3) parent will invoke wait() unlless command included &
     */
    while (1) {
        memset(args, '\0', sizeof(args));
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
                return 0;
            
            default:
                break;
        }

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

void default_exec(char **args)
{

    pid_t pid, waitPid;

    pid = fork(); // create new process

    if (pid < 0) { // error
        fprintf(stderr, "Fork Failed\n");
        return;
    }
    else if (pid == 0) { // chile process
        execvp(args[0], args);
        fprintf(stdout, "error\n");
    }
    else { // parent process
        waitpid(pid, NULL, 0);
        //sleep(1); // give delay in order to get results of child process
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