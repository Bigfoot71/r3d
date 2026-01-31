find_program(PYTHON_EXECUTABLE python3 REQUIRED)

function(process_asset asset_file output_file_var generated_dir)
    get_filename_component(asset_name "${asset_file}" NAME)
    set(output_file "${generated_dir}/assets/${asset_name}.h")
    set("${output_file_var}" "${output_file}" PARENT_SCOPE)

    add_custom_command(
        OUTPUT "${output_file}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${generated_dir}/assets"
        COMMAND "${PYTHON_EXECUTABLE}" "${R3D_ROOT_PATH}/scripts/bin2c.py" 
                --file "${asset_file}" --name "${asset_name}" --mode binary "${output_file}"
        DEPENDS "${asset_file}"
        COMMENT "Processing asset: ${asset_file}"
        VERBATIM
    )
endfunction()

function(embed_assets target_name)
    set(asset_files ${ARGN})

    if(NOT asset_files)
        message(FATAL_ERROR "embed_assets: No asset file specified")
    endif()

    if(NOT DEFINED R3D_GENERATED_INCLUDE_DIR)
        message(FATAL_ERROR "embed_assets: R3D_GENERATED_INCLUDE_DIR is not defined")
    endif()

    set(output_files)
    list(LENGTH asset_files num_assets)
    message(STATUS "Configuring processing of ${num_assets} asset(s) for target ${target_name}...")

    foreach(asset_file ${asset_files})
        if(NOT EXISTS "${asset_file}")
            message(FATAL_ERROR "embed_assets: Asset file not found: ${asset_file}")
        endif()

        get_filename_component(asset_name "${asset_file}" NAME)
        process_asset("${asset_file}" output_file "${R3D_GENERATED_INCLUDE_DIR}")
        list(APPEND output_files "${output_file}")
        message(STATUS "  - ${asset_file} -> assets/${asset_name}.h")
    endforeach()

    set(assets_target "${target_name}_assets")
    add_custom_target(${assets_target}
        DEPENDS ${output_files}
        COMMENT "Generating asset headers for ${target_name}"
    )

    add_dependencies(${target_name} ${assets_target})

    message(STATUS "Target ${assets_target} created with ${num_assets} asset(s)")
    message(STATUS "Assets will be automatically recompiled if modified")
endfunction()
