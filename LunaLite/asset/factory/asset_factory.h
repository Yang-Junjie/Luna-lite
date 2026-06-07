#pragma once
#include "../asset_metadata.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace lunalite::asset {

struct AssetFactoryContext {
    const AssetMetadata* source{nullptr};
    std::filesystem::path target_directory;
};

struct AssetFactoryResult {
    AssetHandle handle{0};
    std::filesystem::path path;
    std::string error;
};

class AssetFactory {
public:
    virtual ~AssetFactory() = default;

    virtual std::string_view id() const = 0;
    virtual std::string_view label() const = 0;
    virtual AssetType outputType() const = 0;
    virtual bool canCreate(const AssetFactoryContext& context) const = 0;
    virtual AssetFactoryResult create(const AssetFactoryContext& context) = 0;
};

} // namespace lunalite::asset
