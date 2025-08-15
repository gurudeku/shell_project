#include "shell.hpp"
#include "parser.hpp"
#include "logger.hpp"
#include "history.hpp"
#include "util.hpp"
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <csignal>
#include <termios.h>
#include <pwd.h>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <filesystem>
#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#include "completion.hpp"
#endif

static Shell* g_shell = nullptr;

static std::string home_dir(){
    const char* h = std::getenv("HOME");
    if(!h){
        struct passwd* pw = getpwuid(getuid());
        if(pw) h = pw->pw_dir;
    }
    return h ? std::string(h) : std::string(".");
}

Shell::Shell(){
    g_shell = this;
    parser = std::make_unique<Parser>();
    logger = std::make_unique<Logger>(home_dir() + "/.myshell.log");
    history = std::make_unique<History>();
    history->load();
}

Shell::~Shell(){
    restore_shell_terminal();
#ifdef HAVE_READLINE
    // readline saves history automatically via write_history if configured; we keep simple
#endif
    history->save();
}

void Shell::install_signal_handlers(){
    struct sigaction sa{};
    sa.sa_handler = Shell::sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP | SA_NOCLDWAIT;
    sigaction(SIGCHLD, &sa, nullptr);

    // Ignore signals in shell; foreground process group will get them
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
}

int Shell::run(int argc, char** argv){
    init_shell();
    install_signal_handlers();
    load_rc();

    // background job monitor
    std::thread monitor([&]{
        while(true){
            std::this_thread::sleep_for(std::chrono::seconds(1));
            check_for_terminated_jobs();
            update_prompt_jobs_hint();
        }
    });
    monitor.detach();

    if(argc > 1){
        // script mode
        std::ifstream ifs(argv[1]);
        if(!ifs){
            std::cerr << "myshell: cannot open script: " << argv[1] << "\n";
            return 1;
        }
        std::string line;
        while(std::getline(ifs, line)){
            line = trim(line);
            if(line.empty() || line[0]=='#') continue;
            execute_line(line);
        }
        return 0;
    }

    while(true){
        std::string line = read_line();
        if(line.empty()) continue;
        execute_line(line);
    }
    return 0;
}

void Shell::init_shell(){
    shell_terminal = STDIN_FILENO;
    interactive = isatty(shell_terminal);

    if(interactive){
        // put shell in its own process group
        while(tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(- shell_pgid, SIGTTIN);

        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);

        shell_pgid = getpid();
        if(setpgid(shell_pgid, shell_pgid) < 0){
            perror("setpgid");
            exit(1);
        }
        if(tcsetpgrp(shell_terminal, shell_pgid) < 0){
            perror("tcsetpgrp");
            exit(1);
        }
        tcgetattr(shell_terminal, &shell_tmodes);
    }
}

void Shell::load_rc(){
    std::string rc = home_dir() + "/.myshellrc";
    std::ifstream ifs(rc);
    if(!ifs) return;
    std::string line;
    while(std::getline(ifs, line)){
        line = trim(line);
        if(line.empty() || line[0]=='#') continue;
        execute_line(line);
    }
}

std::string Shell::prompt(){
    char cwd[4096]; getcwd(cwd, sizeof(cwd));
    std::ostringstream oss;
    oss << "myshell";
    if(prompt_bg_hint.load() > 0) oss << "[jobs:" << prompt_bg_hint.load() << "]";
    oss << ":" << cwd << "$ ";
    return oss.str();
}

std::string Shell::read_line(){
#ifdef HAVE_READLINE
    rl_attempted_completion_function = myshell_completion;
    char* p = readline(prompt().c_str());
    if(!p) { std::cout << "\n"; exit(0); }
    std::string line = p;
    free(p);
    if(!line.empty()) add_history(line.c_str());
#else
    std::cout << prompt();
    std::string line;
    std::getline(std::cin, line);
    if(!std::cin) { std::cout << "\n"; exit(0); }
#endif
    line = trim(line);
    if(!line.empty()) history->add(line), logger->log(line);
    return line;
}

int Shell::execute_line(const std::string& line){
    auto pl = parser->parse(line);
    if(pl.cmds.empty()) return 0;

    // if single command and builtin
    if(pl.cmds.size()==1 && is_builtin(pl.cmds[0])){
        return run_builtin(pl.cmds[0]);
    }
    return launch_pipeline(pl);
}

bool Shell::is_builtin(const Command& cmd) const{
    if(cmd.argv.empty()) return false;
    static const std::vector<std::string> b = {"cd","pwd","exit","jobs","fg","bg","kill","history"};
    for(const auto& s: b) if(cmd.argv[0]==s) return true;
    return false;
}

int Shell::run_builtin(const Command& cmd){
    const auto& a = cmd.argv;
    if(a[0]=="cd") return builtin_cd(a);
    if(a[0]=="pwd") return builtin_pwd();
    if(a[0]=="exit") return builtin_exit();
    if(a[0]=="jobs") return builtin_jobs();
    if(a[0]=="fg") return builtin_fg(a);
    if(a[0]=="bg") return builtin_bg(a);
    if(a[0]=="kill") return builtin_kill(a);
    if(a[0]=="history") return builtin_history();
    return 0;
}

int Shell::builtin_cd(const std::vector<std::string>& args){
    const char* path = args.size() > 1 ? args[1].c_str() : home_dir().c_str();
    if(chdir(path) != 0) perror("cd");
    return 0;
}
int Shell::builtin_pwd(){
    char cwd[4096]; if(getcwd(cwd, sizeof(cwd))) std::cout << cwd << "\n";
    return 0;
}
int Shell::builtin_exit(){
    std::cout << "Bye!\n"; exit(0);
}
int Shell::builtin_jobs(){
    std::lock_guard<std::mutex> lk(jobs_mtx);
    for(auto& [id, job] : jobs){
        std::string st = (job.status==JobStatus::Running?"Running": job.status==JobStatus::Stopped?"Stopped":"Done");
        std::cout << "["<<id<<"] " << (int)job.pgid << " " << st << "  " << job.command << (job.background?" &":"") << "\n";
    }
    return 0;
}
int Shell::builtin_fg(const std::vector<std::string>& args){
    if(args.size()<2){ std::cerr << "fg: usage: fg %jobid\n"; return 1; }
    int id = std::stoi(args[1][0]=='%'? args[1].substr(1):args[1]);
    Job* j = find_job_by_id(id);
    if(!j){ std::cerr << "fg: no such job\n"; return 1; }
    set_foreground_pgid(j->pgid);
    if(j->status == JobStatus::Stopped){
        kill(-j->pgid, SIGCONT);
        j->status = JobStatus::Running;
    }
    int st = wait_for_job(j->pgid);
    restore_shell_terminal();
    return st;
}
int Shell::builtin_bg(const std::vector<std::string>& args){
    if(args.size()<2){ std::cerr << "bg: usage: bg %jobid\n"; return 1; }
    int id = std::stoi(args[1][0]=='%'? args[1].substr(1):args[1]);
    Job* j = find_job_by_id(id);
    if(!j){ std::cerr << "bg: no such job\n"; return 1; }
    if(j->status != JobStatus::Running){
        kill(-j->pgid, SIGCONT);
        j->status = JobStatus::Running;
    }
    j->background = true;
    return 0;
}
int Shell::builtin_kill(const std::vector<std::string>& args){
    if(args.size()<2){ std::cerr << "kill: usage: kill %jobid|pgid\n"; return 1; }
    if(args[1][0]=='%'){
        int id = std::stoi(args[1].substr(1));
        Job* j = find_job_by_id(id);
        if(!j){ std::cerr << "kill: no such job\n"; return 1; }
        if(::kill(-j->pgid, SIGTERM)!=0) perror("kill");
    }else{
        pid_t pg = (pid_t)std::stol(args[1]);
        if(::kill(-pg, SIGTERM)!=0) perror("kill");
    }
    return 0;
}
int Shell::builtin_history(){
    history->print();
    return 0;
}

int Shell::launch_pipeline(const Pipeline& pl){
    // Build printable command
    std::vector<std::string> parts;
    for(const auto& c: pl.cmds){
        parts.push_back(join(c.argv, " "));
    }
    std::string printable = join(parts, " | ");
    if(pl.background) printable += " &";
    return launch_job(pl, printable);
}

int Shell::launch_job(const Pipeline& pl, const std::string& printable){
    size_t n = pl.cmds.size();
    std::vector<int> pipes;
    pipes.resize((n>1)? 2*(n-1): 0);
    for(size_t i=0;i+1<n;++i){
        if(pipe(&pipes[2*i])<0){ perror("pipe"); return 1; }
    }

    pid_t pgid = 0;
    std::vector<pid_t> pids;

    for(size_t i=0;i<n;++i){
        pid_t pid = fork();
        if(pid==0){
            // Child
            // New process group
            if(pgid == 0) pgid = getpid();
            setpgid(getpid(), pgid);
            if(!pl.background) tcsetpgrp(STDIN_FILENO, pgid);

            // Restore default signals
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);

            // Setup pipes
            if(n>1){
                if(i>0){ // read from previous
                    dup2(pipes[2*(i-1)], STDIN_FILENO);
                }
                if(i+1<n){ // write to next
                    dup2(pipes[2*i+1], STDOUT_FILENO);
                }
                // close all pipe fds
                for(size_t k=0;k<pipes.size();++k) close(pipes[k]);
            }

            // redirections
            const auto& cmd = pl.cmds[i];
            if(!cmd.in.empty()){
                int fd = open(cmd.in.c_str(), O_RDONLY);
                if(fd<0){ perror("open <"); _exit(1); }
                dup2(fd, STDIN_FILENO); close(fd);
            }
            if(!cmd.out.empty()){
                int flags = O_WRONLY|O_CREAT|(cmd.append_out? O_APPEND: O_TRUNC);
                int fd = open(cmd.out.c_str(), flags, 0644);
                if(fd<0){ perror("open >"); _exit(1); }
                dup2(fd, STDOUT_FILENO); close(fd);
            }

            // exec
            std::vector<char*> argv;
            for(const auto& s: cmd.argv) argv.push_back(const_cast<char*>(s.c_str()));
            argv.push_back(nullptr);
            execvp(argv[0], argv.data());
            perror("execvp");
            _exit(127);
        }else if(pid>0){
            // Parent
            if(pgid==0) pgid = pid;
            setpgid(pid, pgid);
            pids.push_back(pid);
        }else{
            perror("fork");
            return 1;
        }
    }

    // parent closes pipes
    for(size_t k=0;k<pipes.size();++k) close(pipes[k]);

    // register job
    Job job;
    job.id = next_job_id();
    job.pgid = pgid;
    job.command = printable;
    job.status = JobStatus::Running;
    job.background = pl.background;
    job.pids = pids;
    add_job(job);

    if(pl.background){
        active_bg_jobs.fetch_add(1);
        std::cout << "["<<job.id<<"] "<< pgid << " " << printable << "\n";
        return 0;
    }else{
        set_foreground_pgid(pgid);
        int st = wait_for_job(pgid);
        restore_shell_terminal();
        return st;
    }
}

int Shell::wait_for_job(pid_t pgid){
    int status = 0;
    pid_t pid;
    do{
        pid = waitpid(-pgid, &status, WUNTRACED);
    }while(pid>0 && !WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));
    // update status
    if(WIFSTOPPED(status)){
        std::lock_guard<std::mutex> lk(jobs_mtx);
        auto it = pgid_to_id.find(pgid);
        if(it!=pgid_to_id.end()){
            jobs[it->second].status = JobStatus::Stopped;
            jobs[it->second].background = false;
            active_bg_jobs.fetch_add(1);
        }
    }else{
        std::lock_guard<std::mutex> lk(jobs_mtx);
        auto it = pgid_to_id.find(pgid);
        if(it!=pgid_to_id.end()){
            jobs[it->second].status = JobStatus::Done;
            if(jobs[it->second].background) active_bg_jobs.fetch_sub(1);
        }
    }
    return status;
}

void Shell::add_job(const Job& job){
    std::lock_guard<std::mutex> lk(jobs_mtx);
    jobs[job.id] = job;
    pgid_to_id[job.pgid] = job.id;
}

int Shell::next_job_id(){
    static int cur=1;
    return cur++;
}

Job* Shell::find_job_by_id(int id){
    std::lock_guard<std::mutex> lk(jobs_mtx);
    auto it = jobs.find(id);
    return it==jobs.end()? nullptr : &it->second;
}

Job* Shell::find_job_by_pgid(pid_t pgid){
    std::lock_guard<std::mutex> lk(jobs_mtx);
    auto it = pgid_to_id.find(pgid);
    if(it==pgid_to_id.end()) return nullptr;
    auto it2 = jobs.find(it->second);
    return it2==jobs.end()? nullptr : &it2->second;
}

void Shell::set_foreground_pgid(pid_t pgid){
    tcsetpgrp(shell_terminal, pgid);
}

void Shell::restore_shell_terminal(){
    if(interactive) tcsetpgrp(shell_terminal, shell_pgid);
}

void Shell::sigchld_handler(int){
    // Reap without blocking
    int status;
    pid_t pid;
    while((pid = waitpid(-1, &status, WNOHANG|WUNTRACED|WCONTINUED)) > 0){
        if(WIFEXITED(status) || WIFSIGNALED(status)){
            // mark as done handled in check_for_terminated_jobs
        }
    }
}

void Shell::check_for_terminated_jobs(){
    std::lock_guard<std::mutex> lk(jobs_mtx);
    std::vector<int> to_erase;
    for(auto& [id, job] : jobs){
        // Check any process alive in group
        bool alive = false;
        for(pid_t p : job.pids){
            if(kill(p, 0) == 0){ alive = true; break; }
        }
        if(!alive && job.status != JobStatus::Done){
            job.status = JobStatus::Done;
            if(job.background) active_bg_jobs.fetch_sub(1);
        }
        if(job.status == JobStatus::Done){
            to_erase.push_back(id);
        }
    }
    for(int id: to_erase){
        jobs.erase(id);
    }
}

void Shell::update_prompt_jobs_hint(){
    prompt_bg_hint.store(std::max(0, active_bg_jobs.load()));
}
