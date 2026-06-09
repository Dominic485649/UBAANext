if(NOT DEFINED UBAANEXT_SOURCE_DIR)
    message(FATAL_ERROR "UBAANEXT_SOURCE_DIR is required")
endif()

cmake_path(NORMAL_PATH UBAANEXT_SOURCE_DIR)

function(ubaanext_remove_path path)
    if(EXISTS "${path}")
        file(REMOVE_RECURSE "${path}")
        message(STATUS "Removed ${path}")
    endif()
endfunction()

function(ubaanext_clean_build_tree build_tree)
    if(NOT IS_DIRECTORY "${build_tree}")
        return()
    endif()

    file(GLOB build_entries LIST_DIRECTORIES true "${build_tree}/*")
    set(has_vcpkg_installed OFF)
    foreach(entry IN LISTS build_entries)
        get_filename_component(entry_name "${entry}" NAME)
        if(entry_name STREQUAL "vcpkg_installed")
            set(has_vcpkg_installed ON)
        endif()
    endforeach()

    if(NOT has_vcpkg_installed)
        ubaanext_remove_path("${build_tree}")
        return()
    endif()

    foreach(entry IN LISTS build_entries)
        get_filename_component(entry_name "${entry}" NAME)
        if(entry_name STREQUAL "vcpkg_installed")
            message(STATUS "Kept external dependency cache ${entry}")
        else()
            ubaanext_remove_path("${entry}")
        endif()
    endforeach()
endfunction()

set(root_artifacts
    "${UBAANEXT_SOURCE_DIR}/CMakeCache.txt"
    "${UBAANEXT_SOURCE_DIR}/CMakeFiles"
    "${UBAANEXT_SOURCE_DIR}/cmake_install.cmake"
    "${UBAANEXT_SOURCE_DIR}/CTestTestfile.cmake"
    "${UBAANEXT_SOURCE_DIR}/DartConfiguration.tcl"
    "${UBAANEXT_SOURCE_DIR}/Testing"
    "${UBAANEXT_SOURCE_DIR}/build.ninja"
    "${UBAANEXT_SOURCE_DIR}/rules.ninja"
    "${UBAANEXT_SOURCE_DIR}/.ninja_deps"
    "${UBAANEXT_SOURCE_DIR}/.ninja_log"
    "${UBAANEXT_SOURCE_DIR}/compile_commands.json"
)
foreach(artifact IN LISTS root_artifacts)
    ubaanext_remove_path("${artifact}")
endforeach()

file(GLOB_RECURSE cli_outputs LIST_DIRECTORIES false
    "${UBAANEXT_SOURCE_DIR}/bin/*/*/ubaa"
    "${UBAANEXT_SOURCE_DIR}/bin/*/*/ubaa.exe"
    "${UBAANEXT_SOURCE_DIR}/bin/*/*/ubaa.com"
    "${UBAANEXT_SOURCE_DIR}/bin/*/*/ubaa.pdb"
    "${UBAANEXT_SOURCE_DIR}/bin/*/*/ubaa.ilk"
    "${UBAANEXT_SOURCE_DIR}/bin/*/*/ubaa.exe.sha256"
    "${UBAANEXT_SOURCE_DIR}/bin/*/*/ubaa.com.sha256"
    "${UBAANEXT_SOURCE_DIR}/bin/*/*/ubaa-gui"
    "${UBAANEXT_SOURCE_DIR}/bin/*/*/ubaa-gui.sha256"
)
foreach(output IN LISTS cli_outputs)
    ubaanext_remove_path("${output}")
endforeach()

set(build_root "${UBAANEXT_SOURCE_DIR}/build")
if(IS_DIRECTORY "${build_root}")
    file(GLOB build_entries LIST_DIRECTORIES true "${build_root}/*")
    foreach(entry IN LISTS build_entries)
        if(IS_DIRECTORY "${entry}")
            ubaanext_clean_build_tree("${entry}")
        else()
            ubaanext_remove_path("${entry}")
        endif()
    endforeach()
endif()
