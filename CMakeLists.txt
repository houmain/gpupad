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

# build KDGpu --------------------
find_package(spdlog CONFIG REQUIRED)

find_package(Vulkan)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
add_library(vulkan-memory-allocator::vulkan-memory-allocator ALIAS GPUOpen::VulkanMemoryAllocator)

set(PROJECT_VERSION "0.4.4")
include(FeatureSummary)
add_compile_definitions(KDGPU_STATIC_DEFINE)
add_subdirectory(libs/KDGpu/src/KDGpu)

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
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

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
  src/render/opengl/GLProcessSource.cpp
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
  src/render/RenderTask.cpp  
  src/render/RenderSessionBase.cpp
  src/render/ShaderBase.cpp
  src/render/TextureBase.cpp
  src/render/ShaderPrintf.cpp
  src/render/glslang.cpp
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
  src/session/SessionProperties.cpp
  src/resources.qrc
  src/session/ShaderProperties.ui
  src/session/StreamProperties.ui
  src/session/TargetProperties.ui
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

add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE ${SOURCES} ${HEADERS})

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


# link Qt
if(NOT DEFINED QT_VERSION_MAJOR)
    find_package(QT NAMES Qt5 Qt6 COMPONENTS Widgets REQUIRED)
    set(QT_VERSION_MAJOR ${QT_VERSION_MAJOR} CACHE STRING "Qt version" FORCE)
endif()
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS OpenGL REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Qml REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::OpenGL
    Qt${QT_VERSION_MAJOR}::Qml)

if(${QT_VERSION_MAJOR} EQUAL 6)
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS OpenGLWidgets)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::OpenGLWidgets)
endif()

option(ENABLE_MULTIMEDIA "Enable QtMultimedia" ON)
if(ENABLE_MULTIMEDIA)
    find_package(Qt${QT_VERSION_MAJOR}Multimedia CONFIG)
    if(Qt${QT_VERSION_MAJOR}Multimedia_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Multimedia)
        add_compile_definitions(Qt${QT_VERSION_MAJOR}Multimedia_FOUND)
    endif()
endif()

option(ENABLE_QUICK "Enable QtQuick" ON)
if(ENABLE_QUICK)
    find_package(Qt${QT_VERSION_MAJOR}Quick CONFIG)
    find_package(Qt${QT_VERSION_MAJOR}QuickWidgets CONFIG)
    if(Qt${QT_VERSION_MAJOR}Quick_FOUND AND
       Qt${QT_VERSION_MAJOR}QuickWidgets_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            Qt${QT_VERSION_MAJOR}::Quick
            Qt${QT_VERSION_MAJOR}::QuickWidgets)
        add_compile_definitions(QtQuick_FOUND)
    endif()

    find_package(Qt${QT_VERSION_MAJOR}QuickShapes CONFIG)
    if(Qt${QT_VERSION_MAJOR}QuickShapes_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::QuickShapes)
        add_compile_definitions(QtQuickShapes_FOUND)
    endif()

    find_package(Qt${QT_VERSION_MAJOR}QuickControls2 CONFIG)
    if(Qt${QT_VERSION_MAJOR}QuickControls2_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::QuickControls2)
        add_compile_definitions(QtQuickControls2_FOUND)
    endif()
endif()

# include OpenImageIO
option(ENABLE_OPENIMEAGEIO "Enable OpenImageIO" ON)
    if (ENABLE_OPENIMEAGEIO)
    find_package(OpenImageIO CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenImageIO::OpenImageIO)
    add_compile_definitions(OPENIMAGEIO_ENABLED)
endif()

# include glslang
find_package(glslang CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glslang::OSDependent glslang::glslang 
    glslang::MachineIndependent glslang::GenericCodeGen glslang::glslang-default-resource-limits 
    glslang::SPVRemapper glslang::SPIRV)

# include SPIRV-Cross
find_package(spirv_cross_core CONFIG REQUIRED)
find_package(spirv_cross_glsl CONFIG REQUIRED)
find_package(spirv_cross_reflect CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} 
    PRIVATE spirv-cross-core spirv-cross-glsl spirv-cross-reflect)

# install -------------------------
set(DOC_FILES LICENSE CHANGELOG.md THIRD-PARTY.md)
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    install(TARGETS ${PROJECT_NAME} DESTINATION "bin" COMPONENT Application)
    install(FILES ${DOC_FILES} DESTINATION "share/gpupad" COMPONENT Application)
    install(DIRECTORY share DESTINATION . COMPONENT Application)
    install(DIRECTORY samples DESTINATION "share/gpupad" COMPONENT Samples)
    install(DIRECTORY themes DESTINATION "share/gpupad" COMPONENT Themes)

elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    install(TARGETS ${PROJECT_NAME} DESTINATION . COMPONENT Application)
    install(FILES ${DOC_FILES} DESTINATION . COMPONENT Application)
    install(DIRECTORY samples DESTINATION . COMPONENT Samples)
    install(DIRECTORY themes DESTINATION . COMPONENT Themes)

    install(TARGETS ${PROJECT_NAME} RUNTIME_DEPENDENCIES
        PRE_INCLUDE_REGEXES dep
        PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
        POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
        DESTINATION . COMPONENT Application
    )

    find_program(WINDEPLOYQT windeployqt HINTS "${_qt_bin_dir}")
    install(CODE "set(WINDEPLOYQT \"${WINDEPLOYQT}\")" COMPONENT Application)
    install(CODE [===[
    execute_process(COMMAND "${WINDEPLOYQT}" --release --no-qmltooling --no-opengl-sw --no-system-d3d-compiler --no-translations --no-virtualkeyboard --no-compiler-runtime "${CMAKE_INSTALL_PREFIX}")
    ]===] COMPONENT Application)
    
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    install(TARGETS ${PROJECT_NAME} DESTINATION . COMPONENT Application)
    install(FILES ${DOC_FILES} DESTINATION . COMPONENT Application)
    install(DIRECTORY samples DESTINATION . COMPONENT Samples)
    install(DIRECTORY themes DESTINATION . COMPONENT Themes)

    find_program(MACDEPLOYQT macdeployqt HINTS "${_qt_bin_dir}")
    install(CODE "set(MACDEPLOYQT \"${MACDEPLOYQT}\")" COMPONENT Application)
    install(CODE "set(APPLICATION \"${PROJECT_NAME}.app\")" COMPONENT Application)
    install(CODE [===[
    execute_process(COMMAND cd "${CMAKE_INSTALL_PREFIX}" && "${MACDEPLOYQT}" "${APPLICATION}" -always-overwrite)
    ]===] COMPONENT Application)
endif()

# package -------------------------
set(CPACK_PACKAGE_NAME "GPUpad")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "GPUpad")
set(CPACK_PACKAGE_VENDOR "Albert Kalchmair")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "GPUpad")
set(CPACK_PACKAGE_EXECUTABLES "gpupad;GPUpad")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/houmain/gpupad")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
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
    file(COPY "${CMAKE_SOURCE_DIR}/LICENSE"
        DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
    file(RENAME "${CMAKE_CURRENT_BINARY_DIR}/LICENSE"
        "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.txt")
    set(CPACK_RESOURCE_FILE_LICENSE  "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.txt")
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/src/images/icon.ico")
    set(CPACK_WIX_UPGRADE_GUID "13B245EB-E587-444F-95AA-D1AD22C22A26")

elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "${PROJECT_NAME}")
endif()

include(CPack)
