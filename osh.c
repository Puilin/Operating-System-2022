#include <stdio.h>
#include <unistd.h>
#include <string.h> // for string parsing
#include <stdlib.h> // for free()
#include <sys/wait.h>

#define MAX_LINE 80 /* The maximum length command */

// define prototypes of functions
int takeInput(char **arr);
int count = 0;

int main()
{
    char *args[MAX_LINE/2 + 1]; // array of char pointer(string)
    
    int should_run = 1; // 1: True, 0: False
    int background = 0; // 1: True, 0: False

    pid_t pid, waitPid;

    /**
     * @brief 
     * After reading user input, the steps are:
     * (1) fork a child process using fork()
     * (2) the child process will invoke execvp()
     * (3) parent will invoke wait() unlless command included &
     */
    int i;
    while (1) {
        memset(args, '\0', sizeof(args));
        background = takeInput(args); // take command from user with return code

        pid = fork(); // create new process

        if (pid < 0) { // error
            fprintf(stderr, "Fork Failed\n");
            return 1;
        }
        else if (pid == 0) { // chile process
            execvp(args[0], args);
        }
        else { // parent process
            if (background)
                // continue;
            waitpid(pid, NULL, 0);

            sleep(1); // give delay in order to get results of child process
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
    int code = 0; // default : 0, "&" included (background) : 1
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
        str = (char *) malloc(sizeof(char) * (strlen(parsed) + 1)); // memory allocation
        strcpy(str, parsed); // String copy
        arr[i] = str;
        i++;
        parsed = strtok(NULL, " ");
    }
    arr[i] = 0; // end of array


    return code;

}