# Basaltic

A narrow-focus engine built for turn based games on a hexagonal grid

Built on C99 with Vulkan, SDL2, and ImGui

## Building from source

Basaltic is intended to be cross-platform, but has so far only been tested on Linux. If you are able to build and run on Windows, please let me know!

Because Vulkan is a requirement for this engine, it will not work on OSX.

Use cmake with the included CMakeLists.txt to generate makefiles and compile shaders. You may need to change the library path for htw_libs depending on where you placed the library binary

Requires Vulkan, SDL2, Freetype2, and the htw_libs library (https://github.com/htw6174/htw-libs)

More info at [basalitc.neocities.org](https://basaltic.neocities.org/)
