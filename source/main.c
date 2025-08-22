#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <time.h>

#define MAX_LINE_LENGTH 1024
#define MAX_VARS 256
#define MAX_PROGRAMS 100
#define MAX_PROGRAM_SIZE 10000
#define MAX_VAR_NAME 32
#define MAX_FILENAME 256

// Token types
typedef enum {
    TOK_DOLLAR,
    TOK_IDENTIFIER,
    TOK_NUMBER,
    TOK_COMMA,
    TOK_EOF,
    TOK_UNKNOWN
} TokenType;

// Token structure
typedef struct {
    TokenType type;
    char value[MAX_LINE_LENGTH];
    int number;
} Token;

// Variable structure
typedef struct {
    char name[MAX_VAR_NAME];
    int value;
    int used;
} Variable;

// Program line structure
typedef struct {
    int line_number;
    char content[MAX_LINE_LENGTH];
} ProgramLine;

// MCL System state
typedef struct {
    Variable variables[MAX_VARS];
    int var_count;
    ProgramLine program[MAX_PROGRAMS];
    int program_size;
    char current_program[MAX_FILENAME];
    int program_loaded;
} MCLSystem;

// Global system state
MCLSystem mcl_system = {0};

// Lexer functions
void skip_whitespace(char **input) {
    while (**input && isspace(**input)) {
        (*input)++;
    }
}

Token get_next_token(char **input) {
    Token token = {TOK_UNKNOWN, "", 0};
    skip_whitespace(input);
    
    if (**input == '\0') {
        token.type = TOK_EOF;
        return token;
    }
    
    if (**input == '$') {
        token.type = TOK_DOLLAR;
        strcpy(token.value, "$");
        (*input)++;
        return token;
    }
    
    if (**input == ',') {
        token.type = TOK_COMMA;
        strcpy(token.value, ",");
        (*input)++;
        return token;
    }
    
    if (isdigit(**input)) {
        token.type = TOK_NUMBER;
        int i = 0;
        while (isdigit(**input)) {
            token.value[i++] = **input;
            (*input)++;
        }
        token.value[i] = '\0';
        token.number = atoi(token.value);
        return token;
    }
    
    if (isalpha(**input) || **input == '$') {
        token.type = TOK_IDENTIFIER;
        int i = 0;
        while (isalnum(**input) || **input == '$' || **input == '_') {
            token.value[i++] = **input;
            (*input)++;
        }
        token.value[i] = '\0';
        return token;
    }
    
    // Unknown character, advance and return
    token.value[0] = **input;
    token.value[1] = '\0';
    (*input)++;
    return token;
}

// Variable management
Variable* find_variable(const char* name) {
    for (int i = 0; i < mcl_system.var_count; i++) {
        if (strcmp(mcl_system.variables[i].name, name) == 0) {
            return &mcl_system.variables[i];
        }
    }
    return NULL;
}

Variable* create_variable(const char* name) {
    if (mcl_system.var_count >= MAX_VARS) {
        printf("Error: Maximum variables exceeded\n");
        return NULL;
    }
    
    Variable* var = &mcl_system.variables[mcl_system.var_count];
    strcpy(var->name, name);
    var->value = 0;
    var->used = 1;
    mcl_system.var_count++;
    return var;
}

// Expression evaluation
int evaluate_expression(char **input) {
    skip_whitespace(input);
    
    if (**input == '$') {
        (*input)++; // Skip $
        char var_name[MAX_VAR_NAME];
        int i = 0;
        
        while (isalnum(**input) || **input == '_') {
            var_name[i++] = **input;
            (*input)++;
        }
        var_name[i] = '\0';
        
        Variable* var = find_variable(var_name);
        if (var) {
            return var->value;
        } else {
            printf("Error: Variable $%s not found\n", var_name);
            return 0;
        }
    } else if (isdigit(**input)) {
        int value = 0;
        while (isdigit(**input)) {
            value = value * 10 + (**input - '0');
            (*input)++;
        }
        return value;
    }
    
    return 0;
}

// Command implementations
void cmd_sum(char *args) {
    char *ptr = args;
    int val1 = evaluate_expression(&ptr);
    
    skip_whitespace(&ptr);
    if (*ptr == ',') ptr++; // Skip comma
    
    int val2 = evaluate_expression(&ptr);
    printf("%d\n", val1 + val2);
}

void cmd_syb(char *args) {
    char *ptr = args;
    int val1 = evaluate_expression(&ptr);
    
    skip_whitespace(&ptr);
    if (*ptr == ',') ptr++; // Skip comma
    
    int val2 = evaluate_expression(&ptr);
    printf("%d\n", val1 - val2);
}

void cmd_ini(char *args) {
    char *ptr = args;
    skip_whitespace(&ptr);
    
    if (*ptr != '$') {
        printf("Error: Variable name must start with $\n");
        return;
    }
    ptr++; // Skip $
    
    char var_name[MAX_VAR_NAME];
    int i = 0;
    while (isalnum(*ptr) || *ptr == '_') {
        var_name[i++] = *ptr++;
    }
    var_name[i] = '\0';
    
    skip_whitespace(&ptr);
    int value = 0;
    if (*ptr) {
        value = evaluate_expression(&ptr);
    }
    
    Variable* var = find_variable(var_name);
    if (!var) {
        var = create_variable(var_name);
    }
    
    if (var) {
        var->value = value;
        printf("Variable $%s initialized to %d\n", var_name, value);
    }
}

void cmd_list(char *args) {
    char *ptr = args;
    skip_whitespace(&ptr);
    
    // Parse first parameter
    int first_param = -1;
    int dollar_count = 0;
    char special_param[32] = "";
    
    // Count $ symbols
    char *temp = ptr;
    while (*temp == '$') {
        dollar_count++;
        temp++;
    }
    
    if (dollar_count > 0) {
        ptr = temp; // Skip the $ symbols
        
        // Check for special parameters
        if (strncmp(ptr, "INF", 3) == 0) {
            strcpy(special_param, "INF");
            ptr += 3;
        } else if (*ptr == ',') {
            // Just $ followed by comma
        } else if (isdigit(*ptr)) {
            first_param = evaluate_expression(&ptr);
        } else if (*ptr == 'T') {
            strcpy(special_param, "T");
            ptr++;
        }
    } else if (isdigit(*ptr)) {
        first_param = evaluate_expression(&ptr);
    }
    
    skip_whitespace(&ptr);
    if (*ptr == ',') ptr++; // Skip comma
    
    // Parse second parameter
    int second_param = -1;
    skip_whitespace(&ptr);
    if (*ptr) {
        second_param = evaluate_expression(&ptr);
    }
    
    // Execute LIST command based on parameters
    if (strcmp(special_param, "INF") == 0) {
        // LIST $INF - Show system info
        struct utsname sys_info;
        time_t current_time;
        time(&current_time);
        
        if (uname(&sys_info) == 0) {
            printf("MCL System Information:\n");
            printf("System: %s %s\n", sys_info.sysname, sys_info.release);
            printf("Machine: %s\n", sys_info.machine);
            printf("Node: %s\n", sys_info.nodename);
            printf("Time: %s", ctime(&current_time));
            printf("Variables: %d/%d\n", mcl_system.var_count, MAX_VARS);
            printf("Program loaded: %s\n", mcl_system.program_loaded ? "Yes" : "No");
            if (mcl_system.program_loaded) {
                printf("Program: %s (%d lines)\n", mcl_system.current_program, mcl_system.program_size);
            }
        }
    } else if (strcmp(special_param, "T") == 0) {
        // LIST $$,T - List variables
        printf("Variables:\n");
        for (int i = 0; i < mcl_system.var_count; i++) {
            printf("$%s = %d\n", mcl_system.variables[i].name, mcl_system.variables[i].value);
        }
    } else if (dollar_count == 2 && second_param == 0) {
        // LIST $$,0 - List lines (like BASIC list)
        printf("Program lines:\n");
        for (int i = 0; i < mcl_system.program_size; i++) {
            printf("%d %s\n", mcl_system.program[i].line_number, mcl_system.program[i].content);
        }
    } else if (dollar_count == 2 && second_param == -1) {
        // LIST $$ - List programs/scripts
        DIR *dir = opendir(".");
        if (dir) {
            struct dirent *entry;
            printf("Programs/Scripts:\n");
            while ((entry = readdir(dir)) != NULL) {
                if (strstr(entry->d_name, ".mcl") || strstr(entry->d_name, ".txt")) {
                    printf("%s\n", entry->d_name);
                }
            }
            closedir(dir);
        } else {
            printf("Error: Cannot access current directory\n");
        }
    } else if (first_param == 1 && dollar_count == 1) {
        // LIST 1,$ - List directories
        DIR *dir = opendir(".");
        if (dir) {
            struct dirent *entry;
            struct stat file_stat;
            printf("Directories:\n");
            while ((entry = readdir(dir)) != NULL) {
                if (stat(entry->d_name, &file_stat) == 0 && S_ISDIR(file_stat.st_mode)) {
                    if (entry->d_name[0] != '.') {
                        printf("%s/\n", entry->d_name);
                    }
                }
            }
            closedir(dir);
        } else {
            printf("Error: Cannot access current directory\n");
        }
    } else if (dollar_count == 1 && first_param == 1) {
        // LIST $,1 - List disks (simplified as mount points)
        FILE *mounts = fopen("/proc/mounts", "r");
        if (mounts) {
            char line[256];
            printf("Mounted filesystems:\n");
            while (fgets(line, sizeof(line), mounts)) {
                char device[128], mount_point[128], fs_type[32];
                if (sscanf(line, "%s %s %s", device, mount_point, fs_type) == 3) {
                    if (mount_point[0] == '/' && strlen(mount_point) < 20) {
                        printf("%s on %s (%s)\n", device, mount_point, fs_type);
                    }
                }
            }
            fclose(mounts);
        } else {
            printf("Error: Cannot access mount information\n");
        }
    } else if (dollar_count == 1 && second_param == 2) {
        // LIST $,2 - List $LIBs (libraries)
        printf("Available libraries:\n");
        printf("STDIO.LIB - Standard I/O functions\n");
        printf("MATH.LIB - Mathematical functions\n");
        printf("STRING.LIB - String manipulation\n");
        printf("FILE.LIB - File operations\n");
    } else {
        printf("LIST command format:\n");
        printf("LIST 1,$     - List directories\n");
        printf("LIST $,1     - List disks/mounts\n");
        printf("LIST $$,0    - List program lines\n");
        printf("LIST $,2     - List libraries\n");
        printf("LIST $INF    - Show system info\n");
        printf("LIST $$,T    - List variables\n");
        printf("LIST $$      - List programs\n");
    }
}

void cmd_load(char *args) {
    char *ptr = args;
    skip_whitespace(&ptr);
    
    // Count $ symbols at start
    int dollar_count = 0;
    while (*ptr == '$') {
        dollar_count++;
        ptr++;
    }
    
    if (dollar_count == 2) {
        // LD $$,<disk name> - Load from disk (simplified)
        skip_whitespace(&ptr);
        if (*ptr == ',') ptr++;
        skip_whitespace(&ptr);
        
        printf("Loading from disk: %s\n", ptr);
        printf("Error: Disk loading not implemented in this version\n");
    } else {
        // LD 0,$<filename> - Load a program
        int first_param = evaluate_expression(&ptr);
        skip_whitespace(&ptr);
        if (*ptr == ',') ptr++;
        skip_whitespace(&ptr);
        
        if (*ptr == '$') ptr++; // Skip optional $
        
        FILE *file = fopen(ptr, "r");
        if (file) {
            mcl_system.program_size = 0;
            strcpy(mcl_system.current_program, ptr);
            
            char line[MAX_LINE_LENGTH];
            int line_num = 10;
            
            while (fgets(line, sizeof(line), file) && mcl_system.program_size < MAX_PROGRAMS) {
                // Remove newline
                line[strcspn(line, "\n")] = 0;
                
                mcl_system.program[mcl_system.program_size].line_number = line_num;
                strcpy(mcl_system.program[mcl_system.program_size].content, line);
                mcl_system.program_size++;
                line_num += 10;
            }
            
            fclose(file);
            mcl_system.program_loaded = 1;
            printf("Program loaded: %s (%d lines)\n", ptr, mcl_system.program_size);
        } else {
            printf("Error: Cannot load file '%s'\n", ptr);
        }
    }
}

void cmd_save(char *args) {
    char *ptr = args;
    skip_whitespace(&ptr);
    
    char filename[MAX_FILENAME];
    if (*ptr) {
        strcpy(filename, ptr);
    } else {
        strcpy(filename, "mcl_state.sav");
    }
    
    FILE *file = fopen(filename, "w");
    if (file) {
        // Save variables
        fprintf(file, "# MCL State File\n");
        fprintf(file, "# Variables\n");
        for (int i = 0; i < mcl_system.var_count; i++) {
            fprintf(file, "INI $%s %d\n", mcl_system.variables[i].name, mcl_system.variables[i].value);
        }
        
        // Save current program if loaded
        if (mcl_system.program_loaded) {
            fprintf(file, "# Program: %s\n", mcl_system.current_program);
            for (int i = 0; i < mcl_system.program_size; i++) {
                fprintf(file, "# %d %s\n", mcl_system.program[i].line_number, mcl_system.program[i].content);
            }
        }
        
        fclose(file);
        printf("State saved to: %s\n", filename);
    } else {
        printf("Error: Cannot save to file '%s'\n", filename);
    }
}

void cmd_edit(char *args) {
    char *ptr = args;
    skip_whitespace(&ptr);
    
    if (*ptr != '$') {
        printf("Error: ED requires $NUM<line number>\n");
        return;
    }
    ptr++; // Skip $
    
    if (strncmp(ptr, "NUM", 3) != 0) {
        printf("Error: ED requires $NUM<line number>\n");
        return;
    }
    ptr += 3; // Skip NUM
    
    int line_num = evaluate_expression(&ptr);
    
    // Find the line in the program
    for (int i = 0; i < mcl_system.program_size; i++) {
        if (mcl_system.program[i].line_number == line_num) {
            printf("%d %s\n", mcl_system.program[i].line_number, mcl_system.program[i].content);
            return;
        }
    }
    
    printf("Line %d not found\n", line_num);
}

void cmd_run(char *args) {
    if (!mcl_system.program_loaded) {
        printf("Error: No program loaded\n");
        return;
    }
    
    printf("Running program: %s\n", mcl_system.current_program);
    
    for (int i = 0; i < mcl_system.program_size; i++) {
        printf("Executing line %d: %s\n", mcl_system.program[i].line_number, mcl_system.program[i].content);
        
        // Simple interpretation - treat each line as an MCL command
        char line_copy[MAX_LINE_LENGTH];
        strcpy(line_copy, mcl_system.program[i].content);
        
        char *cmd_ptr = line_copy;
        skip_whitespace(&cmd_ptr);
        
        if (strncmp(cmd_ptr, "PRINT", 5) == 0) {
            cmd_ptr += 5;
            skip_whitespace(&cmd_ptr);
            printf("OUTPUT: %s\n", cmd_ptr);
        } else if (strncmp(cmd_ptr, "INI", 3) == 0) {
            cmd_ini(cmd_ptr + 3);
        } else {
            printf("Unknown program command: %s\n", mcl_system.program[i].content);
        }
    }
    
    printf("Program execution completed\n");
}

// Main command processor
void process_command(char *input) {
    char *ptr = input;
    skip_whitespace(&ptr);
    
    if (strncmp(ptr, "SUM", 3) == 0) {
        cmd_sum(ptr + 3);
    } else if (strncmp(ptr, "SYB", 3) == 0) {
        cmd_syb(ptr + 3);
    } else if (strncmp(ptr, "INI", 3) == 0) {
        cmd_ini(ptr + 3);
    } else if (strncmp(ptr, "LIST", 4) == 0) {
        cmd_list(ptr + 4);
    } else if (strncmp(ptr, "LD", 2) == 0) {
        cmd_load(ptr + 2);
    } else if (strncmp(ptr, "SAVE", 4) == 0) {
        cmd_save(ptr + 4);
    } else if (strncmp(ptr, "ED", 2) == 0) {
        cmd_edit(ptr + 2);
    } else if (strncmp(ptr, "$RUN", 4) == 0) {
        cmd_run(ptr + 4);
    } else if (strncmp(ptr, "HELP", 4) == 0 || strncmp(ptr, "?", 1) == 0) {
        printf("MCL Commands:\n");
        printf("SUM <expr>,<expr>     - Add two values\n");
        printf("SYB <expr>,<expr>     - Subtract two values\n");
        printf("INI $<var> [value]    - Initialize variable\n");
        printf("LIST 1,$              - List directories\n");
        printf("LIST $,1              - List disks/mounts\n");
        printf("LIST $$,0             - List program lines\n");
        printf("LIST $,2              - List libraries\n");
        printf("LIST $INF             - Show system info\n");
        printf("LIST $$,T             - List variables\n");
        printf("LIST $$               - List programs\n");
        printf("LD 0,$<filename>      - Load program\n");
        printf("SAVE [filename]       - Save state\n");
        printf("ED $NUM<line>         - Edit/show line\n");
        printf("$RUN                  - Run loaded program\n");
        printf("EXIT                  - Exit MCL\n");
    } else if (strncmp(ptr, "EXIT", 4) == 0 || strncmp(ptr, "QUIT", 4) == 0) {
        printf("Goodbye from MCL!\n");
        exit(0);
    } else if (*ptr == '\0') {
        // Empty line, do nothing
    } else {
        printf("Unknown command: %s\n", ptr);
        printf("Type HELP for available commands\n");
    }
}

int main() {
    char input[MAX_LINE_LENGTH];
    
    printf("MCL - Miguel's Command Language\n");
    printf("Based on DCL (DIGITAL Command Language)\n");
    printf("Type HELP for available commands, EXIT to quit\n\n");
    
    while (1) {
        printf("MCL> ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // Remove newline
        input[strcspn(input, "\n")] = 0;
        
        // Convert to uppercase for command recognition
        char *ptr = input;
        while (*ptr) {
            if (isalpha(*ptr) && *ptr != '$') {
                *ptr = toupper(*ptr);
            }
            ptr++;
        }
        
        process_command(input);
    }
    
    return 0;
}