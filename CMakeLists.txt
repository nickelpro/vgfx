cmake_minimum_required(VERSION 3.14)
project(vgfx)

# Associate compiled shaders with a target
find_program(glslc glslc)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)
function(target_build_shaders target)
    cmake_parse_arguments("" "" "" "" ${ARGN})
    foreach(shader_in ${_UNPARSED_ARGUMENTS})
        get_filename_component(p ${shader_in} NAME)
        set(shader_out ${CMAKE_BINARY_DIR}/shaders/${p}.spv)
        get_filename_component(p ${shader_in} ABSOLUTE)
        add_custom_command(
            OUTPUT ${shader_out}
            COMMAND ${glslc} -o ${shader_out} ${p}
            DEPENDS ${shader_in}
            IMPLICIT_DEPENDS CXX ${shader_in}
            VERBATIM
        )
        set_source_files_properties(${shader_out} PROPERTIES GENERATED TRUE)
        target_sources(${target} PRIVATE ${shader_out})
    endforeach(shader_in)
endfunction(target_build_shaders)

find_path(
    graphene_config "graphene-config.h"
    HINTS "/usr/lib/graphene-1.0/include"
)
add_executable(main "src/main.c")
target_compile_options(main PRIVATE -Wall -Wpedantic)
target_sources(main PRIVATE
    "src/vk.c" "src/log.c"
    "src/sds.c" "src/verts.c"
    "src/vk_mem_alloc.cpp"
    "include/vgfx.h" "include/logc/log.h"
)
target_include_directories(main PRIVATE
    "include" ${graphene_config}
)
target_link_libraries(main glfw dl graphene-1.0 m vulkan)
target_build_shaders(main "shaders/shader.vert" "shaders/shader.frag")
