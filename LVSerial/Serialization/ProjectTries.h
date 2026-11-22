#pragma once
#include "Protobuf.h"
#include <lvgl/lvgl.h>
#include <memory>

namespace Serialization {
class ProjectTries {
public:
    struct Catalog;
    using CatalogPtr = std::shared_ptr<Catalog>;
    class Scope {
    public:
        explicit Scope(const CatalogPtr& catalog);
        ~Scope();
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    private:
        CatalogPtr previous_;
    };

    // Authoring catalog uses canonical base64 and embeds model/index bytes.
    // Missing indexData is generated and retained once during normalization.
    static nlohmann::json Normalize(const nlohmann::json& tries);
    // Returns an updated catalog, selecting an existing byte-identical model
    // when possible. name is a display filename, never an external model path.
    static nlohmann::json Import(const nlohmann::json& tries, const std::string& name,
        const void* bytes, size_t size, std::string* selectedId = nullptr);
    // Moves owned binary payloads or retains explicitly borrowed flash spans,
    // and removes bulk trie data from document before screen records are copied.
    // ESP32 prefers shared PSRAM bytes and releases replaced owned buffers.
    static CatalogPtr Load(nlohmann::json& document, const BorrowedAssets& borrowed = {});
    // Consumes trusted input spans only during this call. Retained model bytes
    // are owned (preferably in PSRAM), so the caller may release its input.
    static CatalogPtr LoadOwned(nlohmann::json& document, const BorrowedAssets& borrowed = {});
    // The widget owns its predictor/scratch and retains the shared model bytes.
    // Empty trieId removes its predictor; builtin-en-us resolves without a blob.
    static void Attach(lv_obj_t* widget, const std::string& trieId,
        bool autocorrect = true, double threshold = 0.3);
    // Used by Protobuf encoding to store catalog data as native BINARY values.
    static nlohmann::json ToWire(const nlohmann::json& tries);
};
}
