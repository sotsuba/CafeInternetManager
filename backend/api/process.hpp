#pragma once 

#include <signal.h>

#include <string>
#include <vector>
#include <ostream>

struct ProcessInfo {
    int pid;
    int ppid;
    std::string name;
    std::string cmd;
};


class Process {
public:
    explicit Process(int pid);
    Process(const ProcessInfo& info): info_(info) {}; 
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    ProcessInfo get_info()  const { return info_; }
    int destroy()   { return ::kill(info_.pid, SIGKILL); }
    int suspend()    { return ::kill(info_.pid, SIGSTOP); }
    int resume()     { return ::kill(info_.pid, SIGCONT); }
    bool refresh();

    static std::vector<ProcessInfo> get_all();
    static void print_all(std::ostream& output);
private:
    ProcessInfo info_;
};
