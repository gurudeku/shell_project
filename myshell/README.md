# MyShell — A Custom Multithreaded Unix Shell (C++)

MyShell is a Unix-like shell written in modern C++17 with **full job control** (foreground/background, signals), **pipes**, **redirection**, and **multithreading** for logging and job monitoring.  
It optionally supports **readline** for history and autocomplete; otherwise it falls back to standard input.


**Note:**  
The majority of the shell’s core logic, job control, and built-in command handling is implemented in **`src/shell.cpp`**.


## Features
- **Core shell**: `fork`, `execvp`, sequential execution  
- **Job control**:  
  - `&` background  
  - `SIGINT` / `SIGTSTP` forwarding to foreground  
  - `SIGCHLD` reaping  
- **Built-ins**: `cd`, `pwd`, `exit`, `jobs`, `fg`, `bg`, `kill`, `history`  
- **Redirection**: `<`, `>`, `>>`  
- **Pipes**: `cmd1 | cmd2 | cmd3`  
- **Multithreading**:  
  - Logging thread (async file logging)  
  - Job monitor thread (status + prompt info)  
- **History**:  
  - With readline: persistent history at `~/.myshell_history`  
  - Without readline: internal history; `history` prints last commands  
- **Scripting**:  
  - Runs `~/.myshellrc` at startup (if present)  
  - Can execute a script file passed as first CLI arg  


## Build
Requires Linux with `g++` (C++17) and pthreads. Readline is optional.

```bash
# Build without readline
make

# OR build with readline (if libreadline-dev installed)
make READLINE=1
