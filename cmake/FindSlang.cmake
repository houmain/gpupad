# Prevent re-defining the package target
if(TARGET Slang::Slang)
  return()
endif()

# On Windows, we use the DXC compiler for Slang
if(WIN32)
    # Prioritize the DXC installed at ${DXC_ROOT}
    if(DEFINED DXC_ROOT)
        find_program(DXC_LIBRARY
            dxc
            NO_DEFAULT_PATH
            PATHS "${DXC_ROOT}/bin"
            DOC "Path to DXC executable"
        )
    endif()
    find_program(DXC_LIBRARY
        dxc
        DOC "Path to DXC executable"
    )
    get_filename_component(DXC_LIBRARY_DIR ${DXC_LIBRARY} DIRECTORY)
    message(STATUS "Found DXC: ${DXC_LIBRARY_DIR}")
    mark_as_advanced(DXC_LIBRARY_DIR)
endif()

if(WIN32)
    set(Slang_BUILD_CONFIG "windows-x64")
elseif(LINUX)
    set(Slang_BUILD_CONFIG "linux-x64")
elseif(APPLE)
    set(Slang_BUILD_CONFIG "macosx-aarch64")
endif()

# Find the Slang include path
if (DEFINED Slang_ROOT)
    # Prioritize the Slang installed at ${Slang_ROOT}
    find_path(Slang_INCLUDE_DIR      # Set variable Slang_INCLUDE_DIR
              slang.h                # Find a path with Slang.h
              NO_DEFAULT_PATH
              PATHS "${Slang_ROOT}"
              DOC "path to Slang header files"
    )
    find_library(Slang_LIBRARY       # Set variable Slang_LIBRARY
                 slang               # Find library path with libslang.so, slang.dll, or slang.lib
                 NO_DEFAULT_PATH
                 PATHS "${Slang_ROOT}/bin/${Slang_BUILD_CONFIG}"
                 PATH_SUFFIXES release debug
                 DOC "path to slang library files"
    )
    find_program(Slang_COMPILER       # Set variable Slang_COMPILER
                 slangc               # Find executable path with slangc
                 NO_DEFAULT_PATH
                 PATHS "${Slang_ROOT}/bin/${Slang_BUILD_CONFIG}"
                 PATH_SUFFIXES release debug
                 DOC "path to slangc compiler executable"
    )
endif()
# Once the prioritized find_path succeeds the result variable will be set and stored in the cache
# so that no call will search again.
find_path(Slang_INCLUDE_DIR      # Set variable Slang_INCLUDE_DIR
          slang.h                # Find a path with Slang.h
          DOC "path to Slang header files"
)
find_library(Slang_LIBRARY       # Set variable Slang_LIBRARY
             slang               # Find library path with libslang.so, or slang.lib
             DOC "path to slang library files"
)
find_program(Slang_COMPILER       # Set variable Slang_COMPILER
             slangc               # Find library path with slangc
             DOC "path to slangc compiler executable"
)
 
set(Slang_LIBRARIES ${Slang_LIBRARY})
set(Slang_INCLUDE_DIRS ${Slang_INCLUDE_DIR})

if(WIN32)
    # Find library path with slang.dll. Note that find_library can only get .lib files on Windows.
    find_file(Slang_DLL
              NAMES slang.dll
              HINTS "${Slang_INCLUDE_DIR}/../bin" # HINTS is used to search in the same directory as the include files
              DOC "path to slang dynamic library files"
    )
    get_filename_component(Slang_DLL_DIR ${Slang_DLL} DIRECTORY)
    file(GLOB Slang_LIBRARY_DLL ${Slang_DLL_DIR}/*.dll)
endif()

include(FindPackageHandleStandardArgs)
# Handle the QUIETLY and REQUIRED arguments and set Slang_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(Slang
                                  DEFAULT_MSG
                                  Slang_INCLUDE_DIRS
                                  Slang_LIBRARIES
)

if(Slang_FOUND)
    message(STATUS "Found Slang library: ${Slang_LIBRARY}")
    add_library(Slang::Slang UNKNOWN IMPORTED)
 	set_target_properties(Slang::Slang PROPERTIES
		IMPORTED_LOCATION ${Slang_LIBRARY}
		IMPORTED_LINK_INTERFACE_LIBRARIES "${Slang_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${Slang_INCLUDE_DIRS}"
    )
endif()

mark_as_advanced(
    Slang_INCLUDE_DIRS
    Slang_INCLUDE_DIR
    Slang_LIBRARIES
    Slang_LIBRARY
    Slang_LIBRARY_DLL
)

