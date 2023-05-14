# Basaltic

A narrow-focus engine and reference game, built for turn based games on a hexagonal grid

Built on C99 with Sokol, SDL2, ImGui, and Flecs

## Startup options

Basaltic supports these program parameters:

-d DIRECTORY

- Use DIRECTORY as the working directory for game resources/assets/other data files. Default is one directory level up from where Basaltic is run (..)

-e

- Start with the editor (ImGui interface) enabled. By default the editor is disabled, and can be toggled with `/~

By default Basaltic will launch to a main menu screen (TODO). These options allow changing startup behavior:

-n SEED X Y

- Automatically start a new game with world seed SEED and chunk dimensions [X, Y]

-l PATH

- Load the save file at PATH (TODO)

-c

- Load the most recent save in the configured save directory (TODO)


## Building from source

Basaltic is designed to be cross-platform and should work on any device that supports OpenGL 4.5 or higher. Currently it has only been tested on Linux and Windows 10. If you would like support for another platform, please let me know!

The included CMake config can generate makefiles for Windows (with MinGW) and Linux.

Requires development libraries for SDL2. On Linux, installing SDL2 with your package manager *should* allow CMake to find it automatically. On Windows, I recommend getting the latest mingw zip from here: https://github.com/libsdl-org/SDL/releases

Requires Sokol and htw_libs. Clone this repository with `--recurse-submodules` or clone directly from https://github.com/htw6174/htw-libs and https://github.com/floooh/sokol into the corresponding directories in this repository.
- cimgui is not included as a submodule because it is precompiled with the appropriate backends (SDL+OpenGL), and the compiled libraries for Linux and Windows are distributed here. This will probably change in the future to allow easier switching of backends
- Flecs is not included as a submodule because it requires a few small changes for this project

On Windows, you will need to tell CMake where SDL2 and MinGW are installed. Do this by setting `SDL2_DIR` and `MINGW_PATH` in the project root's CMakeLists.txt
- `SDL2_DIR` should be the directory where cmake config files are stored, and will look like `(...)/x86_64-w64-mingw32/lib/cmake/SDL2`
- `MINGW_PATH` should be the root mingw directory which contains bin/include/lib directories, and will look like `(...)/mingw` or `(...)/mingw64`

On Windows, you will need GLEW. I set this up by adding it to my mingw installation, copying the dll to `mingw/bin` and the header to `mingw/x86_64-w64-mingw32/include/GL`. If you use a different setup, make sure to change how GLEW is set in CMake

More info at [basalitc.neocities.org](https://basaltic.neocities.org/)
