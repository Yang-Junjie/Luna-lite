#pragma once

#include "selection.h"

#include <utility>

namespace lunalite::tooling {

class SelectionContext {
public:
    const Selection& current() const
    {
        return m_selection;
    }

    SelectionKind kind() const
    {
        return m_selection.kind;
    }

    bool hasSelection() const
    {
        return m_selection.kind != SelectionKind::None;
    }

    bool isEntity() const
    {
        return m_selection.kind == SelectionKind::Entity;
    }

    bool isAsset() const
    {
        return m_selection.kind == SelectionKind::Asset;
    }

    bool isFolder() const
    {
        return m_selection.kind == SelectionKind::Folder;
    }

    bool isProject() const
    {
        return m_selection.kind == SelectionKind::Project;
    }

    scene::Entity selectedEntity() const
    {
        return isEntity() ? m_selection.entity : scene::Entity{};
    }

    asset::AssetHandle selectedAsset() const
    {
        return isAsset() ? m_selection.asset : asset::AssetHandle{0};
    }

    const std::filesystem::path& selectedPath() const
    {
        return m_selection.path;
    }

    void clear()
    {
        m_selection = {};
    }

    void selectEntity(scene::Entity entity)
    {
        m_selection = Selection{
            .kind = SelectionKind::Entity,
            .entity = entity,
        };
    }

    void selectAsset(asset::AssetHandle asset)
    {
        m_selection = Selection{
            .kind = SelectionKind::Asset,
            .asset = asset,
        };
    }

    void selectFolder(std::filesystem::path path)
    {
        m_selection = Selection{
            .kind = SelectionKind::Folder,
            .path = std::move(path),
        };
    }

    void selectProject()
    {
        m_selection = Selection{
            .kind = SelectionKind::Project,
        };
    }

private:
    Selection m_selection{};
};

} // namespace lunalite::tooling
