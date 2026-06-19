cmake_minimum_required(VERSION 3.22)
project(gpupad LANGUAGES CXX C)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(FetchSparse)

set(BUILD_TYPES "Debug" "Release")
get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(multi_config)
    set(CMAKE_CONFIGURATION_TYPES "${BUILD_TYPES}" CACHE STRING "list of supported configuration types" FORCE)
elseif(NOT CMAKE_BUILD_TYPE OR NOT CMAKE_BUILD_TYPE IN_LIST BUILD_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

option(ENABLE_UNITY_BUILD "Enable Unity Build" OFF)
if (ENABLE_UNITY_BUILD)
    set(CMAKE_UNITY_BUILD TRUE)
endif()

if (MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W1 /MP)
    add_compile_options($<$<CONFIG:Release>:/GL>)
    add_compile_options($<$<CONFIG:Release>:/GR->)
    add_link_options($<$<CONFIG:Release>:/LTCG>)
else()
    add_compile_options($<$<CONFIG:Release>:-fno-rtti>)

    # use mold linker when available
    find_program(MOLD_PATH NAMES mold)
    if(MOLD_PATH)
        message(NOTICE "Using mold linker")
        add_link_options(-fuse-ld=mold)
    endif()

    # use ccache when available
    find_program(CCACHE_PATH NAMES ccache)
    if(CCACHE_PATH)
        message(NOTICE "Using ccache")
        set(CMAKE_C_COMPILER_LAUNCHER "ccache")
        set(CMAKE_CXX_COMPILER_LAUNCHER "ccache")
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core)

if(Qt6_VERSION VERSION_GREATER_EQUAL 6.3)
    qt_standard_project_setup()
else()
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    include(GNUInstallDirs)
endif()
set(CMAKE_AUTORCC ON)

# setup version -------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(GIT_COMMIT_LAST_TAG "0.0.0")
find_package(Git)
if(NOT VERSION AND GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE VERSION
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

configure_file("src/version.h.in" "${CMAKE_SOURCE_DIR}/src/_version.h")

# -------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DQT_NO_CAST_TO_ASCII
                #-DQT_NO_CAST_FROM_ASCII
                -DQT_NO_URL_CAST_FROM_STRING
                -DQT_NO_CAST_FROM_BYTEARRAY
                -DQT_NO_SIGNALS_SLOTS_KEYWORDS
                -DQT_USE_QSTRINGBUILDER
                -DQT_NO_NARROWING_CONVERSIONS_IN_CONNECT
                #-DQT_STRICT_ITERATORS
                -DQT_NO_KEYWORDS
                -DQT_DISABLE_DEPRECATED_BEFORE=0x060500
                -DQT_NO_FOREACH)

set(SOURCES
  libs/SingleApplication/singleapplication.cpp
  libs/SingleApplication/singleapplication_p.cpp
  src/FileDialog.cpp
  src/FileCache.cpp
  src/MessageList.cpp
  src/Settings.cpp
  src/Singletons.cpp
  src/SynchronizeLogic.cpp
  src/TextureData.cpp
  src/VideoPlayer.cpp
  src/VideoManager.cpp
  src/InputState.cpp
  src/SourceType.cpp
  src/Style.cpp
  src/Theme.cpp
  src/windows/AboutDialog.cpp
  src/windows/FileBrowserWindow.cpp
  src/windows/MainWindow.cpp
  src/windows/MainWindow.ui
  src/windows/MessageWindow.cpp
  src/windows/OutputWindow.cpp
  src/windows/WindowTitle.cpp
  src/editors/DockWindow.cpp
  src/editors/DockTitle.cpp
  src/editors/EditorManager.cpp
  src/editors/binary/BinaryEditor.cpp
  src/editors/binary/BinaryEditorToolBar.cpp
  src/editors/binary/BinaryEditorToolBar.ui
  src/editors/source/Completer.cpp
  src/editors/source/FindReplaceBar.cpp
  src/editors/source/FindReplaceBar.ui
  src/editors/source/MultiTextCursors.cpp
  src/editors/source/SourceEditor.cpp
  src/editors/source/SourceEditorToolBar.cpp
  src/editors/source/SourceEditorToolBar.ui
  src/editors/source/SyntaxGeneric.cpp
  src/editors/source/SyntaxGLSL.cpp
  src/editors/source/SyntaxHighlighter.cpp
  src/editors/source/SyntaxHLSL.cpp
  src/editors/source/SyntaxSlang.cpp
  src/editors/source/SyntaxJavaScript.cpp
  src/editors/texture/TextureEditor.cpp
  src/editors/texture/TextureEditorToolBar.cpp
  src/editors/texture/TextureEditorToolBar.ui
  src/editors/texture/TextureInfoBar.cpp
  src/editors/texture/TextureInfoBar.ui
  src/editors/texture/TextureEditorBackground.cpp
  src/editors/texture/TextureEditorItem.cpp
  src/editors/qml/QmlView.cpp
  src/main.cpp
  src/render/Renderer.cpp
  src/render/ProcessSource.cpp
  src/render/RenderTask.cpp
  src/render/RenderSessionBase.cpp
  src/render/ShaderBase.cpp
  src/render/BufferBase.cpp
  src/render/PipelineBase.cpp
  src/render/TextureBase.cpp
  src/render/PrintfBase.cpp
  src/render/ShaderCompiler.cpp
  src/render/ShaderCompiler_glslang.cpp
  src/render/Reflection.cpp
  src/render/Reflection_Builder.cpp
  src/render/Reflection_JSON.cpp
  src/scripting/ScriptEngine.cpp
  src/scripting/ScriptEngine.js
  src/scripting/ScriptSession.cpp
  src/scripting/ScriptTimeout.cpp
  src/scripting/CustomActions.cpp
  src/scripting/objects/AppScriptObject.cpp
  src/scripting/objects/AppScriptObject_Session.cpp
  src/scripting/objects/ConsoleScriptObject.cpp
  src/scripting/objects/EditorScriptObject.cpp
  src/scripting/objects/LibraryScriptObject.cpp
  src/scripting/objects/MouseScriptObject.cpp
  src/scripting/objects/KeyboardScriptObject.cpp
  src/scripting/objects/ItemScriptObject.cpp
  src/session/Item.cpp
  src/resources.qrc
  src/session/properties/AccelerationStructureProperties.ui
  src/session/properties/AttachmentProperties.cpp
  src/session/properties/AttachmentProperties.ui
  src/session/properties/AttributeProperties.ui
  src/session/properties/BindingProperties.cpp
  src/session/properties/BindingProperties.ui
  src/session/properties/BlockProperties.ui
  src/session/properties/BufferProperties.ui
  src/session/properties/CallProperties.cpp
  src/session/properties/CallProperties.ui
  src/session/properties/FieldProperties.ui
  src/session/properties/GeometryProperties.ui
  src/session/properties/GroupProperties.ui
  src/session/properties/InstanceProperties.ui
  src/session/properties/ProgramProperties.ui
  src/session/properties/PropertiesEditor.cpp
  src/session/properties/ScriptProperties.ui
  src/session/properties/SessionProperties.cpp
  src/session/properties/SessionProperties.ui
  src/session/properties/ShaderProperties.ui
  src/session/properties/StreamProperties.ui
  src/session/properties/TargetProperties.ui
  src/session/properties/TextureProperties.cpp
  src/session/properties/TextureProperties.ui
  src/session/SessionEditor.cpp
  src/session/SessionModel.cpp
  src/session/SessionModelCore.cpp
  src/widgets/ColorMask.cpp
  src/widgets/ColorPicker.cpp
  src/widgets/DataComboBox.cpp
  src/widgets/ExpressionEditor.cpp
  src/widgets/ExpressionLineEdit.cpp
  src/widgets/ExpressionMatrix.cpp
  src/widgets/WindowWidget.cpp
  src/widgets/ReferenceComboBox.cpp)

if(WIN32)
  set(SOURCES ${SOURCES}
  src/windows/setWindowBorderTheme.cpp
  src/render/ShaderCompiler_Microsoft.cpp)
endif()

# OpenGL -------------------------
if (NOT APPLE)
   set(ENABLE_OPENGL_DEFAULT TRUE)
endif()
option(GPUPAD_ENABLE_OPENGL "Enable OpenGL" ${ENABLE_OPENGL_DEFAULT})
if (GPUPAD_ENABLE_OPENGL)
  set(SOURCES ${SOURCES}
  src/render/opengl/GLBuffer.cpp
  src/render/opengl/GLCall.cpp
  src/render/opengl/GLTextureEditorBackground.cpp
  src/render/opengl/GLTextureEditorItem.cpp
  src/render/opengl/GLProgram.cpp
  src/render/opengl/GLProgram_Reflection.cpp
  src/render/opengl/GLShader.cpp
  src/render/opengl/GLStream.cpp
  src/render/opengl/GLTarget.cpp
  src/render/opengl/GLTexture.cpp
  src/render/opengl/GLContext.cpp
  src/render/opengl/GLDevice.cpp
  src/render/opengl/GLPrintf.cpp
  src/render/opengl/GLRenderSession.cpp
  src/render/opengl/GLWindow.cpp)
endif()

# Vulkan -------------------------
find_package(Vulkan)
if (Vulkan_FOUND)
   set(ENABLE_VULKAN_DEFAULT TRUE)
endif()
option(GPUPAD_ENABLE_VULKAN "Enable Vulkan" ${ENABLE_VULKAN_DEFAULT})
if (GPUPAD_ENABLE_VULKAN)
  set(SOURCES ${SOURCES}
  src/render/vulkan/KDGpuEnums.cpp
  src/render/vulkan/VKBuffer.cpp
  src/render/vulkan/VKCall.cpp
  src/render/vulkan/VKPipeline.cpp
  src/render/vulkan/VKProgram.cpp
  src/render/vulkan/VKPrintf.cpp
  src/render/vulkan/VKShader.cpp
  src/render/vulkan/VKStream.cpp
  src/render/vulkan/VKTarget.cpp
  src/render/vulkan/VKTexture.cpp
  src/render/vulkan/VKDevice.cpp
  src/render/vulkan/VKTextureEditorBackground.cpp
  src/render/vulkan/VKTextureEditorItem.cpp
  src/render/vulkan/VKTextureEditorItem_GL.cpp
  src/render/vulkan/VKTextureEditorItem_Share.cpp
  src/render/vulkan/VKRenderSession.cpp
  src/render/vulkan/VKAccelerationStructure.cpp
  src/render/vulkan/VKWindow.cpp)
endif()

# D3D -------------------------
if (WIN32)
   set(ENABLE_D3D_DEFAULT TRUE)
endif()
option(GPUPAD_ENABLE_D3D "Enable D3D12" ${ENABLE_D3D_DEFAULT})
if (GPUPAD_ENABLE_D3D)
  set(SOURCES ${SOURCES}
  src/render/direct3d/D3DAccelerationStructure.cpp
  src/render/direct3d/D3DBuffer.cpp
  src/render/direct3d/D3DCall.cpp
  src/render/direct3d/D3DEnums.cpp
  src/render/direct3d/D3DPipeline.cpp
  src/render/direct3d/D3DProgram.cpp
  src/render/direct3d/D3DPrintf.cpp
  src/render/direct3d/D3DShader.cpp
  src/render/direct3d/D3DShader_Reflection.cpp
  src/render/direct3d/D3DStream.cpp
  src/render/direct3d/D3DTarget.cpp
  src/render/direct3d/D3DTexture.cpp
  src/render/direct3d/D3DDevice.cpp
  src/render/direct3d/D3DRenderSession.cpp
  libs/d3d12/RenderTargetHelper.cpp)
endif()

file(GLOB_RECURSE HEADERS include src/*.h)

if (MSVC)
    add_compile_options(/W3)
    
    set(HEADERS ${HEADERS} src/resources.rc src/qt6.natvis)
    
    # build binaries in bin directory
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/bin)
    
    # make GPUpad the startup project
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
else()
    add_compile_options(-Wall)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})

qt_add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

target_include_directories(${PROJECT_NAME} PRIVATE src libs)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# link Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets Qml)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Qml)

# build KTX -------------------------
set(PROJECT_VERSION "0.0.0")
set(CMAKE_FOLDER KTX)
set(GPUPAD_KTX_GIT_REPOSITORY "https://github.com/KhronosGroup/KTX-Software.git" CACHE STRING "KTX-Software Git repository")
set(GPUPAD_KTX_GIT_REVISION "26cddc8d99386eb4120b8416956d54661fef15c5" CACHE STRING "KTX-Software Git revision")
FetchSparse(KTXSoftware
    GIT_REPOSITORY ${GPUPAD_KTX_GIT_REPOSITORY}
    GIT_TAG ${GPUPAD_KTX_GIT_REVISION}
    SOURCE_SUBDIR lib
    CACHE_OPTIONS
        BUILD_TESTING:BOOL=OFF
        BUILD_SHARED_LIBS:BOOL=OFF
        LIBKTX_FEATURE_ETC_UNPACK:BOOL=OFF
        LIBKTX_VERSION_READ_ONLY:BOOL=OFF
        KTX_GIT_VERSION_FULL:STRING=v0.0.0-noversion
    SPARSE_EXCLUDE
        tests
        external/basis_universal/bin
        external/basis_universal/test_files
        external/basis_universal/python
        external/basis_universal/shader_deblocking
        external/basis_universal/webgl
        external/astc-encoder/Test
)
target_link_libraries(${PROJECT_NAME} PRIVATE ktx)

# link OpenGL -------------------------
if (GPUPAD_ENABLE_OPENGL)
    find_package(Qt6 REQUIRED COMPONENTS OpenGL)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::OpenGL)
    target_compile_definitions(${PROJECT_NAME} PRIVATE OPENGL_ENABLED)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(${PROJECT_NAME} PRIVATE GL)
    elseif(WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
    elseif(APPLE)
        find_package(OpenGL REQUIRED)
        include_directories(${OPENGL_INCLUDE_DIR})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_LIBRARIES})
    endif()
endif()

# build KDGpu -------------------------
if (GPUPAD_ENABLE_VULKAN)
    set(CMAKE_FOLDER KDGpu)
    set(GPUPAD_VMA_GIT_REPOSITORY "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git" CACHE STRING "VulkanMemoryAllocator Git repository")
    set(GPUPAD_VMA_GIT_REVISION "3aa921224c154a0d2c43912bc88e1c42ce1f7607" CACHE STRING "VulkanMemoryAllocator Git revision")
    FetchSparse(VulkanMemoryAllocator
        GIT_REPOSITORY ${GPUPAD_VMA_GIT_REPOSITORY}
        GIT_TAG ${GPUPAD_VMA_GIT_REVISION}
        CACHE_OPTIONS
            VMA_ENABLE_INSTALL:BOOL=OFF
        SPARSE_EXCLUDE
            docs
    )
    add_library(vulkan-memory-allocator::vulkan-memory-allocator ALIAS VulkanMemoryAllocator)

    set(GPUPAD_SPDLOG_GIT_REPOSITORY "https://github.com/gabime/spdlog.git" CACHE STRING "spdlog Git repository")
    set(GPUPAD_SPDLOG_GIT_REVISION "f544fce006850a58063277068948dcda5681a27c" CACHE STRING "spdlog Git revision")
    FetchSparse(spdlog
        GIT_REPOSITORY ${GPUPAD_SPDLOG_GIT_REPOSITORY}
        GIT_TAG ${GPUPAD_SPDLOG_GIT_REVISION}
        CACHE_OPTIONS
            SPDLOG_INSTALL:BOOL=OFF
            SPDLOG_BUILD_EXAMPLE:BOOL=OFF
            SPDLOG_BUILD_TESTS:BOOL=OFF
            SPDLOG_BUILD_BENCH:BOOL=OFF
            SPDLOG_FMT_EXTERNAL:BOOL=OFF
        SPARSE_EXCLUDE
            tests
    )

    include(FeatureSummary)
    add_compile_definitions(KDGPU_STATIC_DEFINE)
    set(CMAKE_SKIP_INSTALL_RULES TRUE)
    add_subdirectory(libs/KDGpu/src/KDGpu EXCLUDE_FROM_ALL)
    set(CMAKE_SKIP_INSTALL_RULES FALSE)
    target_include_directories(KDGpu PUBLIC "$<BUILD_INTERFACE:${spdlog_SOURCE_DIR}/include/spdlog>")

    target_link_libraries(${PROJECT_NAME} PRIVATE KDGpu spdlog::spdlog)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VULKAN_ENABLED)
endif()

# link D3D -------------------------
if (GPUPAD_ENABLE_D3D)
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        d3d11
        dxgi)
    target_compile_definitions(${PROJECT_NAME} PRIVATE D3D_ENABLED)
endif()

# link DXC -------------------------
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE d3dcompiler)

    find_package(directx-dxc CONFIG)
    option(GPUPAD_ENABLE_DXC "Enable DXC" ${directx-dxc_FOUND})
    if(GPUPAD_ENABLE_DXC)
        target_link_libraries(${PROJECT_NAME} PRIVATE 
            Microsoft::DirectXShaderCompiler)      
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE DXC_ENABLED)
endif()

find_package(Qt6Multimedia CONFIG)
option(GPUPAD_ENABLE_MULTIMEDIA "Enable QtMultimedia" ${Qt6Multimedia_FOUND})
if(GPUPAD_ENABLE_MULTIMEDIA)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Multimedia)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QtMultimedia_FOUND)
endif()

find_package(Qt6Quick CONFIG)
find_package(Qt6QuickWidgets CONFIG)
option(GPUPAD_ENABLE_QUICK "Enable QtQuick" ${Qt6QuickWidgets_FOUND})
if(GPUPAD_ENABLE_QUICK AND Qt6Quick_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Quick Qt6::QuickWidgets)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QtQuick_FOUND)

    find_package(Qt6QuickControls2 CONFIG)
    if(Qt6QuickControls2_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::QuickControls2)
        target_compile_definitions(${PROJECT_NAME} PRIVATE QtQuickControls2_FOUND)
    endif()
endif()

# link SPIRV-Tools
set(CMAKE_FOLDER SPIRV-Tools)
set(GPUPAD_SPIRV_HEADERS_GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Headers.git" CACHE STRING "SPIRV-Headers Git repository")
set(GPUPAD_SPIRV_HEADERS_GIT_REVISION "c63848ecf2200425511319fd8bf2c17b751e501e" CACHE STRING "SPIRV-Headers Git revision")
FetchSparse(SPIRV-Headers
    GIT_REPOSITORY ${GPUPAD_SPIRV_HEADERS_GIT_REPOSITORY}
    GIT_TAG ${GPUPAD_SPIRV_HEADERS_GIT_REVISION}
    CACHE_OPTIONS
        SPIRV_HEADERS_ENABLE_TESTS:BOOL=OFF
        SPIRV_HEADERS_ENABLE_INSTALL:BOOL=OFF
)
FetchContent_GetProperties(SPIRV-Headers SOURCE_DIR GPUPAD_SPIRV_HEADERS_SOURCE_DIR)

set(GPUPAD_SPIRV_TOOLS_GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Tools.git" CACHE STRING "SPIRV-Tools Git repository")
set(GPUPAD_SPIRV_TOOLS_GIT_REVISION "ee4f6beddb14cf6e9259288b86625405b4cdc4e1" CACHE STRING "SPIRV-Tools Git revision")
FetchSparse(SPIRV-Tools
    GIT_REPOSITORY ${GPUPAD_SPIRV_TOOLS_GIT_REPOSITORY}
    GIT_TAG ${GPUPAD_SPIRV_TOOLS_GIT_REVISION}
    CACHE_OPTIONS
        SPIRV-Headers_SOURCE_DIR:PATH=${GPUPAD_SPIRV_HEADERS_SOURCE_DIR}
        SKIP_SPIRV_TOOLS_INSTALL:BOOL=ON
        SPIRV_SKIP_EXECUTABLES:BOOL=ON
        SPIRV_SKIP_TESTS:BOOL=ON
        SPIRV_WERROR:BOOL=OFF
        SPIRV_TOOLS_USE_MIMALLOC:BOOL=OFF
)
target_link_libraries(${PROJECT_NAME} 
    PRIVATE SPIRV-Tools-opt)

# link SPIRV-Reflect
set(CMAKE_FOLDER SPIRV-Reflect)
set(GPUPAD_SPIRV_REFLECT_GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Reflect.git" CACHE STRING "SPIRV-Reflect Git repository")
set(GPUPAD_SPIRV_REFLECT_GIT_REVISION "c90b7b781cdcff63cf1b409ffc7ca0a714e0425e" CACHE STRING "SPIRV-Reflect Git revision")
FetchSparse(SPIRV-Reflect
    GIT_REPOSITORY ${GPUPAD_SPIRV_REFLECT_GIT_REPOSITORY}
    GIT_TAG ${GPUPAD_SPIRV_REFLECT_GIT_REVISION}
    CACHE_OPTIONS
        SPIRV_REFLECT_EXECUTABLE:BOOL=OFF
        SPIRV_REFLECT_INSTALL:BOOL=OFF
        SPIRV_REFLECT_STATIC_LIB:BOOL=ON
    SPARSE_EXCLUDE
        tests
        examples
)
target_link_libraries(${PROJECT_NAME} 
    PRIVATE spirv-reflect-static)

# link glslang
set(GPUPAD_GLSLANG_GIT_REPOSITORY "https://github.com/KhronosGroup/glslang.git" CACHE STRING "glslang Git repository")
set(GPUPAD_GLSLANG_GIT_REVISION "7801882058ce876e7708f941f0825a58eb94351a" CACHE STRING "glslang Git revision")
FetchSparse(glslang
    GIT_REPOSITORY ${GPUPAD_GLSLANG_GIT_REPOSITORY}
    GIT_TAG ${GPUPAD_GLSLANG_GIT_REVISION}
    CACHE_OPTIONS
        GLSLANG_TESTS:BOOL=OFF
        GLSLANG_ENABLE_INSTALL:BOOL=OFF
        ENABLE_GLSLANG_BINARIES:BOOL=OFF
        ENABLE_GLSLANG_JS:BOOL=OFF
        ENABLE_OPT:BOOL=ON
    SPARSE_EXCLUDE
        Test
)
target_link_libraries(${PROJECT_NAME} PRIVATE glslang::glslang
    glslang::glslang-default-resource-limits glslang::SPIRV)

# link SPIRV-Cross
set(CMAKE_FOLDER SPIRV-Cross)
set(GPUPAD_SPIRV_CROSS_GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Cross.git" CACHE STRING "SPIRV-Cross Git repository")
set(GPUPAD_SPIRV_CROSS_GIT_REVISION "146679ff8255a6068518685599d7fb8761d1b570" CACHE STRING "SPIRV-Cross Git revision")
FetchSparse(SPIRV-Cross
    GIT_REPOSITORY ${GPUPAD_SPIRV_CROSS_GIT_REPOSITORY}
    GIT_TAG ${GPUPAD_SPIRV_CROSS_GIT_REVISION}
    CACHE_OPTIONS
        SPIRV_CROSS_CLI:BOOL=OFF
        SPIRV_CROSS_ENABLE_TESTS:BOOL=OFF
        SPIRV_CROSS_SKIP_INSTALL:BOOL=ON
        SPIRV_CROSS_ENABLE_CPP:BOOL=OFF
        SPIRV_CROSS_ENABLE_MSL:BOOL=OFF
        SPIRV_CROSS_ENABLE_C_API:BOOL=OFF
        SPIRV_CROSS_ENABLE_UTIL:BOOL=OFF
    SPARSE_EXCLUDE
        reference
        shaders*
)
target_link_libraries(${PROJECT_NAME} 
    PRIVATE spirv-cross-core spirv-cross-glsl spirv-cross-hlsl spirv-cross-reflect)

# link OpenImageIO
find_package(OpenImageIO)
if (OpenImageIO_FOUND)
    set(ENABLE_OPENIMEAGEIO_DEFAULT TRUE)
endif()
option(GPUPAD_ENABLE_OPENIMEAGEIO "Enable OpenImageIO" ${ENABLE_OPENIMEAGEIO_DEFAULT})
if (GPUPAD_ENABLE_OPENIMEAGEIO)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenImageIO::OpenImageIO)
    target_compile_definitions(${PROJECT_NAME} PRIVATE  OpenImageIO_FOUND)
endif()

# link Slang
find_package(Slang)
if (Slang_FOUND)
    set(ENABLE_SLANG_DEFAULT TRUE)
endif()
option(GPUPAD_ENABLE_SLANG "Enable Slang" ${ENABLE_SLANG_DEFAULT})
if (GPUPAD_ENABLE_SLANG)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${Slang_LIBRARY})
    target_compile_definitions(${PROJECT_NAME} PRIVATE Slang_FOUND)
endif()

# add custom action projects
set(CMAKE_FOLDER Actions)
set(ACTIONS
    ImportOBJ
    GenerateMesh
)
set(CMAKE_SHARED_LIBRARY_PREFIX "")
foreach(ACTION ${ACTIONS})
    add_subdirectory("extra/actions/${ACTION}" EXCLUDE_FROM_ALL)
    add_dependencies(${PROJECT_NAME} ${ACTION})
endforeach()

# install -------------------------
set(DOC_FILES LICENSE CHANGELOG.md THIRD-PARTY.md)
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME Application)

if(NOT CMAKE_SYSTEM_NAME MATCHES "Windows")
    install(TARGETS ${PROJECT_NAME} DESTINATION "bin")
    install(FILES extra/bin/gpupad-headless DESTINATION "bin")
    install(FILES ${DOC_FILES} DESTINATION "share/gpupad")
    install(DIRECTORY extra/share DESTINATION .)
    install(DIRECTORY extra/samples DESTINATION "share/gpupad" COMPONENT Samples)
    install(DIRECTORY extra/themes DESTINATION "share/gpupad" COMPONENT Themes)
    install(DIRECTORY extra/actions DESTINATION "share/gpupad" COMPONENT Actions)
    install(DIRECTORY extra/libs DESTINATION "share/gpupad" COMPONENT Actions)
    install(DIRECTORY extra/qml DESTINATION "share/gpupad" COMPONENT Actions)
    foreach(ACTION ${ACTIONS})
        install(TARGETS ${ACTION} DESTINATION "share/gpupad/actions/${ACTION}" COMPONENT Actions)
    endforeach()

else()
    install(TARGETS ${PROJECT_NAME} DESTINATION "bin")
    install(FILES extra/bin/gpupad-headless.cmd DESTINATION "bin")
    install(FILES ${DOC_FILES} DESTINATION .)
    install(DIRECTORY extra/samples DESTINATION . COMPONENT Samples)
    install(DIRECTORY extra/themes DESTINATION . COMPONENT Themes)
    install(DIRECTORY extra/actions DESTINATION . COMPONENT Actions)
    install(DIRECTORY extra/libs DESTINATION . COMPONENT Actions)
    install(DIRECTORY extra/qml DESTINATION . COMPONENT Actions)
    foreach(ACTION ${ACTIONS})
        install(TARGETS ${ACTION} DESTINATION "actions/${ACTION}" COMPONENT Actions)
    endforeach()

    # install Qt dependencies
    set(DEPLOY_TOOL_OPTIONS "--qmldir ${CMAKE_CURRENT_SOURCE_DIR}/extra/qml")
    set(DEPLOY_TOOL_OPTIONS ${DEPLOY_TOOL_OPTIONS} --no-opengl-sw --no-system-d3d-compiler --no-system-dxc-compiler)
    qt_generate_deploy_app_script(
        TARGET ${PROJECT_NAME}
        OUTPUT_SCRIPT deploy_script
        NO_TRANSLATIONS
        NO_COMPILER_RUNTIME
        DEPLOY_TOOL_OPTIONS ${DEPLOY_TOOL_OPTIONS}
    )
    install(SCRIPT ${deploy_script})

    # install other dependencies
    set(QT_BIN_DIR "${Qt6_DIR}/../../../bin")
    install(TARGETS ${PROJECT_NAME} RUNTIME_DEPENDENCIES
        DIRECTORIES ${QT_BIN_DIR}
        PRE_INCLUDE_REGEXES dep
        PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
        POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# package -------------------------
set(CPACK_PACKAGE_NAME "GPUpad")
set(CPACK_PACKAGE_DESCRIPTION "A flexible GLSL and HLSL shader editor and IDE")
set(CPACK_PACKAGE_VENDOR "Albert Kalchmair")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "GPUpad")
set(CPACK_PACKAGE_EXECUTABLES "gpupad;GPUpad")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/houmain/gpupad")
set(CPACK_PACKAGE_CONTACT ${CPACK_PACKAGE_HOMEPAGE_URL})
set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/src/images/icon.ico")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_STRIP_FILES TRUE)
set(CPACK_COMPONENT_APPLICATION_HIDDEN TRUE)
set(CPACK_COMPONENT_APPLICATION_REQUIRED TRUE)

if(VERSION MATCHES "^[0-9]+\.[0-9]+\.[0-9]+")
    string(REGEX MATCHALL "[0-9]+" VERSION_LIST "${VERSION}")
    list(GET VERSION_LIST 0 CPACK_PACKAGE_VERSION_MAJOR)
    list(GET VERSION_LIST 1 CPACK_PACKAGE_VERSION_MINOR)
    list(GET VERSION_LIST 2 CPACK_PACKAGE_VERSION_PATCH)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(CPACK_GENERATOR TGZ)
    
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(CPACK_GENERATOR WIX)

    file(READ  "${CMAKE_SOURCE_DIR}/LICENSE" LICENSE_RTF)
    string(REPLACE "\n" "\\par" LICENSE_RTF "${LICENSE_RTF}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.rtf" "{\\rtf1\\ansi\\deff0\\nouicompat{\\fonttbl{\\f0\\fnil\\fcharset0 Sans Serif;}}\\fs18 ${LICENSE_RTF} }")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.rtf")

    set(CPACK_WIX_UPGRADE_GUID "13B245EB-E587-444F-95AA-D1AD22C22A26")
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/src/images/icon.ico")
    set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/extra/wix/dialog.png")
    set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/extra/wix/banner.png")

elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "${PROJECT_NAME}")
endif()

include(CPack)
