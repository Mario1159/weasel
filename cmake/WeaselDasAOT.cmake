# cmake/WeaselDasAOT.cmake
#
# Provides weasel_aot_das() function for AOT-compiling .das files
# into C++ source files using the standalone daslang binary.
#
# The standalone daslang needs a declarations file (weasel_api.das)
# in its daslib directory that declares the function signatures.
# This file is installed alongside the engine.

include(CMakeParseArguments)

# Capture the cmake module directory at file scope.
# CMAKE_CURRENT_LIST_DIR inside a function() reflects the *caller's* file,
# not the file where the function is defined.  Store it here while the file
# is being included so the lookup functions can use the correct base path.
get_filename_component(_WEASEL_CMAKE_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

# Find the daslang binary. Searches PATH first, then falls back to
# the Weasel export variable.
function(_weasel_find_daslang)
    if(DASLANG_TOOL)
        return()
    endif()
    find_program(DASLANG_TOOL daslang)
    if(NOT DASLANG_TOOL AND DEFINED Weasel_DASLANG_TOOL)
        set(DASLANG_TOOL "${Weasel_DASLANG_TOOL}")
    endif()
    if(NOT DASLANG_TOOL)
        message(FATAL_ERROR
            "daslang binary not found on PATH. "
            "Cannot AOT-compile .das files. "
            "Install daScript or set Weasel_DASLANG_TOOL.")
    endif()
    set(DASLANG_TOOL "${DASLANG_TOOL}" PARENT_SCOPE)
endfunction()

# Find the daslib directory for AOT declarations.
# This is where weasel_api.das lives.
function(_weasel_find_daslib)
    if(WEASEL_DASLIB_DIR)
        return()
    endif()
    # Check engine source tree (for in-tree builds)
    set(_daslib_hint "${_WEASEL_CMAKE_MODULE_DIR}/../_deps/daslang-src/daslib")
    if(EXISTS "${_daslib_hint}/weasel_api.das")
        set(WEASEL_DASLIB_DIR "${_daslib_hint}" PARENT_SCOPE)
        return()
    endif()
    # Check installed location (lib/cmake/Weasel/../../../share/weasel/daslib)
    set(_daslib_hint "${_WEASEL_CMAKE_MODULE_DIR}/../../../share/weasel/daslib")
    if(EXISTS "${_daslib_hint}/weasel_api.das")
        set(WEASEL_DASLIB_DIR "${_daslib_hint}" PARENT_SCOPE)
        return()
    endif()
    # Check user's home daslib
    set(_daslib_hint "$ENV{HOME}/.local/daslib")
    if(EXISTS "${_daslib_hint}/weasel_api.das")
        set(WEASEL_DASLIB_DIR "${_daslib_hint}" PARENT_SCOPE)
        return()
    endif()
    message(FATAL_ERROR
        "weasel_api.das not found. Cannot AOT-compile .das files.")
endfunction()

# AOT-compile a list of .das files into .cpp files.
#
# weasel_aot_das(
#     FILES file1.das file2.das ...
#     OUTPUT_VAR <variable>       # variable name to receive generated .cpp paths
#     [TARGET <target>]           # custom target name (default: ${PROJECT_NAME}_aot)
# )
function(weasel_aot_das)
    cmake_parse_arguments(AOT "" "OUTPUT_VAR;TARGET" "FILES" ${ARGN})

    if(NOT AOT_FILES)
        message(FATAL_ERROR "weasel_aot_das: FILES is required")
    endif()
    if(NOT AOT_OUTPUT_VAR)
        message(FATAL_ERROR "weasel_aot_das: OUTPUT_VAR is required")
    endif()

    _weasel_find_daslang()
    _weasel_find_daslib()

    if(NOT AOT_TARGET)
        set(AOT_TARGET ${PROJECT_NAME}_aot)
    endif()

    set(_GeneratedSrcs "")
    set(_OutputDir "${CMAKE_CURRENT_BINARY_DIR}/das_aot")

    foreach(_DasFile IN LISTS AOT_FILES)
        get_filename_component(_AbsFile "${_DasFile}" ABSOLUTE)
        get_filename_component(_Name "${_DasFile}" NAME_WE)
        set(_OutFile "${_OutputDir}/${_Name}.das.cpp")
        set(_TmpDir "${_OutputDir}/_tmp_${_Name}")

        # daScript -aot mode only resolves modules from the input file's directory.
        # Create a temp dir with the input file + weasel_api.das + daslib/weasel_ecs.das.
        add_custom_command(
            OUTPUT "${_OutFile}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_TmpDir}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_TmpDir}/daslib"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_AbsFile}" "${_TmpDir}/${_Name}.das"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${WEASEL_DASLIB_DIR}/weasel_api.das"
                    "${_TmpDir}/weasel_api.das"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${WEASEL_DASLIB_DIR}/weasel_ecs.das"
                    "${_TmpDir}/daslib/weasel_ecs.das"
            COMMAND "${DASLANG_TOOL}" -aot
                    "${_TmpDir}/${_Name}.das" "${_OutFile}"
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${_TmpDir}"
            DEPENDS "${_AbsFile}"
                    "${WEASEL_DASLIB_DIR}/weasel_api.das"
                    "${WEASEL_DASLIB_DIR}/weasel_ecs.das"
            COMMENT "AOT-compiling ${_DasFile} -> ${_Name}.das.cpp"
            VERBATIM
        )

        list(APPEND _GeneratedSrcs "${_OutFile}")
    endforeach()

    add_custom_target(${AOT_TARGET} DEPENDS ${_GeneratedSrcs})
    set(${AOT_OUTPUT_VAR} "${_GeneratedSrcs}" PARENT_SCOPE)
endfunction()
