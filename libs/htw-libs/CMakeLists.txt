cmake_minimum_required(VERSION 3.0)

project(htw LANGUAGES C)

add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)
#add_compile_definitions($<$<CONFIG:Debug>:VK_DEBUG>) # use to enable Vulkan validation layers

add_library(htw SHARED htw_core_math.c htw_geomap_chunkmap.c htw_geomap_hexgrid.c htw_geomap_tilemap.c htw_geomap_valuemap.c htw_geomap_generators.c htw_geomap_spatialStorage.c htw_vulkan.c)

find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS} ${Vulkan_INCLUDE_DIRS})
target_link_libraries(htw PUBLIC ${SDL2_LIBRARIES} ${Vulkan_LIBRARIES})

set_target_properties(htw PROPERTIES PUBLIC_HEADER htw_core.h)

install(TARGETS htw LIBRARY PUBLIC_HEADER DESTINATION lib)
