#include "Generator.h"
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

namespace {
std::atomic_bool cancelled{false};
void Stop(int) { cancelled.store(true, std::memory_order_relaxed); }
class InputCancellation {
    std::atomic_bool done{false};
    std::thread worker;
public:
    InputCancellation() : worker([this] {
        std::string pending;
        while (!done.load(std::memory_order_relaxed)) {
            char bytes[128]; std::size_t count = 0;
#ifdef _WIN32
            const auto input = GetStdHandle(STD_INPUT_HANDLE); DWORD available = 0, received = 0;
            if (GetFileType(input) != FILE_TYPE_PIPE || !PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr)) break;
            if (available && ReadFile(input, bytes, (std::min)(DWORD(sizeof bytes), available), &received, nullptr)) count = received;
#else
            pollfd input{STDIN_FILENO, POLLIN, 0};
            if (poll(&input, 1, 20) > 0) {
                if (input.revents & POLLIN) { const auto received = read(STDIN_FILENO, bytes, sizeof bytes); if (received <= 0) break; count = std::size_t(received); }
                else if (input.revents & (POLLHUP | POLLERR | POLLNVAL)) break;
            }
#endif
            for (std::size_t i = 0; i < count; ++i) {
                if (bytes[i] == '\n') { if (pending == "cancel" || pending == "cancel\r") cancelled = true; pending.clear(); }
                else if (pending.size() < 128) pending += bytes[i];
            }
            if (!count) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }) {}
    ~InputCancellation() { done = true; if (worker.joinable()) worker.join(); }
};
std::uint64_t Number(const std::string& value, const std::string& flag) {
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) throw std::invalid_argument(flag + " requires an unsigned integer");
    std::size_t used = 0; const auto result = std::stoull(value, &used); if (used != value.size()) throw std::invalid_argument("Invalid " + flag); return result;
}
}
int ModelGenMain(int argc, char** argv) {
    try {
        namespace fs = std::filesystem;
        LanguageModels::Options options;
        // Match Corpus Workbench's CLI: omitted budget is unlimited, --compact
        // and --low-memory select v3 and external counting explicitly.
        options.maximumBytes.reset(); options.compact = false; options.lowMemory = false;
        const auto directory = fs::absolute(fs::u8path(argv[0])).parent_path();
        for (const auto& candidate : {directory / "language-models/wordlists", directory / "wordlists", directory.parent_path() / "wordlists"})
            if (fs::is_directory(candidate)) { options.wordListDirectory = candidate; break; }
        bool inputs = false;
        for (int index = 1; index < argc; ++index) {
            const std::string flag = argv[index];
            if (inputs || flag.empty() || flag.front() != '-') { options.inputs.push_back(fs::u8path(flag)); continue; }
            if (flag == "--") { inputs = true; continue; }
            if (flag == "--help" || flag == "-h") {
                std::cout << "UICreatorModelGen --trie-output FILE [--language en|es|fr|de|el|ar|hi|zh|bn|id|ur|ru] [--trie-size BYTES] [--workers N] [--low-memory] [--compact] [--no-cache] -- CORPUS...\n"; return 0;
            }
            if (flag == "--no-subdirectories") { options.recursive = false; continue; }
            if (flag == "--no-cache") { options.useCache = false; continue; }
            if (flag == "--no-wordlist" || flag == "--no-word-list") { options.useWordList = false; continue; }
            if (flag == "--low-memory") { options.lowMemory = true; continue; }
            if (flag == "--compact") { options.compact = true; continue; }
            if (index + 1 >= argc) throw std::invalid_argument("Missing value for " + flag);
            const std::string value = argv[++index];
            if (flag == "--trie-output") options.trieOutput = fs::u8path(value);
            else if (flag == "--file" || flag == "-f") options.inputs.push_back(fs::u8path(value));
            else if (flag == "--json-output" || flag == "--output" || flag == "-o") options.jsonOutput = fs::u8path(value);
            else if (flag == "--language") options.language = value;
            else if (flag == "--workers") { const auto count = Number(value, flag); if (count > 1024) throw std::invalid_argument("Worker count exceeds 1024"); options.workers = static_cast<unsigned>(count); }
            else if (flag == "--trie-size") options.maximumBytes = Number(value, flag);
            else if (flag == "--unigrams") options.limits[0] = Number(value, flag);
            else if (flag == "--bigrams") options.limits[1] = Number(value, flag);
            else if (flag == "--trigrams") options.limits[2] = Number(value, flag);
            else if (flag == "--chunk-entries") { const auto count = Number(value, flag); if (count > SIZE_MAX) throw std::invalid_argument("Count chunk exceeds this platform's address space"); options.chunkEntries = std::size_t(count); }
            else if (flag == "--wordlist-dir") options.wordListDirectory = fs::u8path(value);
            else if (flag == "--word-list" || flag == "--wordlist") options.wordListPath = fs::u8path(value);
            else if (flag == "--cache-directory" || flag == "--cache-dir") options.cacheDirectory = fs::u8path(value);
            else if (flag == "--scratch-directory") options.scratchDirectory = fs::u8path(value);
            else throw std::invalid_argument("Unknown option " + flag);
        }
        std::signal(SIGINT, Stop); std::signal(SIGTERM, Stop); InputCancellation cancellation;
        const auto result = LanguageModels::Generate(options, [](const auto& progress) {
            std::cout << "@progress\t" << progress.percent << '\t' << progress.stage << std::endl;
        }, &cancelled);
        std::cout << "@result\t" << result.counts[0] << '\t' << result.counts[1] << '\t' << result.counts[2] << '\t' << result.trieBytes << std::endl;
        return 0;
    } catch (const LanguageModels::Cancelled& error) { std::cerr << error.what() << '\n'; return 130; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    std::vector<std::string> strings; strings.reserve(argc);
    for (int index = 0; index < argc; ++index) {
        const auto size = WideCharToMultiByte(CP_UTF8, 0, argv[index], -1, nullptr, 0, nullptr, nullptr);
        if (!size) return 1;
        std::string value(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, argv[index], -1, value.data(), size, nullptr, nullptr); value.pop_back(); strings.push_back(std::move(value));
    }
    std::vector<char*> arguments; arguments.reserve(argc); for (auto& value : strings) arguments.push_back(value.data());
    return ModelGenMain(argc, arguments.data());
}
#else
int main(int argc, char** argv) { return ModelGenMain(argc, argv); }
#endif
