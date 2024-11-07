cmake_minimum_required(VERSION 3.21)
project(gpupad LANGUAGES CXX C)

if (MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W1 /MP)
    add_compile_options($<$<CONFIG:Release>:/GL>)
    add_compile_options($<$<CONFIG:Release>:/GR->)
    add_link_options($<$<CONFIG:Release>:/LTCG>)
else()
    add_compile_options($<$<CONFIG:Release>:-fno-rtti>)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core)
qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

# build KDGpu --------------------
find_package(spdlog CONFIG REQUIRED)

find_package(Vulkan)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
add_library(vulkan-memory-allocator::vulkan-memory-allocator ALIAS GPUOpen::VulkanMemoryAllocator)

set(PROJECT_VERSION "0.6.0")
include(FeatureSummary)
add_compile_definitions(KDGPU_STATIC_DEFINE)
add_subdirectory(libs/KDGpu/src/KDGpu EXCLUDE_FROM_ALL)

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
                -DQT_DISABLE_DEPRECATED_BEFORE=0x060000
                -DQT_NO_FOREACH)

set(SOURCES
  libs/SingleApplication/singleapplication.cpp
  libs/SingleApplication/singleapplication_p.cpp
  libs/spirv-reflect/spirv_reflect.cpp
  src/FileDialog.cpp
  src/EvaluatedPropertyCache.cpp
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
  src/editors/source/SyntaxJavaScript.cpp
  src/editors/texture/GLWidget.cpp
  src/editors/texture/Histogram.cpp
  src/editors/texture/TextureEditor.cpp
  src/editors/texture/TextureEditorToolBar.cpp
  src/editors/texture/TextureEditorToolBar.ui
  src/editors/texture/TextureInfoBar.cpp
  src/editors/texture/TextureInfoBar.ui
  src/editors/texture/TextureBackground.cpp
  src/editors/texture/TextureItem.cpp
  src/editors/qml/QmlView.cpp
  src/main.cpp
  src/render/opengl/GLBuffer.cpp
  src/render/opengl/GLCall.cpp
  src/render/opengl/GLProgram.cpp
  src/render/opengl/GLShader.cpp
  src/render/opengl/GLStream.cpp
  src/render/opengl/GLTarget.cpp
  src/render/opengl/GLTexture.cpp
  src/render/opengl/GLPrintf.cpp
  src/render/opengl/GLRenderSession.cpp
  src/render/opengl/GLRenderer.cpp
  src/render/opengl/GLComputeRange.cpp
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
  src/render/vulkan/VKRenderer.cpp
  src/render/vulkan/VKRenderSession.cpp
  src/render/ProcessSource.cpp
  src/render/RenderTask.cpp
  src/render/RenderSessionBase.cpp
  src/render/ShaderBase.cpp
  src/render/TextureBase.cpp
  src/render/ShaderPrintf.cpp
  src/render/Spirv.cpp
  src/scripting/ScriptConsole.cpp
  src/scripting/MouseScriptObject.cpp
  src/scripting/KeyboardScriptObject.cpp
  src/scripting/SessionScriptObject.cpp
  src/scripting/ScriptEngine.cpp
  src/scripting/ScriptEngineJavaScript.cpp
  src/scripting/ScriptEngineJavaScript.js
  src/scripting/ScriptSession.cpp
  src/scripting/CustomActions.cpp
  src/scripting/CustomActions.ui
  src/session/AttachmentProperties.cpp
  src/session/AttachmentProperties.ui
  src/session/AttributeProperties.ui
  src/session/BindingProperties.cpp
  src/session/BindingProperties.ui
  src/session/BufferProperties.ui
  src/session/BlockProperties.ui
  src/session/CallProperties.cpp
  src/session/CallProperties.ui
  src/session/ColorMask.cpp
  src/session/ColorPicker.cpp
  src/session/FieldProperties.ui
  src/session/DataComboBox.cpp
  src/session/Item.cpp
  src/session/ExpressionEditor.cpp
  src/session/ExpressionMatrix.cpp
  src/session/ExpressionLineEdit.cpp
  src/session/GroupProperties.ui
  src/session/ProgramProperties.ui
  src/session/ReferenceComboBox.cpp
  src/session/ScriptProperties.ui
  src/session/SessionEditor.cpp
  src/session/SessionModel.cpp
  src/session/SessionModelCore.cpp
  src/session/PropertiesEditor.cpp
  src/resources.qrc
  src/session/ShaderProperties.ui
  src/session/StreamProperties.ui
  src/session/TargetProperties.ui
  src/session/SessionProperties.cpp
  src/session/SessionProperties.ui
  src/session/TextureProperties.cpp
  src/session/TextureProperties.ui)

file(GLOB_RECURSE HEADERS include src/*.h)
if(WIN32)
    set(HEADERS ${HEADERS} src/resources.rc src/qt6.natvis)
endif()
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})

if (MSVC)
    add_compile_options(/W3)
else()
    add_compile_options(-Wall)
endif()

qt_add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

target_link_libraries(${PROJECT_NAME} PRIVATE KDGpu spdlog::spdlog)

find_package(Ktx CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE KTX::ktx)

target_include_directories(${PROJECT_NAME} PRIVATE src libs)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${PROJECT_NAME} PRIVATE GL)
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
elseif(APPLE)
    find_package(OpenGL REQUIRED)
    include_directories(${OPENGL_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_LIBRARIES})
endif()

# link Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL Qml)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::OpenGL
    Qt6::Qml)

find_package(Qt6Multimedia CONFIG)
option(ENABLE_MULTIMEDIA "Enable QtMultimedia" ON)
if(ENABLE_MULTIMEDIA AND Qt6Multimedia_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Multimedia)
    add_compile_definitions(QtMultimedia_FOUND)
endif()

find_package(Qt6Quick CONFIG)
find_package(Qt6QuickWidgets CONFIG)
option(ENABLE_QUICK "Enable QtQuick" ON)
if(ENABLE_QUICK AND Qt6Quick_FOUND AND Qt6QuickWidgets_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Quick
        Qt6::QuickWidgets)
    add_compile_definitions(QtQuick_FOUND)

    find_package(Qt6QuickControls2 CONFIG)
    if(Qt6QuickControls2_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::QuickControls2)
        add_compile_definitions(QtQuickControls2_FOUND)
    endif()
endif()

# link OpenImageIO
find_package(OpenImageIO CONFIG)
option(ENABLE_OPENIMEAGEIO "Enable OpenImageIO" ON)
if (ENABLE_OPENIMEAGEIO AND OpenImageIO_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenImageIO::OpenImageIO)
    add_compile_definitions(OpenImageIO_FOUND)
endif()

# link glslang
find_package(glslang CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glslang::OSDependent glslang::glslang 
    glslang::MachineIndependent glslang::GenericCodeGen glslang::glslang-default-resource-limits 
    glslang::SPVRemapper glslang::SPIRV)

# link SPIRV-Cross
find_package(spirv_cross_core CONFIG REQUIRED)
find_package(spirv_cross_glsl CONFIG REQUIRED)
find_package(spirv_cross_reflect CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} 
    PRIVATE spirv-cross-core spirv-cross-glsl spirv-cross-reflect)

# install -------------------------
set(DOC_FILES LICENSE CHANGELOG.md THIRD-PARTY.md)
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME Application)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    install(TARGETS ${PROJECT_NAME} DESTINATION "bin")
    install(FILES ${DOC_FILES} DESTINATION "share/gpupad")
    install(DIRECTORY extra/share DESTINATION .)
    install(DIRECTORY extra/samples DESTINATION "share/gpupad" COMPONENT Samples)
    install(DIRECTORY extra/themes DESTINATION "share/gpupad" COMPONENT Themes)

else()
    install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES ${DOC_FILES} DESTINATION .)
    install(DIRECTORY extra/samples DESTINATION . COMPONENT Samples)
    install(DIRECTORY extra/themes DESTINATION . COMPONENT Themes)

    # install Qt dependencies (requries Qt 5.7)
    set(DEPLOY_TOOL_OPTIONS "")
    if(CMAKE_SYSTEM_NAME MATCHES "Windows")
        set(DEPLOY_TOOL_OPTIONS --no-opengl-sw --no-system-d3d-compiler --no-system-dxc-compiler)
    endif()
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
