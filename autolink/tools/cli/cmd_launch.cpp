/******************************************************************************
 * Copyright 2025 The Openbot Authors (duyongquan)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include <fcntl.h>
#include <sys/wait.h>
#include <tinyxml2.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace launch_cli {

const char* g_binary_name = "mainboard";
const int g_default_respawn_limit = 3;
const int force_stop_timeout_secs = 3;

std::vector<std::string> SplitLaunchSearchPath(const std::string& raw) {
    std::vector<std::string> dirs;
    if (raw.empty()) {
        return dirs;
    }
    size_t start = 0;
    while (start <= raw.size()) {
        const size_t pos = raw.find(':', start);
        const std::string part =
            (pos == std::string::npos) ? raw.substr(start)
                                      : raw.substr(start, pos - start);
        if (!part.empty()) {
            dirs.push_back(part);
        }
        if (pos == std::string::npos) {
            break;
        }
        start = pos + 1;
    }
    return dirs;
}

std::string GetLaunchPathEnv() {
    const char* p = std::getenv("AUTOLINK_LAUNCH_PATH");
    return p ? p : "/autolink";
}

/** Resolve a launch file name against AUTOLINK_LAUNCH_PATH (':' separated). */
std::string ResolveLaunchFile(const std::string& launch_file) {
    if (launch_file.empty()) {
        return ResolveLaunchFile("autolink.launch");
    }
    if (launch_file[0] == '/' && access(launch_file.c_str(), R_OK) == 0) {
        return launch_file;
    }
    // Relative path that already exists from cwd.
    if (access(launch_file.c_str(), R_OK) == 0) {
        return launch_file;
    }
    for (const std::string& dir : SplitLaunchSearchPath(GetLaunchPathEnv())) {
        const std::string full = dir + "/" + launch_file;
        if (access(full.c_str(), R_OK) == 0) {
            return full;
        }
    }
    return "";
}

std::string GetLogDir() {
    const char* p = std::getenv("APOLLO_ENV_WORKROOT");
    std::string root = p ? p : "/apollo";
    return root + "/data/log";
}

// ---------------------------------------------------------------------------
// XML helpers (match Python get_param_value / get_param_list)
// ---------------------------------------------------------------------------
std::string GetParamValue(const tinyxml2::XMLElement* module, const char* key,
                          const std::string& default_value = "") {
    const tinyxml2::XMLElement* el = module->FirstChildElement(key);
    if (!el || !el->GetText())
        return default_value;
    const char* t = el->GetText();
    std::string s(t);
    // strip whitespace
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
        return default_value;
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(
        start, end == std::string::npos ? std::string::npos : end - start + 1);
}

std::vector<std::string> GetParamList(const tinyxml2::XMLElement* module,
                                      const char* key) {
    std::vector<std::string> list;
    for (const tinyxml2::XMLElement* el = module->FirstChildElement(key); el;
         el = el->NextSiblingElement(key)) {
        if (!el->GetText())
            continue;
        std::string s(el->GetText());
        size_t start = s.find_first_not_of(" \t\n\r");
        if (start == std::string::npos)
            continue;
        size_t end = s.find_last_not_of(" \t\n\r");
        s = s.substr(start, end == std::string::npos ? std::string::npos
                                                     : end - start + 1);
        if (!s.empty())
            list.push_back(s);
    }
    return list;
}

// ---------------------------------------------------------------------------
// Process entry: one child process we monitor
// ---------------------------------------------------------------------------
struct ProcessEntry {
    pid_t pid = -1;
    std::string name;
    std::vector<std::string> args;
    std::string process_type;
    std::string exception_handler;
    int respawn_limit = g_default_respawn_limit;
    int respawn_cnt = 0;
    bool started = false;
    double time_of_death = 0;

    // For respawn: rebuild and re-exec
    std::string binary_path;
    std::vector<std::string> dag_list;
    std::vector<std::string> plugin_list;
    std::string sched_name;
    std::vector<std::string> extra_args_list;
    std::string cpu_profile_file;
    std::string mem_profile_file;
    int nice_val = 0;
};

static std::vector<ProcessEntry> g_procs;
static volatile sig_atomic_t g_shutdown = 0;

void LogInfo(const std::string& msg) {
    std::cout << "[autolink_launch_" << getpid() << "] INFO " << msg
              << std::endl;
}
void LogWarn(const std::string& msg) {
    std::cerr << "[autolink_launch_" << getpid() << "] WARNING " << msg
              << std::endl;
}
void LogError(const std::string& msg) {
    std::cerr << "[autolink_launch_" << getpid() << "] ERROR " << msg
              << std::endl;
}

// Build argv for library type and start process; returns pid or -1
int StartProcess(ProcessEntry& pe) {
    std::vector<std::string> args;
    if (pe.process_type == "binary") {
        // process_name is full command line (e.g. "binary arg1 arg2")
        std::string name = pe.name;
        size_t pos = 0;
        while (pos < name.size()) {
            size_t end = name.find_first_of(" \t", pos);
            if (end == std::string::npos)
                end = name.size();
            if (end > pos)
                args.push_back(name.substr(pos, end - pos));
            pos = end;
            while (pos < name.size() && (name[pos] == ' ' || name[pos] == '\t'))
                pos++;
        }
    } else {
        args.push_back(pe.binary_path);
        for (const auto& d : pe.dag_list) {
            args.push_back("-d");
            args.push_back(d);
        }
        for (const auto& p : pe.plugin_list) {
            args.push_back("--plugin");
            args.push_back(p);
        }
        if (!pe.name.empty()) {
            args.push_back("-p");
            args.push_back(pe.name);
        }
        if (!pe.sched_name.empty()) {
            args.push_back("-s");
            args.push_back(pe.sched_name);
        }
        for (const auto& a : pe.extra_args_list)
            args.push_back(a);
        if (!pe.cpu_profile_file.empty()) {
            args.push_back("-c");
            args.push_back("-o");
            args.push_back(pe.cpu_profile_file);
        }
        if (!pe.mem_profile_file.empty()) {
            args.push_back("-H");
            args.push_back("-O");
            args.push_back(pe.mem_profile_file);
        }
    }

    pe.args = args;
    std::vector<char*> argv_ptrs;
    for (auto& s : pe.args)
        argv_ptrs.push_back(&s[0]);
    argv_ptrs.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) {
        LogError("fork failed: " + std::string(strerror(errno)));
        return -1;
    }
    if (pid == 0) {
        execvp(argv_ptrs[0], argv_ptrs.data());
        LogError("execvp failed: " + std::string(strerror(errno)));
        _exit(127);
    }

    pe.pid = pid;
    pe.started = true;
    pe.time_of_death = 0;
    LogInfo("Start process [" + pe.name +
            "] successfully. pid: " + std::to_string(pid));
    return pid;
}

bool IsAlive(ProcessEntry& pe) {
    if (!pe.started || pe.pid <= 0)
        return false;
    int status = 0;
    pid_t w = waitpid(pe.pid, &status, WNOHANG);
    if (w == 0)
        return true;
    if (w == pe.pid) {
        pe.time_of_death = static_cast<double>(time(nullptr));
        return false;
    }
    return false;
}

void GetExitState(ProcessEntry& pe) {
    if (pe.pid <= 0)
        return;
    int status = 0;
    waitpid(pe.pid, &status, WNOHANG);
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    std::string cmd;
    for (size_t i = 0; i < pe.args.size(); ++i) {
        if (i)
            cmd += " ";
        cmd += pe.args[i];
    }
    if (code != 0 && code != -1)
        LogError("Process [" + pe.name + "] has died [pid " +
                 std::to_string(pe.pid) + ", exit code " +
                 std::to_string(code) + ", cmd " + cmd + "].");
    else
        LogError("Process [" + pe.name + "] has finished. [pid " +
                 std::to_string(pe.pid) + ", cmd " + cmd + "].");
}

void StopAll(int sig) {
    for (auto& pe : g_procs) {
        if (pe.started && pe.pid > 0) {
            kill(pe.pid, sig);
        }
    }
    for (auto& pe : g_procs) {
        if (pe.started && pe.pid > 0) {
            for (int i = 0; i < force_stop_timeout_secs * 10; ++i) {
                if (waitpid(pe.pid, nullptr, WNOHANG) == pe.pid)
                    break;
                usleep(100000);
            }
            if (waitpid(pe.pid, nullptr, WNOHANG) != pe.pid) {
                LogWarn("Begin force kill process [" + pe.name + "][" +
                        std::to_string(pe.pid) + "].");
                kill(pe.pid, SIGKILL);
                waitpid(pe.pid, nullptr, 0);
            }
            LogInfo("Process [" + pe.name + "] has been stopped.");
        }
    }
    g_procs.clear();
}

void CheckCleanup(int* dead_count, bool* all_died) {
    *dead_count = 0;
    *all_died = false;
    for (auto& pe : g_procs) {
        if (g_shutdown)
            break;
        if (IsAlive(pe))
            continue;
        if (!pe.started)
            continue;

        if (pe.exception_handler == "respawn" &&
            pe.respawn_cnt < pe.respawn_limit) {
            LogWarn("child process [" + pe.name + "][" +
                    std::to_string(pe.pid) + "] exit, respawn!");
            pe.started = false;
            pe.pid = -1;
            if (StartProcess(pe) > 0) {
                pe.respawn_cnt++;
            }
        } else if (pe.exception_handler == "exit") {
            LogWarn("child process [" + pe.name + "][" +
                    std::to_string(pe.pid) + "] exit, stop all");
            g_shutdown = 1;
            return;
        } else {
            (*dead_count)++;
        }
    }
    *all_died = (*dead_count == static_cast<int>(g_procs.size()));
}

bool RunMonitor() {
    while (!g_shutdown) {
        int dead_count = 0;
        bool all_died = false;
        CheckCleanup(&dead_count, &all_died);
        if (g_shutdown)
            break;
        if (all_died)
            break;
        usleep(200000);  // 0.2s
    }
    for (auto& pe : g_procs)
        GetExitState(pe);
    if (g_shutdown)
        return false;
    LogInfo("All processes has died.");
    return true;
}

void ApplyEnvironment(const tinyxml2::XMLElement* root) {
    const tinyxml2::XMLElement* env = root->FirstChildElement("environment");
    if (!env)
        return;
    for (const tinyxml2::XMLElement* var = env->FirstChildElement(); var;
         var = var->NextSiblingElement()) {
        const char* tag = var->Value();
        const char* text = var->GetText();
        if (tag && text)
            setenv(tag, text, 1);
    }
}

int Start(const std::string& launch_file) {
    std::string path = ResolveLaunchFile(launch_file);
    if (path.empty()) {
        LogError("Cannot find launch file: " +
                 (launch_file.empty() ? std::string("autolink.launch")
                                      : launch_file) +
                 " (AUTOLINK_LAUNCH_PATH=" + GetLaunchPathEnv() + ")");
        return 1;
    }

    LogInfo("Launch file [" + path + "]");
    LogInfo(std::string(120, '='));

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(path.c_str()) != tinyxml2::XML_SUCCESS) {
        LogError("Parse xml failed. illegal xml!");
        return 1;
    }

    const tinyxml2::XMLElement* root = doc.RootElement();
    if (!root) {
        LogError("No root element in launch file.");
        return 1;
    }

    ApplyEnvironment(root);

    for (const tinyxml2::XMLElement* module = root->FirstChildElement("module");
         module; module = module->NextSiblingElement("module")) {
        std::string module_name = GetParamValue(module, "name");
        std::string process_name =
            GetParamValue(module, "process_name",
                          "mainboard_default_" + std::to_string(getpid()));
        std::string sched_name =
            GetParamValue(module, "sched_name", "autolink_DEFAULT");
        std::string process_type = GetParamValue(module, "type", "library");
        std::string cpu_profile_file = GetParamValue(module, "cpuprofile");
        std::string mem_profile_file = GetParamValue(module, "memprofile");
        std::string exception_handler =
            GetParamValue(module, "exception_handler");
        std::string respawn_limit_str = GetParamValue(module, "respawn_limit");
        int respawn_limit = g_default_respawn_limit;
        if (!respawn_limit_str.empty()) {
            try {
                respawn_limit = std::stoi(respawn_limit_str);
            } catch (...) {
            }
        }
        std::string nice_str = GetParamValue(module, "nice", "0");
        int nice_val = 0;
        try {
            nice_val = std::stoi(nice_str);
        } catch (...) {
        }

        LogInfo("Load module [" + module_name + "] " + process_type + ": [" +
                process_name + "] [" + sched_name +
                "] conf, exception_handler: [" + exception_handler +
                "], respawn_limit: [" + std::to_string(respawn_limit) + "]");

        ProcessEntry pe;
        pe.name = process_name;
        pe.process_type = process_type;
        pe.exception_handler = exception_handler;
        pe.respawn_limit = respawn_limit;
        pe.sched_name = sched_name;
        pe.cpu_profile_file = cpu_profile_file;
        pe.mem_profile_file = mem_profile_file;
        pe.nice_val = nice_val;

        if (process_type == "binary") {
            if (process_name.empty()) {
                LogError("Start binary failed. Binary process_name is null.");
                continue;
            }
            pe.binary_path =
                process_name;  // first token used by StartProcess for binary
            if (StartProcess(pe) <= 0) {
                LogError("Start manager [" + process_name +
                         "] failed. Stop all!");
                StopAll(SIGINT);
                return 2;
            }
        } else {
            std::vector<std::string> dag_list =
                GetParamList(module, "dag_conf");
            if (dag_list.empty()) {
                LogError("module [" + module_name +
                         "] library dag conf is null.");
                continue;
            }
            pe.binary_path = g_binary_name;
            pe.dag_list = dag_list;
            pe.plugin_list = GetParamList(module, "plugin");
            const char* extra = module->Attribute("extra_args");
            if (extra) {
                std::string ex(extra);
                size_t pos = 0;
                while (pos < ex.size()) {
                    while (pos < ex.size() &&
                           (ex[pos] == ' ' || ex[pos] == '\t'))
                        pos++;
                    if (pos >= ex.size())
                        break;
                    size_t end = ex.find_first_of(" \t", pos);
                    if (end == std::string::npos)
                        end = ex.size();
                    pe.extra_args_list.push_back(ex.substr(pos, end - pos));
                    pos = end;
                }
            }
            if (StartProcess(pe) <= 0) {
                LogError("Start manager [" + process_name +
                         "] failed. Stop all!");
                StopAll(SIGINT);
                return 2;
            }
        }
        g_procs.push_back(pe);
    }

    if (g_procs.empty()) {
        LogError("No module was found in xml config.");
        return 1;
    }

    bool all_died = RunMonitor();
    if (!all_died) {
        LogInfo("Stop all processes...");
        StopAll(SIGINT);
    }
    LogInfo("autolink exit.");
    return 0;
}

void StopLaunch(const std::string& launch_file) {
    std::string cmd;
    if (launch_file.empty()) {
        cmd = "pkill -INT autolink_launch";
    } else {
        cmd = "pkill -INT -f " + launch_file;
    }
    int ret = system(cmd.c_str());
    (void)ret;
    sleep(3);
    LogInfo("Stop autolink launch finished.");
}

void ListLaunch() {
    // List mainboard processes started by launch (best-effort).
    FILE* fp =
        popen("ps -eo pid,args 2>/dev/null | grep '[m]ainboard' || true", "r");
    if (!fp) {
        std::cerr << "Failed to list processes" << std::endl;
        return;
    }
    char buf[512];
    int count = 0;
    while (fgets(buf, sizeof(buf), fp) != nullptr) {
        std::cout << buf;
        ++count;
    }
    pclose(fp);
    if (count == 0) {
        std::cout << "(no mainboard processes found)" << std::endl;
    }
}

void SignalHandler(int sig) {
    LogInfo("Keyboard interrupt received. Stop all processes.");
    g_shutdown = 1;
    StopAll(sig);
}


}  // namespace launch_cli

#include <CLI/CLI.hpp>

#include <memory>
#include <string>

#include "autolink/tools/cli/cmd_launch.hpp"

namespace autolink {
namespace tools {

void SetupLaunch(CLI::App& app) {
    auto* launch =
        app.add_subcommand("launch", "Start/stop modules from launch files");
    launch->require_subcommand(1);

    auto* start = launch->add_subcommand("start", "Start modules");
    auto file = std::make_shared<std::string>();
    start->add_option("file", *file, "Launch file (optional)");
    start->callback([file]() {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = launch_cli::SignalHandler;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);
        int rc = launch_cli::Start(*file);
        if (rc != 0) {
            throw CLI::RuntimeError(rc);
        }
    });

    auto* stop = launch->add_subcommand("stop", "Stop modules");
    auto stop_file = std::make_shared<std::string>();
    stop->add_option("file", *stop_file, "If set, stop only matching launch");
    stop->callback([stop_file]() {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = launch_cli::SignalHandler;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);
        launch_cli::StopLaunch(*stop_file);
    });

    launch->add_subcommand("list", "List running launch/mainboard processes")
        ->callback([]() { launch_cli::ListLaunch(); });
}

}  // namespace tools
}  // namespace autolink
