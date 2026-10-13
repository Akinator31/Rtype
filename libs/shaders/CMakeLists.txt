# Source code of the GLSL shaders. Each program is written in glsl/ (one file per stage) and turned at build
# time into a header holding it as strings, so that the binaries need no shader file. Header-only and without
# any dependency: the render library compiles the shaders with raylib.
add_library(rtype_shaders INTERFACE)
add_library(rtype::shaders ALIAS rtype_shaders)

set(RTYPE_SHADERS_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
target_include_directories(rtype_shaders INTERFACE ${RTYPE_SHADERS_INCLUDE_DIR})

# Checks the GLSL syntax at build time when glslangValidator is installed; otherwise errors only show at run
# time, when the graphics driver compiles the shaders
find_program(GLSLANG_VALIDATOR glslangValidator)

# Generates rtype/shaders/<program>.hpp from the GLSL files of the program, again each time one of them changes.
function(rtype_embed_shader program)
    set(output ${RTYPE_SHADERS_INCLUDE_DIR}/rtype/shaders/${program}.hpp)
    set(sources "")
    set(checks "")
    foreach(stage IN LISTS ARGN)
        set(source ${CMAKE_CURRENT_SOURCE_DIR}/glsl/${stage})
        list(APPEND sources ${source})
        if(GLSLANG_VALIDATOR)
            list(APPEND checks COMMAND ${GLSLANG_VALIDATOR} ${source})
        endif()
    endforeach()
    list(JOIN sources "|" joined)

    add_custom_command(
        OUTPUT ${output}
        ${checks}
        COMMAND ${CMAKE_COMMAND} -DPROGRAM=${program} "-DSOURCES=${joined}" -DOUTPUT=${output}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShader.cmake
        DEPENDS ${sources} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShader.cmake
        COMMENT "Embedding shader ${program}"
        VERBATIM
    )
    set_property(GLOBAL APPEND PROPERTY RTYPE_SHADER_HEADERS ${output})
endfunction()

rtype_embed_shader(ColorFilter ColorFilter.frag)
rtype_embed_shader(Lighting Lighting.vert Lighting.frag)

# The headers are generated before any target linking the library is built
get_property(headers GLOBAL PROPERTY RTYPE_SHADER_HEADERS)
add_custom_target(rtype_shaders_headers DEPENDS ${headers})
add_dependencies(rtype_shaders rtype_shaders_headers)
