# LunaLiteTooling 使用指南

本文档展示如何在编辑器代码、测试代码和潜在脚本中使用 LunaLiteTooling。

---

## 基础用法

### 1. 创建 ToolContext 并设置活动场景

```cpp
#include "../LunaLiteTooling/context/tool_context.h"
#include "../LunaLiteTooling/commands/command_registry.h"

using namespace lunalite;

// 创建上下文
tooling::ToolContext context;

// 绑定当前编辑的场景
context.setScene(m_editorScene);
// AssetManager 和 AssetFactoryManager 使用默认单例
```

### 2. 构造参数并执行命令

```cpp
// 构建参数
tooling::CommandArgs args;
args.set("name", std::string{"Directional Light"});

// 执行命令
auto result = tooling::CommandRegistry::get().execute(
    tooling::CreateEntityCommandId,  // "scene.create_entity"
    context,
    args
);
```

### 3. 处理结果

```cpp
if (!result.success) {
    LUNA_CORE_ERROR("Failed to create entity: {}", result.message);
    return;
}

// 提取返回值
const auto* entityVal = result.get<uint64_t>("created_entity");
if (entityVal) {
    scene::Entity entity = entityFromValue(*entityVal);
    // 使用 entity...
}
```

---

## 典型场景

### 场景 A：Content Browser 右键创建 Sprite

在 Content Browser 面板中，用户右键一个纹理文件，选择 "Create Sprite"。

```cpp
// 在 ContentBrowserPanel::onImGuiRender() 中
if (ImGui::BeginPopupContextItem()) {
    // ... 渲染菜单项 ...
    if (ImGui::MenuItem("Create Sprite")) {
        // 记录待执行命令
        m_pendingCommand = PendingCommand{
            .command_id = tooling::CreateSpriteCommandId,
            .source = textureHandle,
            .target_directory = currentDirectory,
        };
    }
    ImGui::EndPopup();
}

// 在帧末尾处理待执行命令
if (m_pendingCommand) {
    tooling::ToolContext context;
    tooling::CommandArgs args;
    args.set("source_asset", m_pendingCommand->source);
    args.set("target_directory", m_pendingCommand->target_directory);

    auto result = tooling::CommandRegistry::get().execute(
        m_pendingCommand->command_id, context, args);

    if (result.success) {
        const auto* handle = result.get<asset::AssetHandle>("created_asset");
        // 刷新 Asset 列表...
    }
    m_pendingCommand.reset();
}
```

### 场景 B：Hierarchy 面板拖放 Asset 到实体

用户从 Content Browser 拖拽一个 Mesh 到 Hierarchy 面板中的某个实体上。

```cpp
// 在 HierarchyPanel 的拖放处理中
if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET")) {
    auto& [handle, type] = *static_cast<AssetPayload*>(payload->Data);

    tooling::ToolContext context;
    context.setScene(m_scene);
    tooling::CommandArgs args;
    args.set("source_asset", handle);
    args.set("asset_type", static_cast<uint64_t>(type));
    if (targetEntity) {
        args.set("target_entity", entityToValue(targetEntity));
    }

    auto result = tooling::CommandRegistry::get().execute(
        tooling::CreateEntityFromAssetCommandId, context, args);

    if (result.success) {
        const auto* entityVal = result.get<uint64_t>("created_entity");
        // 选中新创建的实体
        if (entityVal) {
            m_selection.selectEntity(entityFromValue(*entityVal));
        }
    }
}
```

### 场景 C：使用 SelectionContext 管理选中状态

`SelectionContext` 是面板之间共享选中状态的桥梁。

```cpp
// HierarchyPanel 选中实体时
void HierarchyPanel::onEntityClicked(scene::Entity entity) {
    m_selection.selectEntity(entity);    // SelectionContext
}

// InspectorPanel 读取选中状态
void InspectorPanel::onImGuiRender() {
    if (!m_selection.isEntity()) {
        ImGui::Text("No entity selected");
        return;
    }

    auto entity = m_selection.selectedEntity();
    // 显示实体属性...
}

// HierarchyPanel 处理删除快捷键
void HierarchyPanel::handleDeleteKey() {
    if (!m_selection.isEntity()) return;

    auto entity = m_selection.selectedEntity();
    auto entityVal = entityToValue(entity);

    tooling::ToolContext context;
    context.setScene(m_scene);
    tooling::CommandArgs args;
    args.set("entity", entityVal);

    auto result = tooling::CommandRegistry::get().execute(
        tooling::DeleteEntityCommandId, context, args);

    if (result.success) {
        m_selection.clear();
    }
}
```

---

## 编写测试

测试文件中展示了如何独立使用 Tooling 命令，不依赖编辑器 UI：

```cpp
// 完整的端到端测试：创建项目 → 导入纹理 → 执行命令 → 验证结果
int main() {
    // 1. 设置测试环境
    project::ProjectInfo info;
    info.name = "TestProject";
    info.assets_path = "Assets";
    project::ProjectManager::instance().createProject(projectRoot, info);

    // 2. 写入测试纹理
    writeTestPngTexture(texturePath);
    asset::AssetManager::get().loadProjectAssets();

    // 3. 获取纹理句柄
    auto textureHandle = asset::AssetManager::get()
        .getHandleByRelativePath("Assets/texture.png");

    // 4. 通过 Tooling 创建 Sprite
    tooling::ToolContext context;
    tooling::CommandArgs args;
    args.set("source_asset", textureHandle);
    args.set("target_directory", std::filesystem::path{"Assets"});

    auto result = tooling::CommandRegistry::get().execute(
        tooling::CreateSpriteCommandId, context, args);

    // 5. 断言结果
    assert(result.success);
    auto* createdAsset = result.get<asset::AssetHandle>("created_asset");
    assert(createdAsset && createdAsset->isValid());

    // 6. 验证创建的 Sprite 属性
    auto* sprite = asset::AssetManager::get().getAsset<asset::Sprite>(*createdAsset);
    assert(sprite->texture == textureHandle);
    assert(sprite->source_rect.width == 2);
    // ...
}
```

---

## 实体编码工具函数

由于 Entity 不能直接在 `CommandValue`（variant）中传递，Tooling 提供了编码/解码工具：

```cpp
// Entity → uint64_t (用于存入 CommandArgs / CommandResult)
uint64_t encoded = entityToValue(entity);

// uint64_t → Entity (用于从 CommandResult 中读取)
scene::Entity decoded = entityFromValue(encoded);
```

在命令实现中，通常使用辅助函数 `entityArg(args, key)` 一步完成读取和解码：

```cpp
auto entity = entityArg(args, "entity");
if (!entity) {
    return CommandResult::fail("requires entity");
}
```

---

## 添加新命令的完整步骤

以一个假想的 `"physics.add_rigidbody"` 命令为例：

### Step 1: 创建命令文件

`LunaLiteTooling/commands/physics_commands.h`:

```cpp
#pragma once
#include <string_view>

namespace lunalite::tooling {
class CommandRegistry;

inline constexpr std::string_view AddRigidbodyCommandId = "physics.add_rigidbody";

void registerPhysicsCommands(CommandRegistry& registry);
}
```

### Step 2: 实现命令

`LunaLiteTooling/commands/physics_commands.cpp`:

```cpp
#include "physics_commands.h"
#include "command_registry.h"
#include "../../LunaLite/scene/scene.h"

namespace lunalite::tooling {
namespace {

CommandResult addRigidbody(ToolContext& ctx, const CommandArgs& args) {
    auto* scene = ctx.scene();
    if (!scene) return CommandResult::fail("No active scene");

    auto entity = entityArg(args, "entity");
    if (!entity) return CommandResult::fail("physics.add_rigidbody requires entity");

    auto mass = args.get<double>("mass").value_or(1.0);
    auto is_kinematic = args.get<bool>("is_kinematic").value_or(false);

    // 添加物理组件...
    // scene->addComponent<RigidbodyComponent>(*entity, mass, is_kinematic);

    return CommandResult::ok("Rigidbody added")
        .set("entity", entityToValue(*entity));
}

} // namespace

void registerPhysicsCommands(CommandRegistry& registry) {
    registry.registerCommand(CommandDesc{
        .id = std::string{AddRigidbodyCommandId},
        .label = "Add Rigidbody",
        .category = "Physics",
        .execute = addRigidbody,
    });
}

} // namespace lunalite::tooling
```

### Step 3: 注册到 Registry

修改 `command_registry.cpp` 的 `registerDefaults()`:

```cpp
void CommandRegistry::registerDefaults() {
    if (m_defaults_registered) return;
    m_defaults_registered = true;

    registerAssetCommands(*this);
    registerSceneCommands(*this);
    registerPhysicsCommands(*this);   // ← 新增
}
```

### Step 4: 更新 CMakeLists.txt

```cmake
add_library(LunaLiteTooling STATIC
    commands/asset_commands.cpp
    commands/command_registry.cpp
    commands/physics_commands.cpp    # ← 新增
    commands/scene_commands.cpp
)
```

### Step 5: 在编辑器中调用

```cpp
tooling::CommandArgs args;
args.set("entity", entityToValue(selectedEntity));
args.set("mass", 5.0);

auto result = tooling::CommandRegistry::get().execute(
    tooling::AddRigidbodyCommandId, context, args);
```

---

## 面板间通信模式

Tooling 提供了两种面板间通信机制：

### 1. Command + SelectionContext（推荐）

```
Panel A                    Panel B
   │                          │
   │  executeCommand()        │
   ├─────────────────────────▶│
   │                          │  CommandRegistry
   │                          │  modifies Scene
   │                          │
   │  result.set("entity",..) │
   │◀─────────────────────────┤
   │                          │
   │  m_selection.selectEntity(...)
   │                          │
   │                          ▼
   │                    Panel B 通过 SelectionContext 感知变化
```

- Panel A 执行命令，命令修改 Scene
- Panel A 从 `CommandResult` 中提取结果
- Panel A 更新 `SelectionContext`，所有观察该 Context 的面板自动感知

### 2. 共享 SelectionContext 引用

```cpp
// EditorLayer 创建时
m_selection;  // SelectionContext 实例

// 构造面板时注入同一个 SelectionContext 引用
m_hierarchy_panel  = HierarchyPanel(m_scene, m_selection);
m_inspector_panel  = InspectorPanel(m_scene, m_selection);
m_content_browser  = ContentBrowserPanel(m_selection);
```

所有面板持有同一个 `SelectionContext&`，任何面板修改选中状态，其他面板立即可见。
