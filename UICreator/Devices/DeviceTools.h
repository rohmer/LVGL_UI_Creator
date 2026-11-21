#pragma once
#include <atomic>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

// All process arguments remain separate through the native process API. No shell
// is used, and callbacks run on the caller's worker thread, never on LVGL's thread.
namespace DeviceTools {
enum class Profile { Arduino, EspIdf, CMake };
struct Options {
    Profile profile=Profile::Arduino;
    std::filesystem::path project, build, executable, python, idfRoot, idfTools, config, library;
    std::string toolPath, fqbn="arduino:esp32:nano_nora", port;
    unsigned baud=115200, jobs=4;
    bool checkIdfConstraints=true;
    bool linuxHostWsl=false;
    std::filesystem::path runProgram;
    std::vector<std::string> runArguments;
};
struct Command {
    std::filesystem::path executable, directory;
    std::vector<std::string> arguments;
    std::map<std::string,std::string> environment;
};
struct Result { int exitCode=-1; bool cancelled=false; std::string error; };
using Output=std::function<void(const std::string&)>;
std::vector<Command> BuildPlan(const Options& options, bool flash=false);
std::vector<Command> ProbePlan(const Options& options);
std::vector<Command> RunPlan(const Options& options);
std::string WslPath(const std::filesystem::path& path);
std::string Describe(const Command& command);
// Discover tools without changing installation/global configuration. Explicit
// user paths take precedence; the returned options may still need installation.
Options Detect(Options options, const std::filesystem::path& workspace={});
Result Run(const std::vector<Command>& commands, std::atomic_bool& cancel, Output output);
std::vector<std::string> SerialPorts();
Result Monitor(const std::string& port, unsigned baud, std::atomic_bool& cancel, Output output);
}
