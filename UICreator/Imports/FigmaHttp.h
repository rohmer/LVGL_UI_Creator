#pragma once
#include <atomic>
#include <cstddef>
#include <map>
#include <string>

namespace FigmaOnline {
struct HttpRequest {
    std::string url;
    std::string token;
    size_t maxBytes = 64u * 1024u * 1024u;
};
struct HttpResponse {
    int status = 0;
    // Header names are lower case. Bodies may contain arbitrary image bytes.
    std::map<std::string, std::string> headers;
    std::string body;
};
// HTTPS only, verified certificates, bounded redirects/time/response size.
// Tokens are sent only to api.figma.com; redirects never forward them to a CDN.
// No URL, credential, or server error body is included in exceptions.
HttpResponse Get(const HttpRequest& request, const std::atomic<bool>* cancel = nullptr);
}
