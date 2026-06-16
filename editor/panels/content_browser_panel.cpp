#include "../../LunaLite/asset/asset_manager.h"
#include "../../LunaLite/asset/factory/asset_factory_manager.h"
#include "../../LunaLite/core/log.h"
#include "../../LunaLite/project/project_manager.h"
#include "../../LunaLiteTooling/commands/asset_commands.h"
#include "../drag_drop.h"
#include "../editor_actions.h"
#include "../modal_dialog.h"
#include "content_browser_panel.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <imgui.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace lunalite::editor {
class ContentBrowserPanel::CreateFolderModal final : public ModalDialog {
public:
    explicit CreateFolderModal(ContentBrowserPanel& panel);

    void openFor(std::filesystem::path target_directory);
    void draw(const std::filesystem::path& project_root, const std::filesystem::path& assets_relative);

private:
    void onDrawContent() override;
    void onDismissed() override;

    ContentBrowserPanel& m_panel;
    std::filesystem::path m_target_directory;
    std::filesystem::path m_project_root;
    std::filesystem::path m_assets_relative;
    std::array<char, 256> m_folder_name{};
    std::string m_error;
    bool m_focus_name{false};
};

namespace {
struct PendingCommandRequest {
    std::string command_id;
    asset::AssetHandle source{0};
    std::filesystem::path target_directory;
};

struct BrowserContext {
    std::filesystem::path project_root;
    std::filesystem::path assets_relative;
    std::filesystem::path assets_root;
};

bool hasParentTraversal(const std::filesystem::path& path)
{
    for (const auto& part : path) {
        if (part == "..") {
            return true;
        }
    }
    return false;
}

bool isSafeRelativePath(const std::filesystem::path& path)
{
    return !path.is_absolute() && !hasParentTraversal(path);
}

bool isSameOrChildPath(const std::filesystem::path& path, const std::filesystem::path& root)
{
    const auto normalizedPath = path.lexically_normal();
    const auto normalizedRoot = root.lexically_normal();
    if (normalizedPath == normalizedRoot) {
        return true;
    }

    if (normalizedRoot.empty()) {
        return isSafeRelativePath(normalizedPath);
    }

    const auto relative = normalizedPath.lexically_relative(normalizedRoot);
    return !relative.empty() && !relative.is_absolute() && !hasParentTraversal(relative);
}

std::optional<BrowserContext> makeBrowserContext()
{
    const auto projectRoot = project::ProjectManager::instance().getProjectRootPath();
    const auto& projectInfo = project::ProjectManager::instance().getProjectInfo();
    if (!projectRoot || !projectInfo) {
        return std::nullopt;
    }

    const auto assetsRelative = projectInfo->assets_path.lexically_normal();
    if (!isSafeRelativePath(assetsRelative)) {
        LUNA_CORE_ERROR("Content Browser cannot use invalid assets path '{}'", assetsRelative.string());
        return std::nullopt;
    }

    return BrowserContext{
        .project_root = *projectRoot,
        .assets_relative = assetsRelative,
        .assets_root = (*projectRoot / assetsRelative).lexically_normal(),
    };
}

std::filesystem::path projectRelativePath(const BrowserContext& context, const std::filesystem::path& path)
{
    const auto normalized = path.lexically_normal();
    if (!normalized.is_absolute()) {
        return normalized;
    }

    const auto relative = normalized.lexically_relative(context.project_root);
    return relative.empty() ? normalized : relative.lexically_normal();
}

std::string displayNameForDirectory(const BrowserContext& context, const std::filesystem::path& directory)
{
    const auto relative = projectRelativePath(context, directory);
    if (relative == context.assets_relative) {
        const auto label = context.assets_relative.filename().string();
        return label.empty() || label == "." ? std::string{"Assets"} : label;
    }

    const auto label = directory.filename().string();
    return label.empty() ? directory.string() : label;
}

std::string displayPath(const std::filesystem::path& path)
{
    const auto text = path.generic_string();
    return text.empty() ? std::string{"."} : text;
}

std::vector<std::filesystem::path> childDirectories(const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> directories;

    std::error_code error;
    std::filesystem::directory_iterator it(directory, error);
    if (error) {
        LUNA_CORE_ERROR("Failed to enumerate directory '{}': {}", directory.string(), error.message());
        return directories;
    }

    for (const auto& entry : it) {
        std::error_code entryError;
        if (!entry.is_directory(entryError) || entryError) {
            continue;
        }
        directories.push_back(entry.path().lexically_normal());
    }

    std::sort(directories.begin(), directories.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.filename().string() < rhs.filename().string();
    });
    return directories;
}

std::vector<const asset::AssetMetadata*> assetsInDirectory(const BrowserContext& context,
                                                           const std::filesystem::path& directory)
{
    std::vector<const asset::AssetMetadata*> assets;
    const auto normalizedDirectory = directory.lexically_normal();
    for (const auto& [_, metadata] : asset::AssetManager::get().getMetadataRegistry()) {
        if (!metadata.Handle.isValid() || metadata.Type == asset::AssetType::None || metadata.MemoryOnly) {
            continue;
        }

        const auto relativePath = projectRelativePath(context, metadata.FilePath);
        if (relativePath.parent_path().lexically_normal() == normalizedDirectory) {
            assets.push_back(&metadata);
        }
    }

    std::sort(assets.begin(), assets.end(), [](const auto* lhs, const auto* rhs) {
        return lhs->FilePath.filename().string() < rhs->FilePath.filename().string();
    });
    return assets;
}

bool isSingleFolderName(std::string_view name)
{
    if (name.empty()) {
        return false;
    }

    const std::filesystem::path path{name};
    return !path.is_absolute() && !path.has_parent_path() && path.filename() == path && !hasParentTraversal(path);
}
} // namespace

ContentBrowserPanel::ContentBrowserPanel(tooling::SelectionContext& selection)
    : m_selection(selection),
      m_create_folder_modal(std::make_unique<CreateFolderModal>(*this))
{}

ContentBrowserPanel::~ContentBrowserPanel() = default;

ContentBrowserPanel::CreateFolderModal::CreateFolderModal(ContentBrowserPanel& panel)
    : ModalDialog("Create Folder"),
      m_panel(panel)
{}

void ContentBrowserPanel::CreateFolderModal::openFor(std::filesystem::path target_directory)
{
    m_target_directory = std::move(target_directory);
    m_error.clear();
    m_folder_name.fill('\0');
    constexpr std::string_view defaultName{"New Folder"};
    std::copy(defaultName.begin(), defaultName.end(), m_folder_name.begin());
    m_focus_name = true;
    open();
}

void ContentBrowserPanel::CreateFolderModal::draw(const std::filesystem::path& project_root,
                                                  const std::filesystem::path& assets_relative)
{
    if (m_target_directory.empty()) {
        close();
        return;
    }

    m_project_root = project_root;
    m_assets_relative = assets_relative;
    ModalDialog::draw();
}

void ContentBrowserPanel::CreateFolderModal::onDrawContent()
{
    if (m_focus_name) {
        ImGui::SetKeyboardFocusHere();
        m_focus_name = false;
    }

    const bool enterPressed =
        ImGui::InputText("Name", m_folder_name.data(), m_folder_name.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    if (!m_error.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.2f, 1.0f), "%s", m_error.c_str());
    }

    const auto createFolder = [&]() {
        const std::string folderName = m_folder_name.data();
        if (!isSingleFolderName(folderName)) {
            m_error = "Invalid folder name";
            return;
        }

        const auto targetRelative = m_target_directory.lexically_normal();
        if (!isSameOrChildPath(targetRelative, m_assets_relative)) {
            m_error = "Target is outside Assets";
            return;
        }

        const auto newFolderRelative = (targetRelative / folderName).lexically_normal();
        if (!isSameOrChildPath(newFolderRelative, m_assets_relative)) {
            m_error = "Target is outside Assets";
            return;
        }

        const auto newFolderAbsolute = (m_project_root / newFolderRelative).lexically_normal();
        if (std::filesystem::exists(newFolderAbsolute)) {
            m_error = "Folder already exists";
            return;
        }

        std::error_code error;
        if (!std::filesystem::create_directory(newFolderAbsolute, error)) {
            m_error = error ? error.message() : std::string{"Failed to create folder"};
            return;
        }

        m_panel.m_current_directory = newFolderRelative;
        m_panel.m_selection.selectFolder(newFolderRelative);
        onDismissed();
        closeCurrent();
    };

    if (enterPressed || ImGui::Button("Create")) {
        createFolder();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        onDismissed();
        closeCurrent();
    }
}

void ContentBrowserPanel::CreateFolderModal::onDismissed()
{
    m_target_directory.clear();
    m_error.clear();
    m_focus_name = false;
}

void ContentBrowserPanel::startCreateFolder(std::filesystem::path target_directory)
{
    m_create_folder_modal->openFor(std::move(target_directory));
}

void ContentBrowserPanel::onImGuiRender()
{
    ImGui::Begin("Content Browser");

    const auto context = makeBrowserContext();
    if (!context) {
        ImGui::TextUnformatted("No project loaded");
        ImGui::End();
        return;
    }

    if (!std::filesystem::exists(context->assets_root) || !std::filesystem::is_directory(context->assets_root)) {
        ImGui::TextUnformatted("Assets directory missing");
        ImGui::End();
        return;
    }

    if (m_current_directory.empty() || !isSameOrChildPath(m_current_directory, context->assets_relative) ||
        !std::filesystem::is_directory(context->project_root / m_current_directory)) {
        m_current_directory = context->assets_relative;
    }

    const auto assetsRootText = context->assets_root.string();
    ImGui::TextUnformatted(assetsRootText.c_str());
    ImGui::Separator();

    if (ImGui::Button("Refresh")) {
        if (!asset::AssetManager::get().loadProjectAssets()) {
            LUNA_CORE_ERROR("Failed to refresh project assets");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Create Folder")) {
        startCreateFolder(m_current_directory);
    }
    ImGui::SameLine();
    const auto currentDirectoryText = displayPath(m_current_directory);
    ImGui::TextDisabled("%s", currentDirectoryText.c_str());
    ImGui::Separator();

    std::optional<PendingCommandRequest> pendingCommandRequest;
    const float treeWidth = 240.0f;
    ImGui::BeginChild("ContentBrowserDirectories", ImVec2(treeWidth, 0.0f), true);

    const auto drawDirectoryTree = [&](const auto& self, const std::filesystem::path& directory) -> void {
        const auto relative = projectRelativePath(*context, directory);
        const auto children = childDirectories(directory);
        auto flags =
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (relative == m_current_directory) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (children.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        if (relative == context->assets_relative) {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        const auto label = displayNameForDirectory(*context, directory);
        ImGui::PushID(relative.generic_string().c_str());
        const bool open = ImGui::TreeNodeEx("##folder", flags, "%s", label.c_str());
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            m_current_directory = relative;
            m_selection.selectFolder(relative);
        }

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Create Folder")) {
                startCreateFolder(relative);
            }
            ImGui::EndPopup();
        }

        if (open && !children.empty()) {
            for (const auto& child : children) {
                self(self, child);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    };
    drawDirectoryTree(drawDirectoryTree, context->assets_root);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("ContentBrowserItems", ImVec2(0.0f, 0.0f), true);

    const auto currentAbsoluteDirectory = (context->project_root / m_current_directory).lexically_normal();
    const auto directories = childDirectories(currentAbsoluteDirectory);
    const auto assets = assetsInDirectory(*context, m_current_directory);

    for (const auto& directory : directories) {
        const auto relative = projectRelativePath(*context, directory);
        const auto label = "[Folder] " + displayNameForDirectory(*context, directory);
        ImGui::PushID(relative.generic_string().c_str());
        const bool selected = m_selection.isFolder() && m_selection.selectedPath() == relative;
        if (ImGui::Selectable(label.c_str(), selected)) {
            m_selection.selectFolder(relative);
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            m_current_directory = relative;
            m_selection.selectFolder(relative);
        }
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Create Folder")) {
                startCreateFolder(relative);
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    for (const auto* metadata : assets) {
        const auto name = metadata->Name.empty() ? metadata->FilePath.filename().string() : metadata->Name;
        const auto label = name + " (" + asset::assetTypeToString(metadata->Type) + ")";
        const auto id = metadata->Handle.toString();
        ImGui::PushID(id.c_str());
        const bool selected = m_selection.isAsset() && m_selection.selectedAsset() == metadata->Handle;
        if (ImGui::Selectable(label.c_str(), selected)) {
            m_selection.selectAsset(metadata->Handle);
        }

        const drag_drop::AssetPayload payload{
            .handle = metadata->Handle,
            .type = metadata->Type,
        };
        drag_drop::setAssetPayload(payload, label.c_str());

        const auto relativePath = projectRelativePath(*context, metadata->FilePath);
        const asset::AssetFactoryContext factoryContext{
            .source = metadata,
            .target_directory = relativePath.parent_path(),
        };
        const auto factories = asset::AssetFactoryManager::get().factoriesFor(factoryContext);
        if (!factories.empty() && ImGui::BeginPopupContextItem()) {
            for (const auto* factory : factories) {
                const auto commandId = tooling::commandIdForAssetFactory(factory->id());
                if (!commandId) {
                    continue;
                }

                const auto factoryLabel = std::string{factory->label()};
                if (ImGui::MenuItem(factoryLabel.c_str())) {
                    pendingCommandRequest = PendingCommandRequest{
                        .command_id = std::string{*commandId},
                        .source = metadata->Handle,
                        .target_directory = relativePath.parent_path(),
                    };
                }
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    if (ImGui::BeginPopupContextWindow("ContentBrowserDirectoryContext",
                                       ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("Create Folder")) {
            startCreateFolder(m_current_directory);
        }
        ImGui::EndPopup();
    }
    const auto available = ImGui::GetContentRegionAvail();
    if (available.y > 0.0f) {
        ImGui::Dummy(available);
    }

    ImGui::EndChild();

    m_create_folder_modal->draw(context->project_root, context->assets_relative);

    if (pendingCommandRequest) {
        const auto result = actions::executeAssetCommand(
            pendingCommandRequest->command_id, pendingCommandRequest->source, pendingCommandRequest->target_directory);
        if (!result.success) {
            LUNA_CORE_ERROR("Failed to execute command '{}': {}",
                            pendingCommandRequest->command_id,
                            result.message.empty() ? "unknown error" : result.message);
        } else if (const auto* created = result.get<asset::AssetHandle>("created_asset")) {
            if (created->isValid()) {
                m_selection.selectAsset(*created);
            }
        }
    }

    ImGui::End();
}

} // namespace lunalite::editor
