target_sources(basaltic_model PRIVATE basaltic_phases.c basaltic_components.c basaltic_systems.c)
target_include_directories(basaltic_model PUBLIC .. components systems)
add_subdirectory(components)
add_subdirectory(systems)
