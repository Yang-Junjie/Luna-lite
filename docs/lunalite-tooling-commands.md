# LunaLiteTooling 命令参考

## 命令系统基础

### CommandValue —— 类型安全的参数载体

```cpp
using CommandValue = std::variant<
    bool,
    int64_t,
    uint64_t,
    double,
    std::string,
    std::filesystem::path,
    asset::AssetHandle
>;
```

支持 7 种类型，涵盖了工具层需要的所有数据形态。通过 `std::variant` + `std::get_if` 实现编译期类型检查。

### CommandArgs —— 输入参数

```cpp
CommandArgs args;
args.set("source_asset", textureHandle);          // AssetHandle
args.set("name", std::string{"Player"});          // string
args.set("keep_world_transform", true);           // bool

auto handle = args.get<asset::AssetHandle>("source_asset");  // optional<AssetHandle>
bool exists  = args.contains("name");                        // true
```

- 使用 `set<T>(key, value)` 设置参数
- 使用 `get<T>(key)` 获取参数（返回 `std::optional<T>`）
- 使用 `contains(key)` 检查是否存在
- 类型不匹配时 `get<T>` 返回 `nullopt`，不会崩溃

### CommandResult —— 输出结果

```cpp
// 成功
auto result = CommandResult::ok("Entity created");
result.set("created_entity", entityToValue(entity));

// 失败
auto result = CommandResult::fail("scene.create_entity requires an active scene");

// 读取结果
if (result.success) {
    auto* entityVal = result.get<uint64_t>("created_entity");
}
```

### CommandHandler —— 命令函数签名

```cpp
using CommandHandler = std::function<CommandResult(ToolContext&, const CommandArgs&)>;
```

每个命令函数：
1. 接收 `ToolContext&` —— 获取 Scene、AssetManager 等依赖
2. 接收 `const CommandArgs&` —— 读取参数
3. 返回 `CommandResult` —— 报告成功/失败并携带数据

---

## 所有内置命令

### Asset 命令

#### `asset.create_sprite`

从纹理资源创建 Sprite 资源。

| 属性 | 值 |
|------|-----|
| **输入参数** | `source_asset` (AssetHandle, 必需): 源纹理<br>`target_directory` (path, 必需): 目标目录 |
| **返回结果** | `created_asset` (AssetHandle): 新创建的 Sprite 句柄<br>`created_path` (path): 新 Sprite 文件路径 |
| **工厂 ID** | `SpriteFromTexture` |

**实现逻辑：**
1. 校验 `source_asset` 有效
2. 校验 `target_directory` 存在
3. 通过 `context.assetManager()` 获取源材质元数据
4. 构造 `AssetFactoryContext`，调用 `context.assetFactoryManager().create("SpriteFromTexture", ...)`
5. 返回新创建的 Sprite 句柄和路径

**编辑器入口：** Content Browser 面板 → 右键纹理 → "Create Sprite"

---

### Scene 命令

#### `scene.create_entity`

在活动场景中创建一个空实体。

| 属性 | 值 |
|------|-----|
| **输入参数** | `name` (string, 可选): 实体名称<br>`parent_entity` (uint64, 可选): 父实体编码 |
| **返回结果** | `created_entity` (uint64): 新实体编码 |

**实现逻辑：**
1. 获取活动场景
2. 调用 `scene->createEntity()`
3. 如果提供了 `name`，设置 `TagComponent`
4. 如果提供了 `parent_entity`，调用 `scene->setParent()`；失败则销毁实体并返回错误

**实体编码说明：** Entity 在跨命令边界传递时，使用 `uint64_t` 编码。工具层提供 `entityToValue()` / `entityFromValue()` 进行转换。

---

#### `scene.delete_entity`

从活动场景中删除一个实体（及其子实体）。

| 属性 | 值 |
|------|-----|
| **输入参数** | `entity` (uint64, 必需): 要删除的实体 |
| **返回结果** | `deleted_entity` (uint64): 已删除实体编码 |

**安全校验：**
- 活动场景必须存在
- `entity` 参数必须提供
- 实体必须在场景中有效

---

#### `scene.set_parent`

为实体设置父节点。

| 属性 | 值 |
|------|-----|
| **输入参数** | `entity` (uint64, 必需): 子实体<br>`parent_entity` (uint64, 必需): 父实体<br>`keep_world_transform` (bool, 可选, 默认 true): 是否保持世界变换 |
| **返回结果** | `entity` (uint64): 子实体<br>`parent_entity` (uint64): 父实体 |

---

#### `scene.clear_parent`

解除实体的父子关系。

| 属性 | 值 |
|------|-----|
| **输入参数** | `entity` (uint64, 必需): 目标实体<br>`keep_world_transform` (bool, 可选, 默认 true): 是否保持世界变换 |
| **返回结果** | `entity` (uint64): 目标实体 |

---

#### `scene.create_entity_from_asset`

从 Asset 创建实体——这是**最复杂的命令**，根据资产类型分派不同的创建逻辑。

| 属性 | 值 |
|------|-----|
| **输入参数** | `source_asset` (AssetHandle, 必需): 源资产<br>`target_entity` (uint64, 可选): 目标父实体<br>`asset_type` (uint64, 可选): 资产类型（用于 AssetType 没有 metadata 时的兜底） |
| **返回结果** | `entity` (uint64) / `created_entity` (uint64) / `affected_entity` (uint64) |

**分派逻辑：**

```
接收 source_asset
       │
       ▼
  resolveAssetType() ─── 从 metadata 获取类型，失败则从 args 的 asset_type 兜底
       │
       ├── AssetType::Prefab ──▶ scene->instantiatePrefab()
       │
       ├── AssetType::Mesh ────▶ 先尝试查找同伴 .lunaprefab 文件
       │                         ├── 找到 → instantiatePrefab()
       │                         └── 未找到 → createMeshRendererEntity()
       │
       ├── AssetType::Sprite ──▶ createSpriteRendererEntity()
       │
       └── AssetType::Script ──▶ target_entity 存在？→ addScriptToEntity()
                                  target_entity 不存在？→ createEntity() + addScriptToEntity()
```

**Mesh 同伴文件机制：** 当导入一个 Mesh 时，系统会检查同目录下是否存在 `同名的 .lunaprefab` 文件。如果存在，优先实例化 Prefab（因为它可能包含更完整的实体配置，如碰撞体、脚本等），否则回退到创建裸 MeshRenderer 实体。

---

## 命令注册流程

```
CommandRegistry::registerDefaults()
    │
    ├── registerAssetCommands(registry)
    │       └── registry.registerCommand("asset.create_sprite", ...)
    │
    └── registerSceneCommands(registry)
            ├── registry.registerCommand("scene.create_entity", ...)
            ├── registry.registerCommand("scene.delete_entity", ...)
            ├── registry.registerCommand("scene.set_parent", ...)
            ├── registry.registerCommand("scene.clear_parent", ...)
            └── registry.registerCommand("scene.create_entity_from_asset", ...)
```

注册时 `CommandRegistry::registerCommand()` 执行三项校验：
1. `id` 不能为空
2. `execute` handler 不能为空
3. `id` 不能重复

任何校验失败都会打印 `LUNA_CORE_ERROR` 并返回 `false`。

`registerDefaults()` 是幂等的——`m_defaults_registered` 标志位确保只执行一次。但 `find()` 和 `execute()` 都会在首次调用时自动触发 `registerDefaults()`，因此调用方无需关心注册时机。

---

## 命令查找机制

### `commandIdForAssetFactory`

这是一个工厂 ID 到命令 ID 的映射函数：

```cpp
std::optional<std::string_view> commandIdForAssetFactory(std::string_view factory_id);
```

当前映射：

| 工厂 ID | 命令 ID |
|---------|---------|
| `SpriteFromTexture` | `asset.create_sprite` |

**用途：** Content Browser 在显示右键菜单时，根据当前选中资产可用的工厂列表，通过此函数查找对应的命令。如果没有映射，说明该工厂操作尚未通过 Tooling 层暴露。
