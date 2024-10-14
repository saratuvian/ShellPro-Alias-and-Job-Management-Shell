# Custom Shell

This project implements a custom shell in C, with support for basic commands, aliases, background jobs, and script execution.

## Features

### 1. Command Execution
- The shell supports basic command execution.
- It can execute commands with arguments and handle redirection (`2>` for stderr).
- Background jobs can be run by appending `&` at the end of the command.

### 2. Aliases
- You can create shortcuts for commands using aliases.
- The `alias` command allows the creation of new aliases in the form of `alias shortcut='command'`.
- The `unalias` command removes an existing alias.

### 3. Background Jobs
- The shell can run processes in the background.
- Use `&` at the end of a command to run it in the background.
- List background jobs using the `jobs` command.

### 4. Script Execution
- The shell supports executing scripts with the `source` command.
- The script must start with `#!/bin/bash` to be valid.
- The shell will execute each line of the script sequentially.

### 5. Conditional Execution
- Commands can be executed conditionally using `&&` (and) or `||` (or).
- Example: `command1 && command2` will execute `command2` only if `command1` succeeds.
- Example: `command1 || command2` will execute `command2` only if `command1` fails.

### 6. Echo with Quotes
- The `echo` command supports printing text, including handling double-quoted strings.

## Usage

### Running the Shell
Simply compile and run the program using the following commands:

```bash
gcc shell.c -o shell
./shell
