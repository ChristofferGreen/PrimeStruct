#include "primec/support/OptionsVmFlags.h"

#include "primec/support/Ascii.h"
#include "primec/support/Options.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>

namespace primec {

namespace {

// A schedule seed: any unsigned 64-bit number, 0 included.
bool parseScheduleSeed(const std::string &text,
                       uint64_t &out,
                       std::string &error,
                       const char *source) {
  const std::string normalized = trimAscii(text);
  if (normalized.empty()) {
    error = std::string(source) + " requires a number";
    return false;
  }
  if (!std::all_of(normalized.begin(), normalized.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
      })) {
    error = std::string("invalid ") + source + " value: " + normalized;
    return false;
  }
  try {
    size_t consumed = 0;
    const unsigned long long parsed = std::stoull(normalized, &consumed);
    if (consumed != normalized.size()) {
      error = std::string("invalid ") + source + " value: " + normalized;
      return false;
    }
    out = static_cast<uint64_t>(parsed);
    return true;
  } catch (...) {
    error = std::string("invalid ") + source + " value: " + normalized;
    return false;
  }
}

// `--name value` or `--name=value`. NotVmFlag when `arg` is another flag.
VmFlagResult takeValue(const std::string &arg,
                       const std::string &name,
                       int argc,
                       char **argv,
                       int &index,
                       std::string &value) {
  if (arg == name) {
    if (index + 1 >= argc) {
      return VmFlagResult::Error;
    }
    value = argv[++index];
    return VmFlagResult::Consumed;
  }
  if (arg.rfind(name + "=", 0) == 0) {
    value = arg.substr(name.size() + 1);
    return VmFlagResult::Consumed;
  }
  return VmFlagResult::NotVmFlag;
}

} // namespace

VmFlagResult parseVmFlag(
    const std::string &arg, int argc, char **argv, int &index, Options &out, std::string &error) {
  std::string value;
  if (arg == "--vm-threads") {
    out.vmThreads = true;
    return VmFlagResult::Consumed;
  }
  VmFlagResult result = takeValue(arg, "--vm-schedule-seed", argc, argv, index, value);
  if (result == VmFlagResult::Consumed) {
    uint64_t seed = 0;
    if (!parseScheduleSeed(value, seed, error, "--vm-schedule-seed")) {
      return VmFlagResult::Error;
    }
    out.vmScheduleSeed = seed;
    return result;
  }
  if (result == VmFlagResult::Error) {
    error = "--vm-schedule-seed requires a number";
    return result;
  }
  result = takeValue(arg, "--vm-repeat", argc, argv, index, value);
  if (result == VmFlagResult::Consumed) {
    uint64_t repeat = 0;
    if (!parseScheduleSeed(value, repeat, error, "--vm-repeat")) {
      return VmFlagResult::Error;
    }
    if (repeat == 0 || repeat > 1000) {
      error = "--vm-repeat requires a number from 1 to 1000";
      return VmFlagResult::Error;
    }
    out.vmRepeat = static_cast<uint32_t>(repeat);
    return result;
  }
  if (result == VmFlagResult::Error) {
    error = "--vm-repeat requires a number";
    return result;
  }
  result = takeValue(arg, "--vm-record-sync", argc, argv, index, value);
  if (result == VmFlagResult::Consumed) {
    out.vmRecordSyncPath = value;
  } else if (result == VmFlagResult::Error) {
    error = "--vm-record-sync requires a file path";
  }
  if (result != VmFlagResult::NotVmFlag) {
    return result;
  }
  result = takeValue(arg, "--vm-replay-sync", argc, argv, index, value);
  if (result == VmFlagResult::Consumed) {
    out.vmReplaySyncPath = value;
  } else if (result == VmFlagResult::Error) {
    error = "--vm-replay-sync requires a file path";
  }
  return result;
}

bool validateVmFlags(Options &out, std::string &error) {
  if (!out.vmRecordSyncPath.empty() && !out.vmThreads) {
    error = "--vm-record-sync records a --vm-threads run and needs --vm-threads";
    return false;
  }
  if (out.vmRepeat != 0 && !out.vmThreads) {
    error = "--vm-repeat repeats a --vm-threads run and needs --vm-threads";
    return false;
  }
  if (out.vmRepeat != 0 && (!out.vmRecordSyncPath.empty() || !out.vmReplaySyncPath.empty())) {
    error = "--vm-repeat cannot be combined with --vm-record-sync or --vm-replay-sync";
    return false;
  }
  if (!out.vmReplaySyncPath.empty() && (out.vmThreads || !out.vmRecordSyncPath.empty())) {
    error = "--vm-replay-sync runs on the deterministic scheduler and cannot be combined with "
            "--vm-threads or --vm-record-sync";
    return false;
  }
  if (!out.vmScheduleSeed.has_value()) {
    if (const char *seedText = std::getenv("PRIMESTRUCT_VM_SCHEDULE_SEED");
        seedText != nullptr && *seedText != '\0') {
      uint64_t seed = 0;
      if (!parseScheduleSeed(seedText, seed, error, "PRIMESTRUCT_VM_SCHEDULE_SEED")) {
        return false;
      }
      out.vmScheduleSeed = seed;
    }
  }
  return true;
}

} // namespace primec
