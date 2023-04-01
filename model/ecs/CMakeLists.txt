target_sources(basaltic_model PRIVATE flecs.c basaltic_components.c)
target_include_directories(basaltic_model PUBLIC .. components systems)
add_subdirectory(systems)
