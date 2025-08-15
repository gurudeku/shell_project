#pragma once
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <termios.h>
#include <sys/types.h>
#include <mutex>
#include <memory>

struct Command {
    std::vector<std::string> argv;
    std::string in;
    std::string out;
    bool append_out{false};
};

struct Pipeline {
    std::vector<Command> cmds;
    bool background{false};
};

enum class JobStatus { Running, Stopped, Done };

struct Job {
    int id;
    pid_t pgid;
    std::string command;
    JobStatus status;
    bool background{false};
    std::vector<pid_t> pids;
};

class Logger;
class History;
class Parser;

class Shell {
public:
    Shell();
    ~Shell();
    int run(int argc, char** argv);

private:
    // core
    void init_shell();
    void load_rc();
    std::string prompt();
    std::string read_line();
    int execute_line(const std::string& line);
    int launch_pipeline(const Pipeline& pl);
    int launch_job(const Pipeline& pl, const std::string& printable);
    int wait_for_job(pid_t pgid);
    void update_prompt_jobs_hint();

    // builtins
    bool is_builtin(const Command& cmd) const;
    int run_builtin(const Command& cmd);
    int builtin_cd(const std::vector<std::string>& args);
    int builtin_pwd();
    int builtin_exit();
    int builtin_jobs();
    int builtin_fg(const std::vector<std::string>& args);
    int builtin_bg(const std::vector<std::string>& args);
    int builtin_kill(const std::vector<std::string>& args);
    int builtin_history();

    // jobs
    void add_job(const Job& job);
    void mark_job_status(pid_t pid, int status);
    void check_for_terminated_jobs();
    int next_job_id();
    Job* find_job_by_id(int id);
    Job* find_job_by_pgid(pid_t pgid);
    void set_foreground_pgid(pid_t pgid);
    void restore_shell_terminal();

    // signal handlers
    static void sigchld_handler(int);
    static void install_signal_handlers();

private:
    // shell state
    bool interactive{true};
    int shell_terminal{-1};
    pid_t shell_pgid{0};
    termios shell_tmodes{};

    // jobs
    mutable std::mutex jobs_mtx;
    std::map<int, Job> jobs;       // id -> Job
    std::map<pid_t, int> pgid_to_id;
    std::atomic<int> active_bg_jobs{0};

    // i/o + helpers
    std::unique_ptr<Logger> logger;
    std::unique_ptr<History> history;
    std::unique_ptr<Parser> parser;

    // prompt hint
    std::atomic<int> prompt_bg_hint{0};
};
