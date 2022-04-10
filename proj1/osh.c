#include <stdio.h>
#include <unistd.h>
#include <string.h> // for string parsing
#include <stdlib.h> // for malloc
#include <sys/wait.h>
#include <fcntl.h> // for O_RDWR

#define MAX_LINE 80 /* The maximum length command */
#define READ 0
#define WRITE 1

// define prototypes of functions
int takeInput(char **arr);
void check_extension(char **args); // check if there is a symbol like ">", "<", "|"
void default_exec(char **args);
void ampersand_exec(char **args); // background (daemon) process

// global variables
int arr[2]; // placeholder for return values of check_extension()

int main()
{
    char *args[MAX_LINE/2 + 1]; // parsed command goes here
    
    int code = 0; // default : 0 / "&" included (background) : 1 / "exit" : 2
    int should_run = 1; // for while loop

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
        fflush(stdout);
        
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
        free(args[i]); // memery deallocation
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
    line[strlen(line)-1] = '\0'; // remove '\n' from input so that the command works

    char *parsed = strtok(line, " "); // Parsing using delimeter " "
    int i = 0;
    while (parsed != NULL) {
        if (strcmp(parsed, "&") == 0) // if there is "&" symbol, the child process will run concurrently
            code = 1;
        else if (strcmp(parsed, "exit") == 0) {
            code = 2;
        }
        str = (char *) malloc(sizeof(char) * (strlen(parsed) + 1)); // memory allocation
        strcpy(str, parsed); // String copy
        arr[i] = str; // save in arr
        i++;
        parsed = strtok(NULL, " ");
    }
    arr[i] = 0; // end of array


    return code;

}

/**
 * @brief check if there is a symbol like ">", "<", "|"
 * 
 * @param args array of parsed command
 */
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

/**
 * @brief command execution without ampersand
 * 
 * @param args array of parsed command
 */
void default_exec(char **args)
{
    int idx, ex_code, fd; // idx : index of delimeter , ex_code : return value of check_extension
    pid_t pid, ppid;
    int file_desc[2]; // for pipe()

    check_extension(args); // check if there's redirect or pipe
    idx = arr[0]; // where below symbols appear
    ex_code = arr[1]; // default : 0, ">" : 1, "<" : 2, "|" : 3    

    pid = fork(); // create new process

    if (pid < 0) { // error
        fprintf(stderr, "Fork Failed\n");
        return;
    }
    else if (pid == 0) {
        switch (ex_code)
        {
        case 0:
            execvp(args[0], args);
            fprintf(stderr, "error\n");
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
            args[idx+1] = NULL;
            execvp(args[0], args);
            break;
        case 3:
            if (pipe(file_desc) < 0) { // create pipe
                fprintf(stderr, "pipe error");
                return;
            }

            ppid = fork(); // new process again

            if (ppid < 0) {
                fprintf(stderr, "fork error");
                return;
            }
            else if (ppid == 0) {
                close(file_desc[READ]);
                dup2(file_desc[WRITE], 1);
                args[idx] = NULL; // "|"
                execvp(args[0], args);
                fprintf(stderr, "execution failed");
                return;
            }
            else {
                close(file_desc[WRITE]);
                dup2(file_desc[READ], 0);
                execvp(args[idx+1], &args[idx+1]);
                fprintf(stderr, "execution failed");
                return;
            }
            waitpid(ppid, NULL, 0);
            break;
        default:
            break;
        }
    }
    else {
        waitpid(pid, NULL, 0);
    }
}

/**
 * @brief command execution with ampersand
 * 
 * @param args array of parsed command
 */
void ampersand_exec(char **args)
{
    int idx, ex_code, fd; // idx : index of delimeter , ex_code : return value of check_extension

    int file_desc[2];
    
    pid_t pid, pid2, pid3;    

    check_extension(args); // check if there's redirect or pipe
    idx = arr[0]; // where below symbols appear
    ex_code = arr[1]; // default : 0, ">" : 1, "<" : 2, "|" : 3    

    pid = fork(); // create new process

    if (pid < 0) { // error
        fprintf(stderr, "Fork Failed\n");
        return;
    }
    else if (pid == 0) { // child process
        pid2 = fork(); // fork again
        if (pid2 < 0) {
            fprintf(stdout, "fork error");
            return;
        }
        else if (pid2 > 0) return;
        else {
            int i;
            for (i=0; i<sizeof(args) && strcmp(args[i], "&") != 0; i++) {}
            args[i] = NULL; // "&" symbol

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
                args[idx] = NULL; // ">"
                execvp(args[0], args);
                break;
            case 2:
                if (fd = open(args[idx+1], O_RDONLY, 0666) < 0) {
                    printf("There's no such file");
                }
                dup2(fd, 0); // replace fd as stdin
                close(fd);
                memmove(args[idx], args[idx+1], sizeof(args[idx+1]));
                args[idx+1] = NULL;
                execvp(args[0], args);
                break;
            case 3:
                if (pipe(file_desc) < 0) { // create pipe
                    fprintf(stderr, "pipe error");
                    return;
                }

                pid3 = fork(); // new process again

                if (pid3 < 0) {
                    fprintf(stderr, "fork error");
                    return;
                }
                else if (pid3 == 0) {
                    close(file_desc[READ]);
                    dup2(file_desc[WRITE], 1);
                    args[idx] = NULL; // "|"
                    execvp(args[0], args);
                    fprintf(stderr, "execution failed");
                    return;
                }
                else {
                    close(file_desc[WRITE]);
                    dup2(file_desc[READ], 0);
                    execvp(args[idx+1], args+idx+1);
                    fprintf(stderr, "execution failed");
                    return;
                }
                waitpid(pid3, NULL, 0);
                break;
            default:
                break;
            }
            
        }
        
    }
    else { // parent process
        printf("[%d] Run in background\n", pid);
        waitpid(-1, NULL, WNOHANG);
        return;
    }

}