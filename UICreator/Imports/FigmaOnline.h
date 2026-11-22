#pragma once
#include "FigmaHttp.h"
#include <FigmaImporter.h>
#include <atomic>
#include <functional>
#include <string>

namespace FigmaOnline {
using json = nlohmann::json;
struct Location {
    std::string fileKey;
    std::string nodeId;
};
struct Design {
    Location location;
    json document;
};
using Progress = std::function<void(const std::string&)>;
using Transport = std::function<HttpResponse(const HttpRequest&, const std::atomic<bool>*)>;

// Accepts ordinary Figma design/file/prototype links, including node links.
// Neither credentials nor the supplied URL are stored in the project document.
Location ParseUrl(const std::string& link);
Design Load(const std::string& link, const std::string& token,
            const std::atomic<bool>* cancel = nullptr, const Progress& progress = {}, const Transport& transport = {});

// Downloads only artwork needed by the selected frames. Existing bundled
// renders are reused when they have sufficient resolution for this scale.
json AddRenders(const Design& design, const std::string& token, const FigmaImport::Options& options,
                const std::atomic<bool>* cancel = nullptr, const Progress& progress = {}, const Transport& transport = {});
}
