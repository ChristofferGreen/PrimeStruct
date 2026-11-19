#include "primec/support/ProgressBeat.h"

#include <cstdlib>
#include <fstream>
#include <string>

#include "primec/testing/TestScratch.h"

#include <filesystem>

#include <fcntl.h>
#include <unistd.h>

#include "third_party/doctest.h"

TEST_SUITE_BEGIN("primestruct.support.progress_beat");

namespace {

// Points PRIMESTRUCT_BEAT_FD at `fd` (or unsets it) for one test and restores
// the value the watchdog gave this process, if any.
class BeatEnvironment {
public:
  explicit BeatEnvironment(const char *value) {
    const char *old = std::getenv("PRIMESTRUCT_BEAT_FD");
    if (old != nullptr) {
      hadOld_ = true;
      old_ = old;
    }
    if (value != nullptr) {
      setenv("PRIMESTRUCT_BEAT_FD", value, 1);
    } else {
      unsetenv("PRIMESTRUCT_BEAT_FD");
    }
  }
  ~BeatEnvironment() {
    if (hadOld_) {
      setenv("PRIMESTRUCT_BEAT_FD", old_.c_str(), 1);
    } else {
      unsetenv("PRIMESTRUCT_BEAT_FD");
    }
  }

private:
  bool hadOld_ = false;
  std::string old_;
};

std::string readAvailable(int fd) {
  char buffer[1024];
  const ssize_t count = read(fd, buffer, sizeof(buffer));
  return count > 0 ? std::string(buffer, static_cast<std::size_t>(count)) : std::string();
}

} // namespace

TEST_CASE("a beat reaches the pipe the environment names") {
  int pipeFds[2];
  REQUIRE(pipe(pipeFds) == 0);
  fcntl(pipeFds[0], F_SETFL, O_NONBLOCK);
  {
    BeatEnvironment environment(std::to_string(pipeFds[1]).c_str());
    CHECK(primec::emitProgressBeat("doctest case: first"));
    CHECK(primec::emitProgressBeat("second\nline"));
  }
  CHECK(readAvailable(pipeFds[0]) == "doctest case: first\nsecond line\n");
  close(pipeFds[0]);
  close(pipeFds[1]);
}

TEST_CASE("a beat writes nothing when the variable is unset or unusable") {
  int pipeFds[2];
  REQUIRE(pipe(pipeFds) == 0);
  fcntl(pipeFds[0], F_SETFL, O_NONBLOCK);
  {
    BeatEnvironment unset(nullptr);
    CHECK_FALSE(primec::emitProgressBeat("unset"));
  }
  {
    BeatEnvironment notANumber("not-a-number");
    CHECK_FALSE(primec::emitProgressBeat("not a number"));
    CHECK(primec::progressBeatDescriptor() == -1);
  }
  CHECK(readAvailable(pipeFds[0]).empty());
  close(pipeFds[0]);
  close(pipeFds[1]);
}

TEST_CASE("a beat never goes to a descriptor that is not a pipe") {
  const int fd = open("/dev/null", O_WRONLY);
  REQUIRE(fd >= 0);
  CHECK_FALSE(primec::emitProgressBeatTo(fd, "not a pipe"));
  CHECK_FALSE(primec::emitProgressBeatTo(-1, "negative"));
  close(fd);
}

TEST_CASE("a beat to a closed reader fails without raising SIGPIPE") {
  int pipeFds[2];
  REQUIRE(pipe(pipeFds) == 0);
  close(pipeFds[0]);
  CHECK_FALSE(primec::emitProgressBeatTo(pipeFds[1], "nobody reads this"));
  close(pipeFds[1]);
}

TEST_CASE("a long beat is cut to one atomic line") {
  int pipeFds[2];
  REQUIRE(pipe(pipeFds) == 0);
  fcntl(pipeFds[0], F_SETFL, O_NONBLOCK);
  CHECK(primec::emitProgressBeatTo(pipeFds[1], std::string(2000, 'x')));
  const std::string line = readAvailable(pipeFds[0]);
  CHECK(line.size() == primec::MaxProgressBeatBytes);
  CHECK(line.back() == '\n');
  close(pipeFds[0]);
  close(pipeFds[1]);
}

TEST_CASE("primec beats between its stages when the variable names a pipe") {
  const auto dir = primec::testing::testScratchPath("progress_beat");
  std::filesystem::create_directories(dir);
  const auto source = dir / "beat.prime";
  {
    std::ofstream file(source);
    file << "[return<int>]\nmain() {\n  return(7i32)\n}\n";
  }
  const std::string command = std::string("\"") + PRIMESTRUCT_TEST_PRIMEC_PATH + "\" --emit=ir \"" +
                              source.string() + "\" -o \"" + (dir / "beat.psir").string() + "\"";
  int pipeFds[2];
  REQUIRE(pipe(pipeFds) == 0);
  fcntl(pipeFds[0], F_SETFL, O_NONBLOCK);
  {
    BeatEnvironment environment(nullptr);
    CHECK(std::system(command.c_str()) == 0);
  }
  CHECK(readAvailable(pipeFds[0]).empty());
  {
    BeatEnvironment environment(std::to_string(pipeFds[1]).c_str());
    CHECK(std::system(command.c_str()) == 0);
  }
  const std::string beats = readAvailable(pipeFds[0]);
  CHECK(beats.find("primec: compile start\n") != std::string::npos);
  CHECK(beats.find("primec: parse\n") != std::string::npos);
  CHECK(beats.find("primec: semantics\n") != std::string::npos);
  close(pipeFds[0]);
  close(pipeFds[1]);
}

TEST_SUITE_END();
