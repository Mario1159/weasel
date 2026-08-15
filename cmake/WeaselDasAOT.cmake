# cmake/WeaselDasAOT.cmake
#
# Provides weasel_aot_das() for AOT-compiling .das files into C++ source
# files using the engine's own module environment (Module_WeaselApi,
# Module_Ecs) loaded in-process via `weasel-cli aot`.
#
# Because the AOT generation runs inside the engine process, `require
# weasel_api` resolves to the real C++ module (not a hand-written stub), so
# the emitted C++ references the real proxy types and actually runs as AOT
# instead of silently falling back to the interpreter.

include(CMakeParseArguments)

# Find the weasel-cli tool. Prefers the in-tree build target; falls back to
# a installed `weasel-cli` on PATH.
function(_weasel_find_cli)
    if(WEASEL_CLI_TOOL)
        return()
    endif()
    if(TARGET weasel-cli)
        set(WEASEL_CLI_TOOL "$<TARGET_FILE:weasel-cli>" PARENT_SCOPE)
        return()
    endif()
    find_program(WEASEL_CLI_TOOL weasel-cli)
    if(NOT WEASEL_CLI_TOOL)
        message(FATAL_ERROR
            "weasel-cli not found (build target or on PATH). "
            "Cannot AOT-compile .das files.")
    endif()
    set(WEASEL_CLI_TOOL "${WEASEL_CLI_TOOL}" PARENT_SCOPE)
endfunction()

# AOT-compile a list of .das files into .cpp files.
#
# weasel_aot_das(
#     FILES file1.das file2.das ...
#     OUTPUT_VAR <variable>       # variable name to receive generated .cpp paths
#     [TARGET <target>]           # custom target name (default: ${PROJECT_NAME}_aot)
#     [COMPONENTS comp1.das comp2.das ...]  # component files whose directories
#                                            # are added as `require` search roots
# )
function(weasel_aot_das)
    cmake_parse_arguments(AOT "" "OUTPUT_VAR;TARGET" "FILES;COMPONENTS" ${ARGN})

    if(NOT AOT_FILES)
        message(FATAL_ERROR "weasel_aot_das: FILES is required")
    endif()
    if(NOT AOT_OUTPUT_VAR)
        message(FATAL_ERROR "weasel_aot_das: OUTPUT_VAR is required")
    endif()

    _weasel_find_cli()

    # Depend on the actual tool. In an in-tree build `weasel-cli` is a build
    # target, so depend on its output file (rebuilds the CLI first). In a
    # standalone build (find_package) it is an installed program, so depend on
    # the resolved path instead — referencing the bare target name there has no
    # rule and breaks the build.
    if(TARGET weasel-cli)
        set(_CliDep "$<TARGET_FILE:weasel-cli>")
    else()
        set(_CliDep "${WEASEL_CLI_TOOL}")
    endif()

    if(NOT AOT_TARGET)
        set(AOT_TARGET ${PROJECT_NAME}_aot)
    endif()

    # Collect unique component directories as `-I` search roots so game
    # modules (`require mouse_rotate`, etc.) resolve during in-process AOT.
    set(_IncludeDirs "")
    foreach(_CompFile IN LISTS AOT_COMPONENTS)
        get_filename_component(_CompDir "${_CompFile}" DIRECTORY)
        list(APPEND _IncludeDirs "${_CompDir}")
    endforeach()
    list(REMOVE_DUPLICATES _IncludeDirs)

    set(_IncludeArgs "")
    foreach(_Dir IN LISTS _IncludeDirs)
        list(APPEND _IncludeArgs "-I" "${_Dir}")
    endforeach()

    set(_GeneratedSrcs "")
    set(_OutputDir "${CMAKE_CURRENT_BINARY_DIR}/das_aot")

    foreach(_DasFile IN LISTS AOT_FILES)
        get_filename_component(_AbsFile "${_DasFile}" ABSOLUTE)
        get_filename_component(_Name "${_DasFile}" NAME_WE)
        set(_OutFile "${_OutputDir}/${_Name}.das.cpp")

        add_custom_command(
            OUTPUT "${_OutFile}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_OutputDir}"
            COMMAND "${WEASEL_CLI_TOOL}" aot "${_AbsFile}" "${_OutFile}"
                    ${_IncludeArgs}
            DEPENDS "${_AbsFile}" ${AOT_COMPONENTS} "${_CliDep}"
            COMMENT "AOT-compiling ${_DasFile} -> ${_Name}.das.cpp (in-process, real module)"
            VERBATIM
        )

        list(APPEND _GeneratedSrcs "${_OutFile}")
    endforeach()

    add_custom_target(${AOT_TARGET} DEPENDS ${_GeneratedSrcs})
    set(${AOT_OUTPUT_VAR} "${_GeneratedSrcs}" PARENT_SCOPE)
endfunction()
