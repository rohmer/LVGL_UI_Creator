#include "FigmaHttp.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#else
#include <dlfcn.h>
// These existing ABI headers avoid requiring the libcurl development package.
// The installed system libcurl performs all networking and TLS verification.
#include "../../3rdParty/Breakpad/src/third_party/curl/curl.h"
#endif

namespace FigmaOnline {
namespace {
constexpr size_t MaximumBody = 64u * 1024u * 1024u;
constexpr size_t MaximumHeaders = 64u * 1024u;
constexpr size_t MaximumUrl = 16384;
using Clock = std::chrono::steady_clock;
struct Url { std::string host, target; };
std::string Lower(std::string value) {
    for(auto& character : value) if(character >= 'A' && character <= 'Z') character += 'a' - 'A';
    return value;
}
Url ParseUrl(const std::string& value) {
    if(value.empty() || value.size() > MaximumUrl) throw std::invalid_argument("Invalid HTTPS URL.");
    for(const unsigned char character : value)
        if(character <= 32 || character >= 127 || character == '\\' || character == '#')
            throw std::invalid_argument("Invalid HTTPS URL.");
    if(value.size() < 8 || Lower(value.substr(0, 8)) != "https://") throw std::invalid_argument("Online Figma import requires HTTPS.");
    const auto end = value.find_first_of("/?", 8);
    auto authority = value.substr(8, end == std::string::npos ? std::string::npos : end - 8);
    if(authority.empty() || authority.find('@') != std::string::npos) throw std::invalid_argument("Invalid HTTPS host.");
    const auto colon = authority.find(':');
    if(colon != std::string::npos) {
        if(authority.substr(colon) != ":443") throw std::invalid_argument("Online Figma import only supports the HTTPS port.");
        authority.resize(colon);
    }
    if(authority.empty() || authority.size() > 253 || authority.front() == '.' || authority.back() == '.') throw std::invalid_argument("Invalid HTTPS host.");
    size_t label = 0;
    for(size_t index = 0; index < authority.size(); ++index) {
        const auto character = authority[index];
        if(character == '.') {
            if(label == 0 || label > 63 || authority[index - 1] == '-') throw std::invalid_argument("Invalid HTTPS host.");
            label = 0;
        } else {
            if(!((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
                 (character >= '0' && character <= '9') || (character == '-' && label != 0))) throw std::invalid_argument("Invalid HTTPS host.");
            ++label;
        }
    }
    if(label == 0 || label > 63 || authority.back() == '-') throw std::invalid_argument("Invalid HTTPS host.");
    auto target = end == std::string::npos ? "/" : value.substr(end);
    if(target.front() == '?') target.insert(target.begin(), '/');
    return {Lower(authority), std::move(target)};
}
std::string Absolute(const Url& previous, const std::string& location) {
    if(location.empty()) throw std::runtime_error("The server returned an empty redirect.");
    if(location.rfind("//", 0) == 0) return "https:" + location;
    if(location.find("://") != std::string::npos) return location;
    const auto origin = "https://" + previous.host;
    if(location.front() == '/') return origin + location;
    const auto query = previous.target.find('?');
    const auto path = previous.target.substr(0, query);
    if(location.front() == '?') return origin + path + location;
    if(location.find(':') != std::string::npos) throw std::invalid_argument("Invalid HTTPS redirect.");
    return origin + path.substr(0, path.find_last_of('/') + 1) + location;
}
void Check(const std::atomic<bool>* cancel, Clock::time_point deadline) {
    if(cancel && cancel->load()) throw std::runtime_error("Online Figma import cancelled.");
    if(Clock::now() >= deadline) throw std::runtime_error("The HTTPS request timed out. Try again.");
}
void Header(HttpResponse& response, const std::string& line) {
    const auto colon = line.find(':');
    if(colon == std::string::npos || colon == 0) return;
    auto name = line.substr(0, colon);
    for(const unsigned char character : name)
        if(!((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') || character == '-')) return;
    const auto start = line.find_first_not_of(" \t\r\n", colon + 1);
    if(start == std::string::npos) return;
    const auto end = line.find_last_not_of(" \t\r\n");
    response.headers[Lower(std::move(name))] = line.substr(start, end - start + 1);
}
bool Redirect(int status) { return status == 301 || status == 302 || status == 303 || status == 307 || status == 308; }

#ifdef _WIN32
struct Internet {
    HINTERNET value = nullptr;
    explicit Internet(HINTERNET handle) : value(handle) {}
    Internet(const Internet&) = delete;
    ~Internet() { if(value) WinHttpCloseHandle(value); }
};
[[noreturn]] void NetworkError() {
    const auto error = GetLastError();
    if(error == ERROR_WINHTTP_SECURE_FAILURE) throw std::runtime_error("The server's HTTPS certificate could not be verified. Check the system clock and trusted certificates.");
    if(error == ERROR_WINHTTP_TIMEOUT) throw std::runtime_error("The HTTPS request timed out. Try again.");
    throw std::runtime_error("HTTPS connection failed (Windows error " + std::to_string(error) + "). Check the network and proxy settings.");
}
void Require(BOOL result) { if(!result) NetworkError(); }
std::wstring Wide(const std::string& value) { return std::wstring(value.begin(), value.end()); }
std::string Utf8(const std::wstring& value) {
    if(value.empty()) return {};
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if(length <= 0) throw std::runtime_error("The server returned invalid HTTP headers.");
    std::string result(static_cast<size_t>(length), '\0');
    if(!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), length, nullptr, nullptr))
        throw std::runtime_error("The server returned invalid HTTP headers.");
    return result;
}
HttpResponse Once(const HttpRequest& input, const Url& url, const std::string& token, const std::atomic<bool>* cancel, Clock::time_point deadline) {
    Check(cancel, deadline);
    Internet session(WinHttpOpen(L"LVGL-UI-Creator/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if(!session.value) NetworkError();
    Require(WinHttpSetTimeouts(session.value, 10000, 10000, 10000, 15000));
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
    protocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
    if(!WinHttpSetOption(session.value, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof protocols)) {
        protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
        Require(WinHttpSetOption(session.value, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof protocols));
    }
    const auto host = Wide(url.host), target = Wide(url.target);
    Internet connection(WinHttpConnect(session.value, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
    if(!connection.value) NetworkError();
    Internet request(WinHttpOpenRequest(connection.value, L"GET", target.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE | WINHTTP_FLAG_ESCAPE_DISABLE | WINHTTP_FLAG_ESCAPE_DISABLE_QUERY));
    if(!request.value) NetworkError();
    DWORD disabled = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
    Require(WinHttpSetOption(request.value, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof disabled));
    const auto headers = token.empty() ? std::wstring{} : L"X-Figma-Token: " + Wide(token) + L"\r\n";
    Require(WinHttpSendRequest(request.value, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
        static_cast<DWORD>(headers.size()), WINHTTP_NO_REQUEST_DATA, 0, 0, 0));
    Check(cancel, deadline);
    Require(WinHttpReceiveResponse(request.value, nullptr));
    Check(cancel, deadline);
    HttpResponse response;
    DWORD code = 0, size = sizeof code;
    Require(WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &size, WINHTTP_NO_HEADER_INDEX));
    response.status = static_cast<int>(code);
    size = 0;
    if(!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size, WINHTTP_NO_HEADER_INDEX) && GetLastError() != ERROR_INSUFFICIENT_BUFFER) NetworkError();
    if(size > MaximumHeaders * sizeof(wchar_t)) throw std::runtime_error("The HTTPS response headers are too large.");
    std::vector<wchar_t> raw(size / sizeof(wchar_t) + 1, 0);
    Require(WinHttpQueryHeaders(request.value, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, raw.data(), &size, WINHTTP_NO_HEADER_INDEX));
    const auto text = Utf8(std::wstring(raw.data()));
    for(size_t begin = 0; begin < text.size();) {
        const auto end = text.find('\n', begin);
        Header(response, text.substr(begin, end == std::string::npos ? std::string::npos : end - begin));
        if(end == std::string::npos) break;
        begin = end + 1;
    }
    if(Redirect(response.status)) return response;
    std::array<char, 32768> buffer;
    for(;;) {
        Check(cancel, deadline);
        DWORD read = 0;
        Require(WinHttpReadData(request.value, buffer.data(), static_cast<DWORD>(buffer.size()), &read));
        if(read == 0) break;
        if(read > input.maxBytes - response.body.size()) throw std::runtime_error("The HTTPS response exceeds the import size limit.");
        response.body.append(buffer.data(), read);
    }
    Check(cancel, deadline);
    return response;
}
#else
class CurlLibrary {
    void* module = nullptr;
    template<typename Function> Function Symbol(const char* name) {
        auto address = dlsym(module, name);
        if(!address) throw std::runtime_error("The installed libcurl library does not support online Figma import.");
        return reinterpret_cast<Function>(address);
    }
public:
    decltype(&curl_easy_init) init;
    decltype(&curl_easy_cleanup) cleanup;
    decltype(&curl_easy_setopt) option;
    decltype(&curl_easy_perform) perform;
    decltype(&curl_easy_getinfo) info;
    decltype(&curl_slist_append) append;
    decltype(&curl_slist_free_all) freeList;
    CurlLibrary() {
        module = dlopen("libcurl.so.4", RTLD_NOW | RTLD_LOCAL);
        if(!module) module = dlopen("libcurl.so", RTLD_NOW | RTLD_LOCAL);
        if(!module) throw std::runtime_error("Online Figma import requires the system libcurl runtime. Install libcurl4 (or the libcurl package for your distribution).");
        try {
            const auto global = Symbol<decltype(&curl_global_init)>("curl_global_init");
            init = Symbol<decltype(init)>("curl_easy_init");
            cleanup = Symbol<decltype(cleanup)>("curl_easy_cleanup");
            option = Symbol<decltype(option)>("curl_easy_setopt");
            perform = Symbol<decltype(perform)>("curl_easy_perform");
            info = Symbol<decltype(info)>("curl_easy_getinfo");
            append = Symbol<decltype(append)>("curl_slist_append");
            freeList = Symbol<decltype(freeList)>("curl_slist_free_all");
            if(global(CURL_GLOBAL_DEFAULT) != CURLE_OK) throw std::runtime_error("The system libcurl library could not initialize HTTPS networking.");
        } catch(...) { dlclose(module); throw; }
    }
    // Keep the system module initialized until process termination. Detached
    // cancelled workers may still be unwinding when editor shutdown begins.
    static CurlLibrary& Get() { static auto* library = new CurlLibrary; return *library; }
};
struct Transfer {
    HttpResponse response;
    const std::atomic<bool>* cancel;
    Clock::time_point deadline;
    size_t limit, headerBytes = 0;
    bool tooLarge = false, headersTooLarge = false, allocationFailed = false;
};
bool Stopped(const Transfer& state) { return (state.cancel && state.cancel->load()) || Clock::now() >= state.deadline; }
size_t Body(char* bytes, size_t size, size_t count, void* userdata) noexcept {
    auto& state = *static_cast<Transfer*>(userdata);
    if(size && count > std::numeric_limits<size_t>::max() / size) { state.tooLarge = true; return 0; }
    const auto length = size * count;
    if(Stopped(state)) return 0;
    if(length > state.limit - state.response.body.size()) { state.tooLarge = true; return 0; }
    try { state.response.body.append(bytes, length); }
    catch(...) { state.allocationFailed = true; return 0; }
    return length;
}
size_t Headers(char* bytes, size_t size, size_t count, void* userdata) noexcept {
    auto& state = *static_cast<Transfer*>(userdata);
    if(size && count > std::numeric_limits<size_t>::max() / size) { state.headersTooLarge = true; return 0; }
    const auto length = size * count;
    if(Stopped(state)) return 0;
    if(length > MaximumHeaders - state.headerBytes) { state.headersTooLarge = true; return 0; }
    state.headerBytes += length;
    try {
        const std::string line(bytes, length);
        // Proxy CONNECT and informational responses can precede the real one.
        if(line.rfind("HTTP/", 0) == 0) state.response.headers.clear();
        else Header(state.response, line);
    } catch(...) { state.allocationFailed = true; return 0; }
    return length;
}
int Progress(void* userdata, double, double, double, double) noexcept { return Stopped(*static_cast<Transfer*>(userdata)) ? 1 : 0; }
HttpResponse Once(const HttpRequest& input, const Url&, const std::string& token, const std::atomic<bool>* cancel, Clock::time_point deadline) {
    Check(cancel, deadline);
    auto& library = CurlLibrary::Get();
    const auto release = [&](CURL* handle) { library.cleanup(handle); };
    std::unique_ptr<CURL, decltype(release)> curl(library.init(), release);
    if(!curl) throw std::runtime_error("Could not create an HTTPS request.");
    const auto freeList = [&](curl_slist* list) { library.freeList(list); };
    std::unique_ptr<curl_slist, decltype(freeList)> headers(nullptr, freeList);
    if(!token.empty()) {
        headers.reset(library.append(nullptr, ("X-Figma-Token: " + token).c_str()));
        if(!headers) throw std::runtime_error("Could not create the Figma authentication header.");
    }
    Transfer state{{}, cancel, deadline, input.maxBytes};
    const auto set = [&](CURLoption option, auto value) {
        if(library.option(curl.get(), option, value) != CURLE_OK) throw std::runtime_error("The installed libcurl cannot configure a secure HTTPS request.");
    };
    set(CURLOPT_URL, input.url.c_str());
    set(CURLOPT_FOLLOWLOCATION, 0L);
    set(CURLOPT_PROTOCOLS, static_cast<long>(CURLPROTO_HTTPS));
    set(CURLOPT_REDIR_PROTOCOLS, static_cast<long>(CURLPROTO_HTTPS));
    set(CURLOPT_SSL_VERIFYPEER, 1L);
    set(CURLOPT_SSL_VERIFYHOST, 2L);
    set(CURLOPT_CONNECTTIMEOUT_MS, 10000L);
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
    set(CURLOPT_TIMEOUT_MS, static_cast<long>(std::max<int64_t>(1, remaining)));
    set(CURLOPT_NOSIGNAL, 1L);
    set(CURLOPT_VERBOSE, 0L);
    set(CURLOPT_USERAGENT, "LVGL-UI-Creator/1.0");
    set(CURLOPT_HTTPHEADER, headers.get());
    set(CURLOPT_WRITEFUNCTION, &Body);
    set(CURLOPT_WRITEDATA, &state);
    set(CURLOPT_HEADERFUNCTION, &Headers);
    set(CURLOPT_HEADERDATA, &state);
    set(CURLOPT_NOPROGRESS, 0L);
    set(CURLOPT_PROGRESSFUNCTION, &Progress);
    set(CURLOPT_PROGRESSDATA, &state);
    const auto result = library.perform(curl.get());
    Check(cancel, deadline);
    if(state.tooLarge) throw std::runtime_error("The HTTPS response exceeds the import size limit.");
    if(state.headersTooLarge) throw std::runtime_error("The HTTPS response headers are too large.");
    if(state.allocationFailed) throw std::runtime_error("Not enough memory to receive the HTTPS response.");
    if(result != CURLE_OK) throw std::runtime_error("HTTPS connection failed. Check the network, proxy settings, system clock and trusted certificates.");
    long code = 0;
    if(library.info(curl.get(), CURLINFO_RESPONSE_CODE, &code) != CURLE_OK) throw std::runtime_error("Could not read the HTTPS response status.");
    state.response.status = static_cast<int>(code);
    return std::move(state.response);
}
#endif
}
HttpResponse Get(const HttpRequest& request, const std::atomic<bool>* cancel) {
    if(request.maxBytes == 0 || request.maxBytes > MaximumBody) throw std::invalid_argument("Invalid HTTPS response size limit.");
    if(request.token.size() > 4096) throw std::invalid_argument("The Figma access token is too long.");
    for(const unsigned char character : request.token)
        if(character <= 32 || character >= 127) throw std::invalid_argument("The Figma access token must not contain whitespace or control characters.");
    auto current = request;
    auto parsed = ParseUrl(current.url);
    auto token = parsed.host == "api.figma.com" ? request.token : std::string{};
    // current.url is deliberately retained as-is so signed CDN query strings
    // are never decoded/re-encoded by the importer.
    const auto deadline = Clock::now() + std::chrono::seconds(60);
    for(unsigned redirects = 0;; ++redirects) {
        Check(cancel, deadline);
        auto response = Once(current, parsed, token, cancel, deadline);
        if(!Redirect(response.status)) return response;
        if(redirects >= 3) throw std::runtime_error("The server returned too many HTTPS redirects.");
        const auto location = response.headers.find("location");
        if(location == response.headers.end()) throw std::runtime_error("The server returned a redirect without a destination.");
        current.url = Absolute(parsed, location->second);
        parsed = ParseUrl(current.url);
        if(parsed.host != "api.figma.com") token.clear();
    }
}
}
