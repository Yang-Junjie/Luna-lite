#pragma once
#include "../core/log.h"
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

    template <typename T> AssetHandle add(std::shared_ptr<T> asset)
    {
        if (asset == nullptr) {
            LUNA_ASSERT(asset != nullptr, "Cannot add null asset.");
            LUNA_CORE_ERROR("Failed to add null asset");
            return AssetHandle{0};
        }

        if (!asset->handle.isValid()) {
            do {
                asset->handle = AssetHandle{};
            } while (m_assets.contains(static_cast<uint64_t>(asset->handle)));
        } else if (m_assets.contains(static_cast<uint64_t>(asset->handle))) {
            LUNA_CORE_ERROR("Asset handle already exists: {}", asset->handle.toString());
            return AssetHandle{0};
        }

        const auto key = static_cast<uint64_t>(asset->handle);
        m_assets.emplace(key, std::move(asset));
        return AssetHandle{key};
    }

    template <typename T> T* get(AssetHandle handle)
    {
        auto* asset = getAsset(handle);
        return dynamic_cast<T*>(asset);
    }

    template <typename T> const T* get(AssetHandle handle) const
    {
        const auto* asset = getAsset(handle);
        return dynamic_cast<const T*>(asset);
    }

private:
    AssetDatabase() = default;
    ~AssetDatabase() = default;

    Asset* getAsset(AssetHandle handle)
    {
        if (!handle.isValid()) {
            LUNA_CORE_ERROR("Failed to get asset: invalid handle");
            return nullptr;
        }

        const auto it = m_assets.find(static_cast<uint64_t>(handle));
        if (it == m_assets.end()) {
            LUNA_CORE_ERROR("Failed to get asset: {}", handle.toString());
            return nullptr;
        }

        return it->second.get();
    }

    const Asset* getAsset(AssetHandle handle) const
    {
        if (!handle.isValid()) {
            LUNA_CORE_ERROR("Failed to get asset: invalid handle");
            return nullptr;
        }

        const auto it = m_assets.find(static_cast<uint64_t>(handle));
        if (it == m_assets.end()) {
            LUNA_CORE_ERROR("Failed to get asset: {}", handle.toString());
            return nullptr;
        }

        return it->second.get();
    }

    std::unordered_map<uint64_t, std::shared_ptr<Asset>> m_assets;
};
} // namespace lunalite::asset
