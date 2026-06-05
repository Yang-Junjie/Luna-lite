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
        LUNA_ASSERT(asset, "Cannot add a null asset.");
        if (!asset) {
            LUNA_CORE_ERROR("Failed to add asset: null asset pointer");
            return AssetHandle{0};
        }

        if (!asset->handle.isValid()) {
            do {
                asset->handle = AssetHandle{};
            } while (m_assets.contains(asset->handle));
        } else if (m_assets.contains(asset->handle)) {
            LUNA_CORE_ERROR("Asset handle already exists: {}", asset->handle.toString());
            return AssetHandle{0};
        }

        const auto handle = asset->handle;
        m_assets.emplace(handle, std::move(asset));
        return handle;
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

    bool contains(AssetHandle handle) const
    {
        return handle.isValid() && m_assets.contains(handle);
    }

    void clear()
    {
        m_assets.clear();
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

        const auto it = m_assets.find(handle);
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

        const auto it = m_assets.find(handle);
        if (it == m_assets.end()) {
            LUNA_CORE_ERROR("Failed to get asset: {}", handle.toString());
            return nullptr;
        }

        return it->second.get();
    }

    std::unordered_map<AssetHandle, std::shared_ptr<Asset>> m_assets;
};
} // namespace lunalite::asset
