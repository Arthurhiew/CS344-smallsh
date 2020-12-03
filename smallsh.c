#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

//I'm sorry for writing everything in main. I just learned how to use C from scratch two weeks ago
//and i'm not really good at it yet. Writing everything in main because i had a bad experience when
//trying to split them into function
int isBackgroundStopped = 0;
int foregroundStatus = 0;
void catchSIGINT(int signo);
void catchSIGTSTP(int signo);

//Signal handler for CTRL + C
void catchSIGINT(int signo)
{
    char *message = "terminated by signal 2\n";
    write(STDERR_FILENO, message, 23);
}

//Signal handler for CTRL + Z
void catchSIGTSTP(int signo)
{
    //check if in foreground or background
    if (isBackgroundStopped == 0)
    {
        char *message = "Entering foreground-only mode (& is now ignored)\n";
        write(STDERR_FILENO, message, 49);
        isBackgroundStopped = 1;
    }
    else if (isBackgroundStopped == 1)
    {
        char *message = "Exiting foreground-only mode\n";
        write(STDERR_FILENO, message, 29);
        isBackgroundStopped = 0;
    }
}

int main()
{
    //Variables
    //argument array
    char *args[512];
    memset(args, '\0', sizeof(args));

    int pid = getpid();
    int pidArray[512];
    memset(pidArray, '\0', sizeof(pidArray));
    int pidIndex = 0;

    //Signal Handlers
    //SIGINT
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = catchSIGINT;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;

    // SIGTSTP
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = catchSIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;

    //ignore signals
    struct sigaction ignore_action = {0};
    ignore_action.sa_handler = SIG_IGN;

    sigaction(SIGINT, &ignore_action, NULL);
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    do
    {
        //Variables
        size_t bufferSize = 0;
        char *buffer = NULL;
        char *token = NULL;
        char temp[2048];
        char inputFile[2048];
        char outputFile[2048];
        int backgroundMode = 0;
        int inputRedirect = 0;
        int outputRedirect = 0;
        int redirect = 0;
        memset(temp, '\0', sizeof(temp));
        memset(inputFile, '\0', sizeof(inputFile));
        memset(outputFile, '\0', sizeof(outputFile));

        printf("%s ", ":");
        fflush(stdout);

        //get user input
        int numCharEntered = getline(&buffer, &bufferSize, stdin);
        if (numCharEntered == -1)
            clearerr(stdin);

        // Remove the trailing \n that getline adds
        buffer[strcspn(buffer, "\n")] = '\0';

        //ignore empty line and comments
        if (strlen(buffer) == 0 || buffer[0] == '#')
            continue;
        //the first token
        token = strtok(buffer, " ");

        //allocate size
        args[0] = calloc(sizeof(token), sizeof(char));

        //copy the token to the first argument
        strcpy(args[0], token);

        // get the next token
        token = strtok(NULL, " ");
        int argsCount = 1;
        //store all the arguments into the array
        while (token != NULL)
        {
            //handle inputs
            if (strcmp(token, "&") == 0)
            { //check if brackground is enabled
                if (isBackgroundStopped == 0)
                    backgroundMode = 1;
            }
            //handle input redirection
            else if (strcmp(token, "<") == 0)
            {
                redirect = 1;
                inputRedirect = 1;
                // take the next argument and store in in inputFile
                token = strtok(NULL, " ");
                //store input name to an array
                strcpy(inputFile, token);
            }
            //handle input redirection
            else if (strcmp(token, ">") == 0)
            {
                redirect = 1;
                outputRedirect = 1;
                // take the next argument and store in in inputFile
                token = strtok(NULL, " ");
                //store input name to an array
                strcpy(outputFile, token);
            }
            else if (strstr(token, "$$")) //expand $$
            {
                //replace $ with null
                token[strcspn(token, "$")] = '\0';
                //convert pid to int
                sprintf(temp, "%d", pid);
                //allocate array size
                args[argsCount] = calloc(sizeof(token) + sizeof(temp), sizeof(char));
                sprintf(temp, "%s%d", token, pid);
                //copy to argument array
                strcpy(args[argsCount], temp);
                argsCount++;
            }
            else //other inputs
            {
                args[argsCount] = calloc(sizeof(token), sizeof(char));
                strcpy(args[argsCount], token);
                argsCount++;
            }

            // go to the next argument
            token = strtok(NULL, " ");
        }

        //Built-in commands
        if (strcmp(args[0], "cd") == 0)
        {
            if (args[1] != NULL)
                chdir(args[1]);
            else //go to home directory if no argument is specified
                chdir(getenv("HOME"));
        }
        else if (strcmp(args[0], "status") == 0) //return the status of the shell
        {
            //check status
            if (WIFEXITED(foregroundStatus) != 0) //check if the process exited normally
                printf("exit value %d\n", WEXITSTATUS(foregroundStatus));
            else if (WIFSIGNALED(foregroundStatus) != 0) //check if the process is terminated by signal
                printf("terminated by signal %d\n", WTERMSIG(foregroundStatus));
            fflush(stdout);
        }
        else if (strcmp(args[0], "exit") == 0) //exit the shell
        {

            int i; //kill all the child process upon exit
            for (i = 0; i < pidIndex; ++i)
            {
                kill(pidArray[i], SIGTERM);
                int childExitMethod = -5;
                pid_t pidStatus = waitpid(pidArray[i], &childExitMethod, 0);
            }

            exit(0);
        }
        else //non-builtin command
        {

            // spawns the new process
            int result;
            pid_t spawnPid = -5;      //bogus value
            int childExitStatus = -5; //bogus value
            spawnPid = fork();        //process id of the child
            switch (spawnPid)
            {
            case -1: //fork failed and no child is created

                perror("Hull Breach!\n");
                exit(1);
                break;

            case 0: // child is created

                //ignore CTRL + Z for for child process
                sigaction(SIGTSTP, &ignore_action, NULL);

                //foreground
                if (backgroundMode == 0 && redirect == 1)
                {
                    //SIGINT for background process
                    sigaction(SIGINT, &SIGINT_action, NULL);

                    //redirect input
                    //check if the inputFile array is empty
                    if (inputRedirect == 1 && strcmp(inputFile, "") != 0)
                    {
                        // if (strcmp(inputFile, "") != 0)

                        //open file decriptor
                        int sourceInput = open(inputFile, O_RDONLY);
                        if (sourceInput == -1)
                        {
                            printf("%s : cannot open file.\n", inputFile);
                            fflush(stdout);
                            exit(1);
                        }
                        //redirect input tp stdin
                        result = dup2(sourceInput, 0);
                        if (result == -1)
                        {
                            perror("source dup2()");
                            exit(1);
                        }
                        //close file on exec
                        fcntl(sourceInput, F_SETFD, FD_CLOEXEC);
                    }

                    //redirect output
                    if (outputRedirect == 1 && strcmp(outputFile, "") != 0)
                    {
                        //check if the outputFile is empty
                        // if ()
                        // {
                        //open file descriptor
                        int targetOutput = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        if (targetOutput == -1)
                        {
                            printf("%s: cannot open file.\n", outputFile);
                            fflush(stdout);
                            exit(1);
                        }
                        //redirect output to stdout
                        result = dup2(targetOutput, 1);

                        if (result == -1)
                        {
                            perror("source dup2()");
                            exit(1);
                        }
                        //close on exec
                        fcntl(targetOutput, F_SETFD, FD_CLOEXEC);
                        // }
                    }
                }
                //background process
                else if (backgroundMode == 1 && redirect == 1)
                {
                    //redirect input or output to dev/null if no target
                    if (inputRedirect == 1 && strcmp(inputFile, "") == 0)
                    {
                        int devIn = open("/dev/null", O_RDONLY);
                        if (devIn == -1)
                        {
                            printf("cannot open /dev/null file.\n");
                            fflush(stdout);
                            exit(1);
                        }
                        result = dup2(devIn, 0);
                        if (result == -1)
                        {
                            perror("source dup2()");
                            exit(1);
                        }
                        fcntl(devIn, F_SETFD, FD_CLOEXEC);
                    }
                    //redirect output
                    if (outputRedirect == 1 && strcmp(outputFile, "") == 0)
                    {
                        int devOut = open("/dev/null", O_RDONLY);
                        if (devOut == -1)
                        {
                            printf("cannot open /dev/null file.\n");
                            fflush(stdout);
                            exit(1);
                        }
                        result = dup2(devOut, 1);
                        if (result == -1)
                        {
                            perror("source dup2()");
                            exit(1);
                        }
                        fcntl(devOut, F_SETFD, FD_CLOEXEC);
                    }
                }
                // exec non-builtin commands
                if (execvp(args[0], args) < 0)
                {
                    printf("%s: no such file or directory\n", args[0]);
                    fflush(stdout);
                    exit(1);
                }
                break;

            default: // in the parent
                if (backgroundMode == 1 && isBackgroundStopped == 0)
                {
                    //add the child pids to the array
                    pidArray[pidIndex] = spawnPid;
                    //shell does not wait for background process to complete
                    pidIndex++;
                    printf("background pid is: %d\n", spawnPid);
                    fflush(stdout);
                }
                else
                {
                    //wait for the the child process to terminate
                    waitpid(spawnPid, &childExitStatus, 0);
                    //set global variable
                    foregroundStatus = childExitStatus;
                }
            }
        }
        int k;
        // check the background process and see if they'd terminated
        for (k = 0; k < pidIndex; ++k)
        {
            //check all the pids
            int childExitStatus = -5;
            pid_t pidStatus = waitpid(pidArray[k], &childExitStatus, WNOHANG);
            if (pidStatus > 0)
            {
                //check status
                printf("background pid %d is done: ", pidArray[k]);
                //check if the child signal has been terminated or exited normally
                if (WIFSIGNALED(childExitStatus) != 0) //check terminated by signal
                {
                    printf("terminated by signal %d\n", WTERMSIG(childExitStatus));
                    fflush(stdout);
                }
                else if (WIFEXITED(childExitStatus) != 0) //child terminated normally
                {
                    printf("exit value %d\n", WEXITSTATUS(childExitStatus));
                    fflush(stdout);
                }
            }
        }

        //Reset the argument array by pointing them to null
        int j;
        for (j = 0; j < argsCount; j++)
            args[j] = '\0';
    } while (1);

    // free the heapn
    int j;
    for (j = 0; j < 512; j++)
        free(args[j]);
    kill(0, SIGKILL);
    return 0;
}
