# Luna Lite

Luna Lite is a lightweight version of Luna Engine.

It will be meticulously refined to achieve extremely high readability and performance, and will reach the stage for release.

It is simple and easy to understand.

It will implement these features:
- pluggable render architecture version 1.0
- a simple RHI for the default renderer
  - RHI will be implemented in OpenGL
  - RHI will be implemented in OpenGL ES
  - RHI will be implemented in Vulkan
  - RHI will be implemented in DirectX(maybe)
  - RHI will be implemented in Metal(maybe)
  - RHI will be implemented in WebGPU(maybe)
- a default renderer with RHI

My goal is to transform Luna into different software by statically compiling different modules. 
For instance, combining 2D renderer + 2D physics engine + script module can result in a 2D game engine. 
Combining high-quality offline renderer and mesh editor can lead to a 3D modeling software.
but you cant dynamically change at runtime.
And so on.
