find_program(PYTHON_EXECUTABLE python3 REQUIRED)

function(process_shader shader_file output_file_var)
    get_filename_component(shader_name "${shader_file}" NAME)
    set(output_file "${CMAKE_BINARY_DIR}/generated/include/shaders/${shader_name}.h")
    set("${output_file_var}" "${output_file}" PARENT_SCOPE)

    add_custom_command(
        OUTPUT "${output_file}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/generated/include/shaders"
        COMMAND "${PYTHON_EXECUTABLE}" "${R3D_ROOT_PATH}/scripts/glsl_processor.py" 
                "${shader_file}" "${CMAKE_CURRENT_BINARY_DIR}/shader_${shader_name}.tmp"
        COMMAND "${PYTHON_EXECUTABLE}" "${R3D_ROOT_PATH}/scripts/bin2c.py" 
                --file "${CMAKE_CURRENT_BINARY_DIR}/shader_${shader_name}.tmp" 
                --name "${shader_name}" --mode text "${output_file}"
        COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_CURRENT_BINARY_DIR}/shader_${shader_name}.tmp"
        DEPENDS "${shader_file}"
        COMMENT "Processing shader: ${shader_file}"
        VERBATIM
    )
endfunction()

function(embed_shaders target_name)
    set(shader_files ${ARGN})

    if(NOT shader_files)
        message(FATAL_ERROR "embed_shaders: No shader file specified")
    endif()

    set(output_files)
    list(LENGTH shader_files shader_count)
    message(STATUS "Configuring processing of ${shader_count} shader(s) for target ${target_name}...")

    foreach(shader_file ${shader_files})
        if(NOT EXISTS "${shader_file}")
            message(FATAL_ERROR "embed_shaders: Shader file not found: ${shader_file}")
        endif()

        process_shader("${shader_file}" output_file)
        list(APPEND output_files "${output_file}")
        message(STATUS "  - ${shader_file} -> ${output_file}")
    endforeach()

    set(shader_target "${target_name}_shaders")
    add_custom_target(${shader_target}
        DEPENDS ${output_files}
        COMMENT "Generating shader headers for ${target_name}"
    )

    add_dependencies(${target_name} ${shader_target})

    message(STATUS "Target ${shader_target} created with ${shader_count} shader(s)")
endfunction()
