if(CMAKE_SCRIPT_MODE_FILE)
    foreach(required_var
        FETCH_SPARSE_SOURCE_DIR
        FETCH_SPARSE_GIT_REPOSITORY
        FETCH_SPARSE_GIT_TAG)
        if(NOT DEFINED ${required_var} OR "${${required_var}}" STREQUAL "")
            message(FATAL_ERROR "${required_var} is required")
        endif()
    endforeach()

    find_package(Git REQUIRED)

    get_filename_component(source_dir_abs "${FETCH_SPARSE_SOURCE_DIR}" ABSOLUTE)
    file(TO_CMAKE_PATH "${source_dir_abs}" source_dir_abs)
    string(TOLOWER "${source_dir_abs}" source_dir_lower)

    if(DEFINED FETCH_SPARSE_PROJECT_SOURCE_DIR AND NOT "${FETCH_SPARSE_PROJECT_SOURCE_DIR}" STREQUAL "")
        get_filename_component(project_source_dir_abs "${FETCH_SPARSE_PROJECT_SOURCE_DIR}" ABSOLUTE)
        file(TO_CMAKE_PATH "${project_source_dir_abs}" project_source_dir_abs)
        string(TOLOWER "${project_source_dir_abs}" project_source_dir_lower)
        if(source_dir_lower STREQUAL project_source_dir_lower)
            message(FATAL_ERROR "Refusing to fetch into the project source directory: ${source_dir_abs}")
        endif()
    endif()

    if(DEFINED FETCH_SPARSE_ALLOWED_SOURCE_ROOT AND NOT "${FETCH_SPARSE_ALLOWED_SOURCE_ROOT}" STREQUAL "")
        get_filename_component(allowed_source_root_abs "${FETCH_SPARSE_ALLOWED_SOURCE_ROOT}" ABSOLUTE)
        file(TO_CMAKE_PATH "${allowed_source_root_abs}" allowed_source_root_abs)
        string(TOLOWER "${allowed_source_root_abs}" allowed_source_root_lower)
        string(FIND "${source_dir_lower}/" "${allowed_source_root_lower}/" allowed_root_pos)
        if(NOT allowed_root_pos EQUAL 0)
            message(FATAL_ERROR "Refusing to fetch outside ${allowed_source_root_abs}: ${source_dir_abs}")
        endif()
    endif()

    if(DEFINED FETCH_SPARSE_PATTERNS)
        string(REPLACE "|" ";" sparse_patterns "${FETCH_SPARSE_PATTERNS}")
    else()
        set(sparse_patterns)
    endif()

    set(git_filter_args)
    if(DEFINED FETCH_SPARSE_GIT_FILTER AND NOT "${FETCH_SPARSE_GIT_FILTER}" STREQUAL "")
        list(APPEND git_filter_args "--filter=${FETCH_SPARSE_GIT_FILTER}")
    endif()

    function(fetch_sparse_run_git)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" ${ARGN}
            WORKING_DIRECTORY "${FETCH_SPARSE_SOURCE_DIR}"
            RESULT_VARIABLE result)
        if(NOT result EQUAL 0)
            string(REPLACE ";" " " command_line "${ARGN}")
            message(FATAL_ERROR "git ${command_line} failed with exit code ${result}")
        endif()
    endfunction()

    if(NOT EXISTS "${FETCH_SPARSE_SOURCE_DIR}/.git")
        file(REMOVE_RECURSE "${FETCH_SPARSE_SOURCE_DIR}")
        get_filename_component(parent_dir "${FETCH_SPARSE_SOURCE_DIR}" DIRECTORY)
        file(MAKE_DIRECTORY "${parent_dir}")

        execute_process(
            COMMAND "${GIT_EXECUTABLE}" clone
                ${git_filter_args}
                --no-checkout
                "${FETCH_SPARSE_GIT_REPOSITORY}"
                "${FETCH_SPARSE_SOURCE_DIR}"
            RESULT_VARIABLE result)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "git clone failed with exit code ${result}")
        endif()
    endif()

    fetch_sparse_run_git(config remote.origin.url "${FETCH_SPARSE_GIT_REPOSITORY}")
    fetch_sparse_run_git(config advice.detachedHead false)

    if(sparse_patterns)
        fetch_sparse_run_git(sparse-checkout init --no-cone)
        fetch_sparse_run_git(sparse-checkout set --no-cone ${sparse_patterns})
    else()
        fetch_sparse_run_git(sparse-checkout disable)
    endif()

    fetch_sparse_run_git(fetch ${git_filter_args} --no-tags origin "${FETCH_SPARSE_GIT_TAG}")
    fetch_sparse_run_git(checkout --detach FETCH_HEAD)
    return()
endif()

include_guard(GLOBAL)
include(FetchContent)

function(fetch_sparse_normalize_path out_var path)
    string(REPLACE "\\" "/" normalized "${path}")
    string(REGEX REPLACE "^/+" "" normalized "${normalized}")
    string(REGEX REPLACE "/+$" "" normalized "${normalized}")
    set(${out_var} "/${normalized}/" PARENT_SCOPE)
endfunction()

function(fetch_sparse_parse_cache_option name out_name out_type out_value cache_option)
    if(NOT "${cache_option}" MATCHES "^([^:=]+)(:([^:=]+))?=(.*)$")
        message(FATAL_ERROR
            "Invalid CACHE_OPTIONS entry for FetchSparse(${name}): ${cache_option}. "
            "Expected NAME:TYPE=VALUE or NAME=VALUE.")
    endif()

    set(cache_option_name "${CMAKE_MATCH_1}")
    set(cache_option_type "${CMAKE_MATCH_3}")
    set(cache_option_value "${CMAKE_MATCH_4}")
    if(cache_option_type STREQUAL "")
        set(cache_option_type STRING)
    endif()

    set(${out_name} "${cache_option_name}" PARENT_SCOPE)
    set(${out_type} "${cache_option_type}" PARENT_SCOPE)
    set(${out_value} "${cache_option_value}" PARENT_SCOPE)
endfunction()

function(FetchSparse name)
    set(one_value_args
        FIND_PACKAGE
        GIT_REPOSITORY
        GIT_TAG
        GIT_FILTER
        SOURCE_SUBDIR)
    set(multi_value_args
        CACHE_OPTIONS
        SPARSE_EXCLUDE)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "${one_value_args}" "${multi_value_args}")

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown arguments for FetchSparse(${name}): ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    if(ARG_FIND_PACKAGE)
        find_package(${name} QUIET)
        set(find_package_found_var "${name}_FOUND")
        if(DEFINED ${find_package_found_var})
            set(find_package_found "${${find_package_found_var}}")
        else()
            set(find_package_found FALSE)
        endif()

        if(find_package_found)
            message(STATUS "FetchSparse(${name}): using package ${name}")
            return()
        endif()
    endif()

    if(NOT ARG_GIT_REPOSITORY)
        message(FATAL_ERROR "FetchSparse(${name}) requires GIT_REPOSITORY")
    endif()
    if(NOT ARG_GIT_TAG)
        message(FATAL_ERROR "FetchSparse(${name}) requires GIT_TAG")
    endif()
    if(NOT DEFINED ARG_GIT_FILTER)
        set(ARG_GIT_FILTER "blob:none")
    endif()

    set(sparse_patterns)
    if(ARG_SPARSE_EXCLUDE)
        list(APPEND sparse_patterns "/*")
        foreach(excluded_path IN LISTS ARG_SPARSE_EXCLUDE)
            fetch_sparse_normalize_path(excluded_pattern "${excluded_path}")
            list(APPEND sparse_patterns "!${excluded_pattern}")
        endforeach()
    endif()
    string(REPLACE ";" "|" sparse_patterns_arg "${sparse_patterns}")

    set(fetch_command
        ${CMAKE_COMMAND}
        "-DFETCH_SPARSE_SOURCE_DIR=<SOURCE_DIR>"
        "-DFETCH_SPARSE_PROJECT_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        "-DFETCH_SPARSE_ALLOWED_SOURCE_ROOT=${FETCHCONTENT_BASE_DIR}"
        "-DFETCH_SPARSE_GIT_REPOSITORY=${ARG_GIT_REPOSITORY}"
        "-DFETCH_SPARSE_GIT_TAG=${ARG_GIT_TAG}"
        "-DFETCH_SPARSE_GIT_FILTER=${ARG_GIT_FILTER}"
        "-DFETCH_SPARSE_PATTERNS=${sparse_patterns_arg}"
        -P "${CMAKE_CURRENT_FUNCTION_LIST_FILE}")

    set(fetch_content_args)
    if(ARG_SOURCE_SUBDIR)
        list(APPEND fetch_content_args SOURCE_SUBDIR "${ARG_SOURCE_SUBDIR}")
    endif()

    FetchContent_Declare(${name}
        DOWNLOAD_COMMAND ${fetch_command}
        UPDATE_COMMAND ${fetch_command}
        ${fetch_content_args})

    set(cache_option_count 0)
    foreach(cache_option IN LISTS ARG_CACHE_OPTIONS)
        fetch_sparse_parse_cache_option(${name}
            cache_option_name
            cache_option_type
            cache_option_value
            "${cache_option}")

        math(EXPR cache_option_count "${cache_option_count} + 1")
        set(cache_option_${cache_option_count}_name "${cache_option_name}")
        if(DEFINED CACHE{${cache_option_name}})
            set(cache_option_${cache_option_count}_had_cache TRUE)
            get_property(cache_option_${cache_option_count}_value CACHE "${cache_option_name}" PROPERTY VALUE)
            get_property(cache_option_${cache_option_count}_type CACHE "${cache_option_name}" PROPERTY TYPE)
            get_property(cache_option_${cache_option_count}_help CACHE "${cache_option_name}" PROPERTY HELPSTRING)
            get_property(cache_option_${cache_option_count}_advanced CACHE "${cache_option_name}" PROPERTY ADVANCED)
        else()
            set(cache_option_${cache_option_count}_had_cache FALSE)
        endif()

        set(${cache_option_name} "${cache_option_value}" CACHE ${cache_option_type}
            "FetchSparse ${name} option ${cache_option_name}" FORCE)
    endforeach()

    FetchContent_MakeAvailable(${name})

    if(cache_option_count GREATER 0)
        foreach(cache_option_index RANGE 1 ${cache_option_count})
            set(cache_option_name "${cache_option_${cache_option_index}_name}")
            if(cache_option_${cache_option_index}_had_cache)
                set(${cache_option_name}
                    "${cache_option_${cache_option_index}_value}"
                    CACHE ${cache_option_${cache_option_index}_type}
                    "${cache_option_${cache_option_index}_help}"
                    FORCE)
                if(cache_option_${cache_option_index}_advanced)
                    mark_as_advanced(FORCE ${cache_option_name})
                else()
                    mark_as_advanced(CLEAR ${cache_option_name})
                endif()
            else()
                unset(${cache_option_name} CACHE)
            endif()
        endforeach()
    endif()
endfunction()
