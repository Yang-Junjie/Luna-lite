#pragma once
#include "asset.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace lunalite::asset {
class AssetDatabase {
public:
    static AssetDatabase& get()
    {
        static AssetDatabase instance;
        return instance;
    }

    AssetDatabase(const AssetDatabase&) = delete;
    AssetDatabase& operator=(const AssetDatabase&) = delete;
    AssetDatabase(AssetDatabase&&) = delete;
    AssetDatabase& operator=(AssetDatabase&&) = delete;

    template <typename T>
    AssetHandle add(std::shared_ptr<T> asset)
    {
        if (asset == nullptr) {
            return AssetHandle{0};
        }

        if (!asset->handle.isValid()) {
            asset->handle = AssetHandle{};
        }

        const auto key = static_cast<uint64_t>(asset->handle);
        m_assets[key] = std::move(asset);
        return AssetHandle{key};
    }

    template <typename T>
    T* get(AssetHandle handle)
    {
        auto* asset = getAsset(handle);
        return dynamic_cast<T*>(asset);
    }

    template <typename T>
    const T* get(AssetHandle handle) const
    {
        const auto* asset = getAsset(handle);
        return dynamic_cast<const T*>(asset);
    }

private:
    AssetDatabase() = default;
    ~AssetDatabase() = default;

    Asset* getAsset(AssetHandle handle)
    {
        const auto it = m_assets.find(static_cast<uint64_t>(handle));
        if (it == m_assets.end()) {
            return nullptr;
        }

        return it->second.get();
    }

    const Asset* getAsset(AssetHandle handle) const
    {
        const auto it = m_assets.find(static_cast<uint64_t>(handle));
        if (it == m_assets.end()) {
            return nullptr;
        }

        return it->second.get();
    }

    std::unordered_map<uint64_t, std::shared_ptr<Asset>> m_assets;
};
} // namespace lunalite::asset
