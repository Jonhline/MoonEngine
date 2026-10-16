# Vulkan build dependencies

The PC build fetches code directly from these upstream repositories; the
dependency implementation is not copied into this source tree.

- Vulkan-Headers v1.3.296, Khronos Group, Apache-2.0 or MIT:
  https://github.com/KhronosGroup/Vulkan-Headers/tree/v1.3.296
- volk vulkan-sdk-1.3.296.0, Arseny Kapoulkine, MIT:
  https://github.com/zeux/volk/tree/vulkan-sdk-1.3.296.0
- glslang 14.3.0, Khronos Group and contributors, licenses collected in
  `GLSLANG_LICENSE.txt`:
  https://github.com/KhronosGroup/glslang/tree/14.3.0

volk loads the system Vulkan driver dynamically. glslang compiles the shared
renderer shaders to SPIR-V in memory. Its optimizer, remapper and standalone
tools are not included in the application. These notices accompany binary
packages through `tools/package_release.py`.
