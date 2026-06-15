# LunaLite Architecture

LunaLite is intended to be a lightweight engine core that can be assembled into
different products by statically linking different modules. Runtime hot-swapping
is not the first goal. The first goal is clean static composition.

Examples:

```text
Core + TinyRHI Display + Default Renderer + Lua + Editor
= a general 2D/3D editor

Core + TinyRHI Display + Path Tracer + Mesh Tools + Editor
= a modeling/offline rendering tool

Core + 2D Renderer + 2D Physics + Script
= a 2D game runtime

Core + Asset + Scene
= headless tools, tests, importers, or batch processors
```

## Core Rule

The architecture should follow one direction:

```text
Product Profile -> Modules -> Core / Interfaces
```

Core provides basic data structures, lifecycle hooks, and registries. Modules
register their own capabilities. Product profiles decide which modules are
linked and initialized.

Core should not include or create concrete optional systems such as the default
renderer, Lua runtime, Jolt physics, editor panels, or renderer-specific
components.

## Static Composition

LunaLite should prefer compile-time composition over runtime dynamic plugin
loading.

A product profile is responsible for linking a selected set of modules and
calling their registration functions:

```cpp
registerCoreModule(registry);
registerTinyRHIDisplayModule(registry);
registerDefaultRendererModule(registry);
registerLuaScriptModule(registry);
registerEditorModule(registry);
```

Different executables or CMake targets can choose different sets of modules.
This keeps the engine simple while still allowing different products to be
assembled from the same codebase.

## Main Modules

### LunaLite Core

Core owns the concepts that should exist in every product:

- application base lifecycle
- events, layers, input abstraction, and window abstraction
- scene, entity, transform, parent-child hierarchy, and scene settings
- asset handles, metadata, project information, and asset database concepts
- registries used by modules to attach capabilities
- serialization and tooling extension points

Core should stay independent from concrete renderer, script, physics, and editor
implementations.

### TinyRHI Display

TinyRHI is not only an implementation detail of the default renderer. It is also
the default display and presentation backend for the editor.

TinyRHI Display is responsible for:

- graphics device and swapchain access
- presenting frames to the application window
- rendering ImGui
- showing render results in an ImGui viewport
- uploading CPU-rendered images into a displayable GPU texture

Other renderers do not all need to use TinyRHI internally. However, if their
result needs to appear inside the editor viewport, they should provide output
that the TinyRHI Display module can present.

### Renderer Modules

Renderer modules produce frame output. Examples include:

- default TinyRHI renderer
- CPU path tracer
- software rasterizer
- 2D renderer
- future platform-specific GPU renderers

A renderer module may register:

- renderer factory
- renderer-owned scene components
- render data extraction logic
- asset loaders/importers for renderer assets
- component serializers
- editor inspectors
- editor commands and panels

The default renderer may depend on TinyRHI directly. Core should not depend on
the default renderer.

### Script Modules

Script modules provide runtime behavior for scene entities.

The Lua module may register:

- script runtime factory
- `ScriptComponent`
- script asset importer
- script API bindings
- script editor inspector
- script-related commands

`Scene` should not create a concrete script runtime by itself. Runtime ownership
belongs in a higher-level scene/world runtime that is assembled by the selected
product profile.

### Physics Modules

Physics should be introduced as a module, not as logic embedded directly in
`Scene`.

A physics module may register:

- physics world factory
- rigid body and collider components
- simulation lifecycle hooks
- transform synchronization
- physics debug rendering
- editor inspectors and gizmos
- physics asset importers or shape factories

Possible modules include Jolt, Box2D, or a null physics module.

### Editor Core And Editor Modules

The editor should also follow static module composition.

Editor core should provide:

- editor layer/application shell
- panel registry
- inspector registry
- command UI integration
- selection context
- viewport shell
- menu/tool registration points

Feature modules should register their own editor UI:

- renderer inspectors
- script inspectors
- physics inspectors
- material panels
- asset actions
- debug panels
- gizmos and tools

The long-term direction is that `EditorLayer` owns a list of registered panels
instead of hard-coding every panel as a member.

## Render Output And Presentation

Renderer output should be separated from presentation.

```text
Renderer Module -> RenderOutput -> Display/Presenter -> ImGui Viewport or Swapchain
```

Useful output forms:

- TinyRHI texture or frame image
- CPU RGBA image buffer
- future external/native GPU texture handle

This allows:

- TinyRHI renderers to display directly
- CPU renderers to upload their image through TinyRHI Display
- future independent GPU renderers to use a simple readback path first, then
  optional shared-texture interop later

## Registries

Registries are the main extension mechanism. The important rule is that modules
register into registries; core does not register optional modules by itself.

Important registries include:

- component registry
- component serializer registry
- renderer factory registry
- script runtime factory registry
- physics world factory registry
- asset importer registry
- asset loader registry
- command registry
- editor panel registry
- editor inspector registry
- editor tool/gizmo registry

Component descriptors should eventually include enough behavior to avoid
hard-coded component lists in scene copy, scene serialization, editor
inspection, and tooling commands.

## Current Transition State

The project already has many useful foundations:

- scene/entity/component model
- asset manager and metadata pipeline
- TinyRHI
- default renderer
- Lua script runtime
- editor panels and inspectors
- command and tooling layer
- component, command, asset importer, and asset loader registries

The current code still has several intentional transition issues:

- the main `LunaLite` target links concrete renderer, ImGui, TinyRHI, and Lua
- `Application` creates concrete rendering, presentation, and ImGui systems
- `Scene` owns and creates the Lua script runtime
- `ComponentRegistry` registers renderer and script components from core setup
- `SceneSerializer` hard-codes concrete components
- `Scene::copyFrom` hard-codes concrete components
- `RendererController` creates renderers through an enum switch
- `EditorLayer` owns a fixed set of panels

These are not failures. They are the next seams to convert into static module
registration points.

## Near-Term Direction

The safest next steps are:

1. Define module registration functions without changing behavior.
2. Move optional component registration out of core registry initialization.
3. Move script runtime ownership out of `Scene`.
4. Introduce a product/profile initialization point that calls selected module
   registrations.
5. Split CMake targets so core no longer publicly links concrete optional
   modules.
6. Move component serialization and copy behavior into registered descriptors.
7. Convert editor panels and inspectors to registered editor extensions.

The goal is not to rewrite the engine. The goal is to move one hard-coded
dependency at a time behind a registry owned by the product profile.
