#pragma once
#include "../../3rdParty/JSON/json.hpp"
namespace Serialization
{
struct AssetProvider
{
    virtual ~AssetProvider() = default;
    virtual const void *Resolve(const nlohmann::json &asset) = 0;
    virtual nlohmann::json Save(const void *)
    {
        return nullptr;
    }
};
inline thread_local AssetProvider *currentAssetProvider = nullptr;
struct AssetScope
{
    AssetProvider *previous;
    explicit AssetScope(AssetProvider &provider) : previous(currentAssetProvider)
    {
        currentAssetProvider = &provider;
    }
    ~AssetScope()
    {
        currentAssetProvider = previous;
    }
};
} // namespace Serialization
