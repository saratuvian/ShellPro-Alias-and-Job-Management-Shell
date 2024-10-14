#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_CMD_LENGTH 1024
#define MAX_ARGS 4

// Define a structure for each alias
typedef struct Alias {
    char* shortcut;
    char* command;
    struct Alias* next;
} Alias;

Alias* aliases_head = NULL; // Head of the linked list

int num_commands = 0;
int num_script_lines = 0;
int num_aliases = 0;

// Define a structure for background jobs
typedef struct Job {
    int job_id;
    pid_t pid;
    char* command;
    struct Job* next;
} Job;

Job* jobs_head = NULL; // Head of the background jobs list
int job_counter = 1; // Counter for job IDs

void display_prompt() {
    printf("#cmd:%d|#alias:%d|#script lines:%d> ", num_commands, num_aliases, num_script_lines);
}

// Function to add an alias to the linked list
void add_alias(char* input) {
    // Check if the input is exactly "alias"
    if (strcmp(input, "alias") == 0) {
        // Do nothing and return
        return;
    }
    // Skip the word "alias"
    char* input_copy = strdup(input + 6);
    char* shortcut, * command;
    char* token = strtok(input_copy, "=");
    if (token == NULL) {
        printf("Invalid alias format.\n");
        return;
    }

    // Trim leading and trailing spaces from the alias name
    while (*token == ' ') token++; // Skip leading spaces
    char* end = token + strlen(token) - 1;
    while (end > token && *end == ' ') end--; // Skip trailing spaces
    *(end + 1) = '\0'; // Null-terminate the trimmed string
    shortcut = strdup(token);

    token = strtok(NULL, "'");
    if (token == NULL) {
        printf("Invalid alias format.\n");
        free(shortcut);
        free(input_copy);
        return;
    }

    // Trim leading and trailing spaces from the alias command
    while (*token == ' ') token++; // Skip leading spaces
    end = token + strlen(token) - 1;
    while (end > token && *end == ' ') end--; // Skip trailing spaces
    *(end + 1) = '\0'; // Null-terminate the trimmed string
    command = strdup(token);

    // Check if alias already exists
    Alias* current = aliases_head;
    while (current != NULL) {
        if (strcmp(current->shortcut, shortcut) == 0) {
            printf("Alias already exists.\n");
            free(shortcut);
            free(command);
            return;
        }
        current = current->next;
    }

    // Add the alias if it doesn't already exist
    Alias* new_alias = (Alias*)malloc(sizeof(Alias));
    if (new_alias == NULL) {
        printf("Memory allocation failed for alias.\n");
        free(shortcut);
        free(command);
        return;
    }
    new_alias->shortcut = strdup(shortcut); // Allocate memory for the shortcut string
    new_alias->command = strdup(command); // Allocate memory for the command string
    new_alias->next = aliases_head;

    aliases_head = new_alias;
    num_aliases++; // Increment the number of aliases
}

// Function to remove an alias from the linked list
void remove_alias(char* alias) {
    Alias* current = aliases_head;
    Alias* prev = NULL;

    while (current != NULL) {
        if (strcmp(current->shortcut, alias) == 0) {
            if (prev == NULL) {
                aliases_head = current->next;
            }
            else {
                prev->next = current->next;
            }
            free(current->shortcut);
            free(current->command);
            free(current);
            num_aliases--;
            return;
        }
        prev = current;
        current = current->next;
    }
    printf("Error: Alias not found.\n");
}

// Function to expand an alias if it exists
char* expand_alias(char* command) {
    Alias* current = aliases_head;
    while (current != NULL) {
        if (strcmp(command, current->shortcut) == 0) {
            return strdup(current->command);
        }
        current = current->next;
    }
    return strdup(command);
}

// Function to add a job to the jobs list
void add_job(pid_t pid, char* command) {
    Job* new_job = (Job*)malloc(sizeof(Job));
    if (new_job == NULL) {
        printf("Memory allocation failed for job.\n");
        return;
    }
    new_job->job_id = job_counter++;
    new_job->pid = pid;
    new_job->command = strdup(command);
    new_job->next = jobs_head;
    jobs_head = new_job;
}

// Function to remove a job from the jobs list
void remove_job(pid_t pid) {
    Job* current = jobs_head;
    Job* prev = NULL;
    while (current != NULL) {
        if (current->pid == pid) {
            if (prev == NULL) {
                jobs_head = current->next;
            } else {
                prev->next = current->next;
            }
            free(current->command);
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

/// Function to execute a command
int execute_command(char* command) {
    // Check if the command ends with '&'
    int background = 0;
    if (strlen(command) > 0 && command[strlen(command) - 1] == '&') {
        background = 1;
        command[strlen(command) - 1] = '\0'; // Remove '&' from the command
    }

    char* expanded_command = expand_alias(command); // Expand the command if it's an alias
    char* args[MAX_ARGS + 2];
    int arg_count = 0;
    char* token;
    char* rest;
    if (strncmp(expanded_command, "echo ", 5) == 0) {
        token = strtok_r(expanded_command, " ", &rest);
        args[0] = token;
        int length = strlen(rest);
        if (rest[0] == '"' && rest[length - 1] == '"') {
            rest[length - 1] = '\0';  // Replace the closing quote with null terminator
            rest++;
        }
        args[1] = strdup(rest);
        args[2] = NULL;
    } else {
        token = strtok(expanded_command, " ");
        while (token != NULL && arg_count < MAX_ARGS + 1) {
            args[arg_count++] = token;
            token = strtok(NULL, " ");
        }
        args[arg_count] = NULL;

        if (arg_count > MAX_ARGS) {
            printf("Invalid command: Too many arguments.\n");
            free(expanded_command); // Free the memory allocated for expanded_command
            return -1;
        }
    }

    int ifdup = 0;
    int fd;

    // Handle stderr redirection
    if (strstr(command, "2>") != NULL) {
        char *filename;
        char *comm = strtok_r(command, "2>", &filename);
        filename++; // Skip the space after "2>"
        args[0] = comm;
        args[2] = filename;

        fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
        if (fd == -1) {
            perror("Error opening file");
            return -1;
        }
        ifdup = 1;
    }


    // Execute external command
    pid_t pid = fork();
    if (pid == 0) { // Child process
        if (background) {
            setpgid(0, 0); // Put child process in a new process group for background execution
        }
        if (ifdup == 1) {
            dup2(fd, STDERR_FILENO);
            close(fd); // Close fd after duplicating
        }
        execvp(args[0], args);
        perror("Exec failed");
        exit(1);
    } else if (pid < 0) {
        perror("Error");
        _exit(EXIT_FAILURE);
    } else {
        if (!background) {
            int status;
            waitpid(pid, &status, 0);
        } else {
            add_job(pid, command); // Add the job to the background jobs list
            printf("[%d] %d\n", 1, pid); // Assuming job_counter is 1 for simplicity
        }
    }

    if (ifdup == 1) {
        close(fd); // Close fd in parent process if it was opened
    }

    free(expanded_command); // Free the memory allocated for expanded_command
    return 0;
}

// Function to list all background jobs
void list_jobs() {
    Job* current = jobs_head;
    while (current != NULL) {
        int status;
        pid_t result = waitpid(current->pid, &status, WNOHANG);
        if (result == 0) {
            // Job is still running
            printf("[%d] running %s &\n", current->job_id, current->command);
        } else {
            Job* temp = current;
            current = current->next;
            remove_job(temp->pid); // Remove the job from the list
            continue; // Skip incrementing current
        }
        current = current->next;
    }
}

void execute_conditions(char* command) {
    char* token;
    char* saveptr;
    char* commands[3]; // Array to hold up to 3 commands
    int command_count = 0;
    int fd;
    int ifdup = 0;
    char *comm;
    // Handle stderr redirection at the end of the command
    if (strstr(command, "2>") != NULL) {
        char *filename;
        comm = strtok_r(command, "2>", &filename);
        filename++; // Skip the space after "2>"
        int length2 = strlen(comm);
        comm[length2 - 2] = '\0'; // Skip the last character
        comm++; // Skip the first character
        fd = open(filename, O_RDWR | O_CREAT | O_APPEND, 0666);
        if (fd == -1) {
            perror("Error opening file for redirection");
            return;
        }
        ifdup = 1;
        command = comm;
    }
    // Split the command by '||'
    token = strtok_r(command, "||", &saveptr);
    while (token != NULL && command_count < 3) {
        commands[command_count++] = strdup(token);
        token = strtok_r(NULL, "||", &saveptr);
    }

    int success = 0; // Flag to track if any command succeeded
    char* fail_command;
    for (int i = 0; i < command_count; i++) {
        // Split by '&&' if needed
        char* sub_token;
        char* sub_saveptr;
        char* sub_commands[2]; // Array to hold up to 2 sub-commands
        int sub_command_count = 0;

        sub_token = strtok_r(commands[i], "&&", &sub_saveptr);
        while (sub_token != NULL && sub_command_count < 3) {
            sub_commands[sub_command_count++] = strdup(sub_token);
            sub_token = strtok_r(NULL, "&&", &sub_saveptr);
        }
        int sub_success = 1; // Flag to track if all sub-commands succeed
        if (ifdup == 1) {
            dup2(fd, STDERR_FILENO);
            close(fd); // Close fd after duplicating
        }
        for (int j = 0; j < sub_command_count; j++) {
            if (execute_command(sub_commands[j]) != 0) {
                sub_success = 0; // If any sub-command fails, set sub_success to 0
                fail_command = strdup(sub_commands[j]);
                perror("error: command not found.");
                break; // Break the loop if any '&&' group fails
            }
        }

        if (sub_success) {
            success = 1; // Mark overall success if any '&&' group succeeds
            break; // Break the loop if any '||' group succeeds
        }
    }

    // Free sub-commands memory
    for (int i = 0; i < command_count; i++) {
        free(commands[i]);
    }
}

void execute_script(char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error opening script file.\n");
        return;
    }

    char line[MAX_CMD_LENGTH];

    // Read the first line
    if (fgets(line, sizeof(line), file) == NULL) {
        printf("Script file is empty.\n");
        fclose(file);
        return;
    }

    // Check if the first line is a valid shebang
    if (strncmp(line, "#!/bin/bash", 11) != 0) {
        printf("Error, script must begin with #!/bin/bash.\n");
        fclose(file);
        return;
    }

    // Read and execute each subsequent line in the script
    while (fgets(line, sizeof(line), file) != NULL) {
        num_script_lines++;
        // Remove trailing newline character if present
        line[strcspn(line, "\n")] = '\0';
        execute_conditions(line);
    }

    fclose(file);
}

// Function to display all aliases with their shortcuts and commands
void display_aliases() {
    Alias* current = aliases_head;
    while (current != NULL) {
        printf("%s='%s'\n", current->shortcut, current->command);
        current = current->next;
    }
}


int main() {
    char command[MAX_CMD_LENGTH];

    while (1) {
        display_prompt();
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = '\0';

        if (strcmp(command, "exit_shell") == 0) {
            printf("%d\n", num_commands);
            break;
        }
        else if (strncmp(command, "alias", 5) == 0) {
            if (strlen(command) == 5) {
                display_aliases();
            }
            else {
                add_alias(command);
            }
        }
        else if (strncmp(command, "unalias ", 8) == 0) {
            remove_alias(command + 8);
        }
        else if (strncmp(command, "source ", 7) == 0) {
            execute_script(command + 7);
        }
        else if (strcmp(command, "jobs") == 0) {
            list_jobs();
        }
        else if (strstr(command, "&&") != NULL || strstr(command, "||") != NULL) {
            execute_conditions(command);
        }
        else {
            execute_command(command);
        }
    }

    // Clean up allocated memory for aliases
    Alias* current = aliases_head;
    while (current != NULL) {
        Alias* temp = current;
        current = current->next;
        free(temp->shortcut);
        free(temp->command);
        free(temp);
    }

    // Clean up allocated memory for jobs
    Job* current_job = jobs_head;
    while (current_job != NULL) {
        Job* temp = current_job;
        current_job = current_job->next;
        free(temp->command);
        free(temp);
    }
    return 0;
}