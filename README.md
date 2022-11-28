# Basaltic

A narrow-focus engine built for turn based games on a hexagonal grid

Built on C99 with Vulkan, SDL2, and ImGui

## Startup options

Basaltic supports these program parameters:

-d DIRECTORY

- Use DIRECTORY as the working directory for game resources/assets/other data files. Default is one directory level up from where Basaltic is run (..)

By default Basaltic will launch to your game's main menu screen. These options allow changing startup behavior:

-n SEED

- Automatically start a new game with world seed SEED

-l PATH

- Load the save file at PATH

-c

- Load the most recent save in the configured save directory


## Building from source

Basaltic is intended to be cross-platform, but has so far only been tested on Linux. If you are able to build and run on Windows, please let me know!

Because Vulkan is a requirement for this engine, it will not work on OSX.

Use cmake with the included CMakeLists.txt to generate makefiles and compile shaders. You may need to change the library path for htw_libs depending on where you placed the library binary

Requires development libraries for Vulkan, SDL2, Freetype2, and htw_libs (https://github.com/htw6174/htw-libs)

More info at [basalitc.neocities.org](https://basaltic.neocities.org/)
