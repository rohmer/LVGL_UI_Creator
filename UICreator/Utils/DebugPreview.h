#pragma once
#include "../../3rdParty/JSON/json.hpp"
#include <filesystem>
#include <memory>
#include <string>
class DebugPreview {
public:
    DebugPreview();
    ~DebugPreview();
    void Start(nlohmann::json document, const std::string& sourceDirectory);
    void Stop();
    bool Running() const;
    std::string SnapshotPath() const;
private:
    struct Impl;
    std::unique_ptr<Impl> state;
};
