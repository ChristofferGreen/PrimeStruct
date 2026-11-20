#include "primec/support/ProcessCpuTime.h"

#include <cstddef>
#include <string>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
#define PRIMESTRUCT_CPU_TIME_LIBPROC 1
#endif

#if defined(PRIMESTRUCT_CPU_TIME_LIBPROC)
#include <libproc.h>
#include <sys/resource.h>
#elif defined(__linux__)
#include <dirent.h>

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#endif

namespace primec {

#if defined(PRIMESTRUCT_CPU_TIME_LIBPROC)

namespace {

// Own time plus the time of waited-for children of `pid`, then the same for each live child. A child that exits is
// waited for by its parent, whose child time grows by what the child's own plus child time was, so the sum only grows.
bool addTreeCpuTime(pid_t pid, int depth, std::uint64_t &total) {
  rusage_info_v2 info{};
  if (proc_pid_rusage(pid, RUSAGE_INFO_V2, reinterpret_cast<rusage_info_t *>(&info)) != 0) {
    return false;
  }
  total += info.ri_user_time + info.ri_system_time + info.ri_child_user_time + info.ri_child_system_time;
  if (depth >= 8) {
    return true;
  }
  pid_t children[64];
  const int count = proc_listchildpids(pid, children, static_cast<int>(sizeof(children)));
  for (int index = 0; index < count && index < 64; ++index) {
    (void)addTreeCpuTime(children[index], depth + 1, total);
  }
  return true;
}

} // namespace

bool processTreeCpuTime(pid_t pid, std::uint64_t &out) {
  std::uint64_t total = 0;
  if (!addTreeCpuTime(pid, 0, total)) {
    return false;
  }
  out = total;
  return true;
}

#elif defined(__linux__) && !defined(PRIMESTRUCT_CPU_TIME_LIBPROC)

namespace {

struct LinuxProcess {
  pid_t parent = 0;
  std::uint64_t cpu = 0;
};

// The fields of /proc/<pid>/stat after the parenthesised command name: state, ppid, ..., utime (14), stime (15),
// cutime (16), cstime (17).
bool readLinuxProcess(pid_t pid, LinuxProcess &out) {
  std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
  std::string line;
  if (!std::getline(stat, line)) {
    return false;
  }
  const std::size_t close = line.rfind(')');
  if (close == std::string::npos) {
    return false;
  }
  std::istringstream fields(line.substr(close + 1));
  std::string field;
  std::uint64_t cpu = 0;
  for (int index = 3; index <= 17 && (fields >> field); ++index) {
    try {
      if (index == 4) {
        out.parent = static_cast<pid_t>(std::stol(field));
      } else if (index >= 14) {
        cpu += std::stoull(field);
      }
    } catch (...) {
      return false;
    }
  }
  out.cpu = cpu;
  return true;
}

} // namespace

bool processTreeCpuTime(pid_t pid, std::uint64_t &out) {
  std::unordered_map<pid_t, LinuxProcess> processes;
  DIR *proc = opendir("/proc");
  if (proc == nullptr) {
    return false;
  }
  while (const dirent *entry = readdir(proc)) {
    const std::string name = entry->d_name;
    if (name.empty() || name.find_first_not_of("0123456789") != std::string::npos) {
      continue;
    }
    const pid_t other = static_cast<pid_t>(std::stol(name));
    LinuxProcess process;
    if (readLinuxProcess(other, process)) {
      processes[other] = process;
    }
  }
  closedir(proc);
  if (processes.find(pid) == processes.end()) {
    return false;
  }
  // A process below `pid` is found by walking its parents; the walk is short and stops at `pid` or at init.
  std::uint64_t total = 0;
  for (const auto &[other, process] : processes) {
    pid_t current = other;
    for (int hops = 0; hops < 16 && current > 1; ++hops) {
      if (current == pid) {
        total += process.cpu;
        break;
      }
      const auto parent = processes.find(current);
      if (parent == processes.end()) {
        break;
      }
      current = parent->second.parent;
    }
  }
  out = total;
  return true;
}

#else

bool processTreeCpuTime(pid_t pid, std::uint64_t &out) {
  (void)pid;
  (void)out;
  return false;
}

#endif

} // namespace primec
