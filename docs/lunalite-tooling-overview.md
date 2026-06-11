# LunaLiteTooling 概览

## 一句话定位

LunaLiteTooling 是 Luna Lite 引擎的**命令式工具层**——它把编辑器中的用户操作（创建实体、导入 Asset、删除节点等）抽象为**可注册、可执行、可组合的命令**，让 UI 面板与底层引擎逻辑解耦。

---

## 问题：为什么需要 Tooling 层？

在没有 Tooling 层时，编辑器面板（如 Hierarchy Panel、Content Browser）直接调用 `scene.createEntity()`、`assetManager.loadAsset()` 等引擎 API。这带来几个问题：

- **面板代码与引擎耦合**：每个面板都要知道 AssetManager、Scene、Factory 的存在和调用方式。
- **操作难以撤销/重做**：直接调用 API 没有中间表示，Undo/Redo 系统无处可插。
- **操作难以脚本化**：如果想通过 Lua 脚本或命令行触发同样的操作，需要重写一遍调用逻辑。
- **逻辑分散**：同一种操作（如"从纹理创建 Sprite"）可能在多个面板中重复实现。

LunaLiteTooling 通过**命令模式**解决这些问题：

```
┌─────────────────┐      ┌──────────────────┐      ┌─────────────────┐
│   Editor Panels  │─────▶│  CommandRegistry  │─────▶│  Engine (Luna)  │
│  (ContentBrowser, │      │  (单例, 路由表)   │      │  Scene, Asset,  │
│   Hierarchy, ...) │      └──────────────────┘      │  Factory, ...   │
└─────────────────┘                                   └─────────────────┘
         │                                                    ▲
         │  ToolContext (Scene*, AssetManager*, ...)           │
         └────────────────────────────────────────────────────┘
                              ▲
                              │
                    ┌─────────────────┐
                    │  CommandArgs    │
                    │  (类型安全参数)   │
                    └─────────────────┘
```

---

## 核心架构：四个层次

### 第一层：值类型层（Value Layer）

定义命令之间传递数据的通用类型。

```
CommandValue         ─── std::variant<bool, int64, uint64, double, string, path, AssetHandle>
CommandArgs          ─── unordered_map<string, CommandValue>  （输入参数）
CommandResult        ─── { success, message, unordered_map<string, CommandValue> } （输出结果）
```

- `CommandValue` 是类型擦除容器，支持 7 种常用类型。
- `CommandArgs` 提供 `set<T>(key, val)` 和 `get<T>(key) -> optional<T>` 的类型安全接口。
- `CommandResult` 除了标志位和消息，还可以携带多个返回值（如 `created_entity`、`created_asset`）。

### 第二层：命令层（Command Layer）

定义命令的结构和注册/执行机制。

```cpp
using CommandHandler = std::function<CommandResult(ToolContext&, const CommandArgs&)>;

struct CommandDesc {
    std::string id;        // 唯一标识，如 "scene.create_entity"
    std::string label;     // 人类可读名称，如 "Create Entity"
    std::string category;  // 分类，如 "Scene"
    CommandHandler execute; // 实际执行函数
};
```

核心类 `CommandRegistry` 是一个**单例路由表**：

| 方法 | 作用 |
|------|------|
| `registerCommand(CommandDesc)` | 注册一个命令（检查 id 非空、handler 非空、id 不重复） |
| `registerDefaults()` | 批量注册内置命令（Asset + Scene 命令） |
| `find(id)` | 按 id 查找命令 |
| `commands()` | 列出所有已注册命令 |
| `execute(id, context, args)` | 查找并执行命令 |

`registerDefaults()` 采用**懒加载**：第一次调用 `find()` 或 `execute()` 时自动触发，后续调用直接跳过。

### 第三层：上下文层（Context Layer）

命令在执行时需要访问引擎的各种子系统，但命令本身不应该直接持有这些依赖。`ToolContext` 作为**依赖聚合体**，将命令需要的所有外部依赖集中注入：

```cpp
class ToolContext {
    scene::Scene*          m_scene;              // 当前活动场景
    asset::AssetManager*   m_asset_manager;      // 资产管理器（单例）
    asset::AssetFactoryManager* m_asset_factory_manager; // 工厂管理器
};
```

`SelectionContext` 则跟踪编辑器的**选中状态**，面板之间通过它共享选择信息：

```
SelectionKind:  None | Entity | Asset | Folder | Project
Selection:      { kind, entity, asset, path }
```

### 第四层：命令实现层（Command Implementation Layer）

每个命令是一个自由函数（通常放在匿名 namespace 中），通过 `registerXxxCommands()` 注册到 Registry。

**当前内置命令：**

| 命令 ID | 分类 | 功能 |
|---------|------|------|
| `asset.create_sprite` | Asset | 从纹理创建 Sprite 资源 |
| `scene.create_entity` | Scene | 创建空实体 |
| `scene.delete_entity` | Scene | 删除实体 |
| `scene.set_parent` | Scene | 设置父子关系 |
| `scene.clear_parent` | Scene | 解除父子关系 |
| `scene.create_entity_from_asset` | Scene | 从 Asset 创建实体（支持 Mesh/Sprite/Prefab/Script） |

---

## 设计模式地图

### 1. Command 模式（核心）

> 将请求封装为对象，从而使你可以参数化客户端、队列化请求、记录日志、支持撤销。

每个命令是一个 `CommandDesc` 对象，包含 id 和执行函数。Registry 是调用者与接收者之间的中介。

### 2. Registry / Service Locator

`CommandRegistry` 是单例，充当所有命令的中央注册表。任何代码都可以通过 `CommandRegistry::get().execute(...)` 触发任意命令。

### 3. Context Object

`ToolContext` 将执行环境（Scene、AssetManager 等）打包成一个对象传入命令，避免了命令直接依赖全局状态或单例（尽管 AssetManager 本身就是单例，但注入方式保留了测试时替换的可能性）。

### 4. Type-Safe Variant Arguments

使用 `std::variant` 实现类型安全的动态参数传递，避免了 `void*` 或字符串参数的类型不安全问题。

### 5. Strategy（命令处理器）

每个 `CommandDesc` 持有自己的 `CommandHandler`，不同命令可以有不同的执行策略，Registry 只负责路由，不关心执行逻辑。

---

## 数据流：一条命令的完整生命周期

以"用户在 Content Browser 右键纹理 → 创建 Sprite"为例：

```
1. ContentBrowser 检测到右键菜单点击 "Create Sprite"
       │
2. ContentBrowser 构造 ToolContext 和 CommandArgs
       │    context = ToolContext{}
       │    args.set("source_asset", textureHandle)
       │    args.set("target_directory", "Assets/")
       │
3. 通过 Registry 执行命令
       │    result = CommandRegistry::get().execute("asset.create_sprite", context, args)
       │
4. Registry 查找 "asset.create_sprite" → 找到 createSprite() 函数
       │
5. createSprite() 执行：
       │    a. 从 args 取出 source_asset 和 target_directory
       │    b. 从 context.assetManager() 获取源资源元数据
       │    c. 调用 context.assetFactoryManager().create("SpriteFromTexture", ...)
       │    d. 返回 CommandResult::ok(...).set("created_asset", ...)
       │
6. ContentBrowser 检查 result.success，获取 created_asset
```

---

## 扩展指南

### 添加新命令

1. 在 `commands/` 下新建 `xxx_commands.h` / `xxx_commands.cpp`
2. 定义命令 ID 常量：`inline constexpr std::string_view MyCommandId = "category.my_action";`
3. 实现命令函数（放在匿名 namespace）：
   ```cpp
   namespace {
   CommandResult myAction(ToolContext& ctx, const CommandArgs& args) {
       // 1. 校验参数
       // 2. 通过 ctx 访问引擎
       // 3. 返回结果
   }
   }
   ```
4. 在 `registerXxxCommands()` 中注册：
   ```cpp
   registry.registerCommand(CommandDesc{
       .id = std::string{MyCommandId},
       .label = "My Action",
       .category = "MyCategory",
       .execute = myAction,
   });
   ```
5. 在 `CommandRegistry::registerDefaults()` 中调用 `registerXxxCommands()`。

### 在编辑器中调用命令

```cpp
tooling::ToolContext context;
context.setScene(m_scene);

tooling::CommandArgs args;
args.set("entity", entityToValue(myEntity));
args.set("parent_entity", entityToValue(parentEntity));

auto result = tooling::CommandRegistry::get().execute(
    tooling::SetParentCommandId, context, args);
if (result.success) {
    // 处理结果
}
```

---

## 已知限制与未来方向

| 限制 | 可能的改进 |
|------|-----------|
| 无 Undo/Redo 支持 | 命令可以记录逆向操作，形成历史栈 |
| 无命令队列/宏 | 可以添加 CommandBatch 支持批量执行 |
| ToolContext 字段较少 | 未来可扩展（如 Physics、Audio 上下文） |
| 无异步命令 | 可扩展 CommandResult 支持 pending 状态 |
| 命令发现依赖编译期注册 | 可添加反射/JSON 注册机制 |

---

## 相关文件索引

| 文件 | 职责 |
|------|------|
| `commands/command.h` | `CommandDesc`、`CommandHandler` 类型定义 |
| `commands/command_value.h` | `CommandValue` 变体类型 |
| `commands/command_args.h` | `CommandArgs` 参数容器 |
| `commands/command_result.h` | `CommandResult` 结果容器 |
| `commands/command_registry.h/.cpp` | 命令注册表（单例） |
| `commands/asset_commands.h/.cpp` | Asset 相关命令实现 |
| `commands/scene_commands.h/.cpp` | Scene 相关命令实现 |
| `context/tool_context.h` | 命令执行上下文 |
| `context/selection.h` | `Selection` / `SelectionKind` 类型 |
| `context/selection_context.h` | 选择状态管理器 |
