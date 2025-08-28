#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <sys/wait.h>
#include <string.h>

// Function to get the parent process ID (PPID) from /proc/[pid]/stat
pid_t retrieveParentPid(pid_t pid)
{
    char filePath[256];
    snprintf(filePath, sizeof(filePath), "/proc/%d/stat", pid); // Construct the path to the stat file
    FILE *filePtr = fopen(filePath, "r");
    if (!filePtr)
        return -1; // Return -1 if the file cannot be opened

    pid_t parentPid;
    fscanf(filePtr, "%*d %*s %*c %d", &parentPid); // Read the PPID from the stat file. %*d : Skip the first integer (process ID). %*s : Skip the second field (executable name). %*c : Skip the third field (state). %d  : Read the fourth integer (parent process ID) into parentPid
    fclose(filePtr);
    return parentPid;
}

// Function to check if a process belongs to a given process tree rooted at root
int isPartOfTree(pid_t root, pid_t pid)
{
    while (pid > 1)
    {
        if (pid == root)
            return 1;                 // Return 1 if the process is part of the tree
        pid = retrieveParentPid(pid); // Move up the process tree
    }
    return 0; // Return 0 if the process does not belong to the tree
}

void killDescendants(int rootPid)
{
    DIR *dir = opendir("/proc");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) // loop through the entries of the /proc directory
    {
        if (entry->d_type == DT_DIR) // check if the entry is a directory. It's important because each active process has a corresponding directory within /proc named by its process ID (PID) /proc/[PID]
        {
            int processId = atoi(entry->d_name);                                           // convert string filename to int
            if (processId > 0 && processId != rootPid && isPartOfTree(rootPid, processId)) // The check processId > 0 is used to ensure that the directory name read from /proc is a valid process ID (check against directories like sys, bus etc in /proc).  processId != rootPid since we do not want to kill rootPid, want to kill descendants
            {
                kill(processId, SIGKILL); // Send SIGKILL to the descendant
            }
        }
    }
    closedir(dir);
}

// Function to send SIGSTOP to all descendants of root_process
void stopDescendants(int rootPid)
{
    DIR *dir = opendir("/proc");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR)
        {
            int processId = atoi(entry->d_name);
            if (processId > 0 && processId != rootPid && isPartOfTree(rootPid, processId))
            {
                kill(processId, SIGSTOP); // Send SIGSTOP to the descendant
            }
        }
    }
    closedir(dir);
}

char getProcessState(int pid)
{
    char filePath[256];
    snprintf(filePath, sizeof(filePath), "/proc/%d/stat", pid); // Construct the path to the stat file
    FILE *filePtr = fopen(filePath, "r");
    if (filePtr)
    {
        char state;
        fscanf(filePtr, "%*d %*s %c", &state); // capture/read the state from the stat file
        fclose(filePtr);
        return state;
    }
    return '\0'; // Return null character if the file cannot be opened
}

// Function to check if a process is stopped
int isProcessStopped(int processId)
{
    char processState = getProcessState(processId);

    return processState == 'T'; // Return 1 if the state is 'T' (stopped), otherwise 0
}

// Function to send SIGCONT to all stopped descendants of a process
void continueDescendants(int rootPid)
{
    DIR *dir = opendir("/proc");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR)
        {
            int processId = atoi(entry->d_name);
            if (processId > 0 && processId != rootPid && isPartOfTree(rootPid, processId) && isProcessStopped(processId)) // isProcessStopped is helper function so SIGCONT only sent to stopped descendant
            {
                kill(processId, SIGCONT); // Send SIGCONT to the stopped descendant
            }
        }
    }
    closedir(dir);
}

// Function to list PIDs of all non-direct descendants of processId
void listNonDirectDescendants(int processId)
{
    DIR *dir = opendir("/proc");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR)
        {
            int pid = atoi(entry->d_name);
            if (pid > 0 && isPartOfTree(processId, pid) && retrieveParentPid(pid) != processId && pid != processId) // retrieveParentPid(pid) != processId makes sure direct descendants does not get printed
            {
                printf("%d\n", pid); // Print PID of the non-direct descendant
            }
        }
    }
    closedir(dir);
}

// Function to list PIDs of all immediate descendants
void listImmediateDescendants(int processId)
{
    DIR *dir = opendir("/proc");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR)
        {
            int pid = atoi(entry->d_name);
            if (pid > 0 && retrieveParentPid(pid) == processId) // retrieveParentPid(pid) == processId make sure immidiate descendants only get printed
            {
                printf("%d\n", pid); // Print PID of the immediate descendant
            }
        }
    }
    closedir(dir);
}

// Function to list PIDs of all siblings
void listSiblings(int processId)
{
    int parentPid = retrieveParentPid(processId); // get processID's parent's process id
    DIR *dir = opendir("/proc");
    struct dirent *entry; // struct to represent directory entries
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR)
        {
            int pid = atoi(entry->d_name);
            if (pid > 0 && retrieveParentPid(pid) == parentPid && pid != processId)
            {                        // retrieveParentPid(pid) == parentPid because siblings have same parent
                printf("%d\n", pid); // Print PID of the sibling
            }
        }
    }
    closedir(dir);
}
// Function to list PIDs of all defunct siblings
void listDefunctSiblings(int processId)
{
    int parentPid = retrieveParentPid(processId);
    DIR *dir = opendir("/proc");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR)
        {
            int pid = atoi(entry->d_name);
            if (pid > 0 && retrieveParentPid(pid) == parentPid && pid != processId)
            {

                char processState = getProcessState(pid); // helper function to get process's state
                if (processState == 'Z')
                {
                    printf("%d\n", pid); // Print PID of the defunct sibling
                }
            }
        }
    }
    closedir(dir);
}

// Function to list PIDs of all defunct descendants
void listDefunctDescendants(int processId)
{
    DIR *dir = opendir("/proc");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR)
        {
            int pid = atoi(entry->d_name);
            if (pid > 0 && isPartOfTree(processId, pid) && pid != processId)
            { // processId is the root
                char processState = getProcessState(pid);
                if (processState == 'Z')
                {
                    printf("%d\n", pid); // Print PID of the defunct descendant
                }
            }
        }
    }
    closedir(dir);
}

// Function to list PIDs of all grandchildren
void listGrandchildren(int processId)
{
    DIR *dir = opendir("/proc");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR)
        {
            int pid = atoi(entry->d_name);
            if (pid > 0 && retrieveParentPid(pid) == processId)
            {                                  // when retrieveParentPid(pid) == processId is true it means pid is direct descendant of processId so we skip it and print immediate descendants of it
                listImmediateDescendants(pid); // Print PIDs of grandchildren
            }
        }
    }
    closedir(dir);
}

// Function to print status of process (Defunct/Not Defunct)
void printDefunctOrNot(int processId)
{
    char processState = getProcessState(processId);
    if (processState == 'Z')
    {
        printf("Defunct\n");
    }
    else
    {
        printf("Not Defunct\n");
    }
}

// Function to kill parents of all zombie processes that are the descendants of proceed_id
void killZombieParents(int processId)
{
    DIR *dir = opendir("/proc");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR)
        {
            int pid = atoi(entry->d_name);
            if (pid > 0 && isPartOfTree(processId, pid) && pid != processId)
            {
                if (getProcessState(pid) == 'Z')
                {
                    int parentPid = retrieveParentPid(pid);
                    kill(parentPid, SIGKILL); // Kill the parent of the zombie process
                }
            }
        }
    }
    closedir(dir);
}

// Function to handle different options
void executeOption(char *option, int rootPid, int pid)

{
    // cases where option is not required
    if (strcmp(option, "-dx") == 0) // check if option=="-dx"
    {
        killDescendants(rootPid); // Kill all descendants of rootPid using SIGKILL
        return;
    }
    if (strcmp(option, "-dt") == 0)
    {
        stopDescendants(rootPid); // Send SIGSTOP to all descendants of rootPid
        return;
    }
    if (strcmp(option, "-dc") == 0)
    {
        continueDescendants(rootPid); // Send SIGCONT to all descendants of rootPid
        return;
    }

    // checking isPartOfTree for the case with option. In main it wasn't checked for case with option
    if (!isPartOfTree(rootPid, pid))
    {
        printf("The process %d does not belong to the tree rooted at %d\n", pid, rootPid);
        return; // return if does not belong
    }

    if (strcmp(option, "-rp") == 0)
    {
        kill(pid, SIGKILL); // Send SIGKILL to the specific process using this system call
    }
    else if (strcmp(option, "-nd") == 0)
    {
        listNonDirectDescendants(pid); // List all non-direct descendants of processId
    }
    else if (strcmp(option, "-dd") == 0)
    {
        listImmediateDescendants(pid); // List all immediate descendants of processId
    }
    else if (strcmp(option, "-sb") == 0)
    {
        listSiblings(pid); // List all siblings of processId
    }
    else if (strcmp(option, "-bz") == 0)
    {
        listDefunctSiblings(pid); // List all defunct siblings of processId
    }
    else if (strcmp(option, "-zd") == 0)
    {
        listDefunctDescendants(pid); // List all defunct descendants of processId
    }
    else if (strcmp(option, "-gc") == 0)
    {
        listGrandchildren(pid); // List all grandchildren of processId
    }
    else if (strcmp(option, "-sz") == 0)
    {
        printDefunctOrNot(pid); // Print status of processId (Defunct/Not Defunct)
    }
    else if (strcmp(option, "-kz") == 0)
    {
        killZombieParents(pid); // Kill parents of all zombie processes that are descendants of processId
    }
    else
    {
        printf("Invalid option.\n");
    }
}

// Helper Function to check if a string is a number
int isNumber(const char *str)
{
    while (*str)
    {
        if (!isdigit(*str))
        {
            return 0; // Not a number
        }
        str++;
    }
    return 1; // Is a number
}

int main(int argc, char *argv[])
{
    // three permutations
    //  ./prc24s [Option] [root_process] //for -dx, -dt and -dc
    //  ./prc24s [Option] [root_process] [process_id]
    //  ./prc24s [root_process] [process_id]

    // Check for valid number of arguments
    if (argc < 3 || argc > 4)
    {
        fprintf(stderr, "Error: Incorrect number of arguments. Usage: %s [Option (optional)] [root_process] [process_id (optional)]\n", argv[0]);
        return 1;
    }

    // Check if the first argument starts with a dash (option provided)
    if (argv[1][0] == '-')
    {
        char *option = argv[1];

        if (!isNumber(argv[2])) // check for root_process to be a number
        {
            fprintf(stderr, "Error: root_process must be a number.\n");
            return 1;
        }
        int rootPid = atoi(argv[2]); // atoi converts string to integer

        char *processId = (argc == 4) ? argv[3] : "0"; // 0 is the default value if processId not provided
        if (!isNumber(processId))                      // check for processId to be a number (in case of argc==4)
        {
            fprintf(stderr, "Error: processId must be a number.\n");
            return 1;
        }

        executeOption(option, rootPid, atoi(processId));
    }
    else
    {

        // Handle case without option: ./prc24s [root_process] [process_id]. Here no need to call executeOption()

        if (!isNumber(argv[1]) || !isNumber(argv[2]))
        {
            fprintf(stderr, "Error: root_process and processId must be a number.\n");
            return 1;
        }

        int rootPid = atoi(argv[1]);
        int processId = atoi(argv[2]);

        if (isPartOfTree(rootPid, processId))
        {
            printf("PID: %d PPID: %d\n", processId, retrieveParentPid(processId));
        }
        else
        {
            printf("The process %d does not belong to the tree rooted at %d\n", processId, rootPid);
        }
    }

    return 0;
}
