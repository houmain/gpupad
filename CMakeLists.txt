cmake_minimum_required(VERSION 3.12.0)
project(gpupad LANGUAGES CXX C)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(GIT_COMMIT_LAST_TAG "0.0.0")
find_package(Git)
if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE GIT_COMMIT_LAST_TAG
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

configure_file("src/version.h.in" "${CMAKE_SOURCE_DIR}/src/_version.h")

set(CMAKE_CXX_STANDARD 17)
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
  libs/KTX/lib/checkheader.c
  libs/KTX/lib/errstr.c
  libs/KTX/lib/filestream.c
  libs/KTX/lib/hashlist.c
  libs/KTX/lib/hashtable.c
  libs/KTX/lib/memstream.c
  libs/KTX/lib/swap.c
  libs/KTX/lib/texture.c
  libs/KTX/lib/writer.c
  libs/KTX/lib/writer_v1.c
  libs/KTX/lib/glloader.c
  libs/miniz/miniz.c
  libs/lua/src/lapi.c
  libs/lua/src/lauxlib.c
  libs/lua/src/lbaselib.c
  libs/lua/src/lcode.c
  libs/lua/src/lcorolib.c
  libs/lua/src/lctype.c
  libs/lua/src/ldblib.c
  libs/lua/src/ldebug.c
  libs/lua/src/ldo.c
  libs/lua/src/ldump.c
  libs/lua/src/lfunc.c
  libs/lua/src/lgc.c
  libs/lua/src/linit.c
  libs/lua/src/liolib.c
  libs/lua/src/llex.c
  libs/lua/src/lmathlib.c
  libs/lua/src/lmem.c
  libs/lua/src/loadlib.c
  libs/lua/src/lobject.c
  libs/lua/src/lopcodes.c
  libs/lua/src/loslib.c
  libs/lua/src/lparser.c
  libs/lua/src/lstate.c
  libs/lua/src/lstring.c
  libs/lua/src/lstrlib.c
  libs/lua/src/ltable.c
  libs/lua/src/ltablib.c
  libs/lua/src/ltm.c
  libs/lua/src/lundump.c
  libs/lua/src/lutf8lib.c
  libs/lua/src/lvm.c
  libs/lua/src/lzio.c
  libs/tga/encoder.cpp
  libs/tga/decoder.cpp
  libs/tga/image_iterator.cpp
  libs/tga/stdio.cpp
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
  src/windows/TitleBar.cpp
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
  src/editors/source/SyntaxLua.cpp
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
  src/render/GLBuffer.cpp
  src/render/GLCall.cpp
  src/render/GLProgram.cpp
  src/render/GLShader.cpp
  src/render/GLStream.cpp
  src/render/GLTarget.cpp
  src/render/GLTexture.cpp
  src/render/GLPrintf.cpp
  src/render/RenderSession.cpp
  src/render/RenderTask.cpp
  src/render/Renderer.cpp
  src/render/ProcessSource.cpp
  src/render/ComputeRange.cpp
  src/render/glslang.cpp
  src/scripting/ScriptConsole.cpp
  src/scripting/MouseScriptObject.cpp
  src/scripting/KeyboardScriptObject.cpp
  src/scripting/SessionScriptObject.cpp
  src/scripting/ScriptEngine.cpp
  src/scripting/ScriptEngineJavaScript.cpp
  src/scripting/ScriptEngineJavaScript.js
  src/scripting/ScriptEngineLua.cpp
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

file(GLOB_RECURSE HEADERS include *.h *.hpp)
if(WIN32)
    set(HEADERS ${HEADERS} src/resources.rc src/qt6.natvis)
endif()
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})

add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE ${SOURCES} ${HEADERS})

add_compile_definitions(KTX_OPENGL)
target_include_directories(${PROJECT_NAME} PRIVATE src libs libs/KTX/include libs/gli libs/glm)

if (MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W3)
    add_compile_options($<$<CONFIG:Release>:/GR->)
    add_compile_options($<$<CONFIG:Release>:/GL>)
    add_compile_options($<$<CONFIG:Release>:/Os>)
    add_link_options($<$<CONFIG:Release>:/LTCG>)
else()
    add_compile_options(-Wall)
    add_compile_options($<$<CONFIG:Release>:-fno-rtti>)
    add_link_options($<$<CONFIG:Release>:-s>)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${PROJECT_NAME} GL)
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} opengl32)
elseif(APPLE)
    find_package(OpenGL REQUIRED)
    include_directories(${OPENGL_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${OPENGL_LIBRARIES})
endif()

# link Qt
if(NOT DEFINED QT_VERSION_MAJOR)
    find_package(QT NAMES Qt5 Qt6 COMPONENTS Widgets REQUIRED)
    set(QT_VERSION_MAJOR ${QT_VERSION_MAJOR} CACHE STRING "Qt version" FORCE)
endif()
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS OpenGL REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Qml REQUIRED)
target_link_libraries(${PROJECT_NAME}
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::OpenGL
    Qt${QT_VERSION_MAJOR}::Qml)

if(${QT_VERSION_MAJOR} EQUAL 6)
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS OpenGLWidgets)
    target_link_libraries(${PROJECT_NAME} Qt${QT_VERSION_MAJOR}::OpenGLWidgets)
endif()

option(ENABLE_MULTIMEDIA "Enable QtMultimedia" ON)
if(ENABLE_MULTIMEDIA)
    find_package(Qt${QT_VERSION_MAJOR}Multimedia CONFIG)
    if(Qt${QT_VERSION_MAJOR}Multimedia_FOUND)
        target_link_libraries(${PROJECT_NAME} Qt${QT_VERSION_MAJOR}::Multimedia)
        add_compile_definitions(Qt${QT_VERSION_MAJOR}Multimedia_FOUND)
    endif()
endif()

option(ENABLE_QUICK "Enable QtQuick" ON)
if(ENABLE_QUICK)
    find_package(Qt${QT_VERSION_MAJOR}Quick CONFIG)
    find_package(Qt${QT_VERSION_MAJOR}QuickWidgets CONFIG)
    if(Qt${QT_VERSION_MAJOR}Quick_FOUND AND
       Qt${QT_VERSION_MAJOR}QuickWidgets_FOUND)
        target_link_libraries(${PROJECT_NAME}
            Qt${QT_VERSION_MAJOR}::Quick
            Qt${QT_VERSION_MAJOR}::QuickWidgets)
        add_compile_definitions(QtQuick_FOUND)
    endif()

    find_package(Qt${QT_VERSION_MAJOR}QuickShapes CONFIG)
    if(Qt${QT_VERSION_MAJOR}QuickShapes_FOUND)
        target_link_libraries(${PROJECT_NAME} Qt${QT_VERSION_MAJOR}::QuickShapes)
        add_compile_definitions(QtQuickShapes_FOUND)
    endif()

    find_package(Qt${QT_VERSION_MAJOR}QuickControls2 CONFIG)
    if(Qt${QT_VERSION_MAJOR}QuickControls2_FOUND)
        target_link_libraries(${PROJECT_NAME} Qt${QT_VERSION_MAJOR}::QuickControls2)
        add_compile_definitions(QtQuickControls2_FOUND)
    endif()
endif()

# include SPIRV-Cross
option(ENABLE_SPIRV_CROSS "Enable SPIRV-Cross" ON)
if (ENABLE_SPIRV_CROSS)
    set(SPIRV_CROSS_ENABLE_GLSL TRUE CACHE INTERNAL "SPIRV-Cross GLSL" FORCE)
    set(SPIRV_CROSS_CLI FALSE CACHE INTERNAL "SPIRV-Cross CLI" FORCE)
    set(SPIRV_CROSS_ENABLE_CPP FALSE CACHE INTERNAL "SPIRV-Cross CPP" FORCE)
    set(SPIRV_CROSS_ENABLE_C_API FALSE CACHE INTERNAL "SPIRV-Cross C-API" FORCE)
    set(SPIRV_CROSS_ENABLE_HLSL FALSE CACHE INTERNAL "SPIRV-Cross HLSL" FORCE)
    set(SPIRV_CROSS_ENABLE_MSL FALSE CACHE INTERNAL "SPIRV-Cross MSL" FORCE)
    set(SPIRV_CROSS_ENABLE_REFLECT FALSE CACHE INTERNAL "SPIRV-Cross REFLECT" FORCE)
    set(SPIRV_CROSS_ENABLE_TESTS FALSE CACHE INTERNAL "SPIRV-Cross TESTS" FORCE)
    set(SPIRV_CROSS_ENABLE_UTIL FALSE CACHE INTERNAL "SPIRV-Cross UTIL" FORCE)
    set(SPIRV_CROSS_SKIP_INSTALL TRUE CACHE INTERNAL "SPIRV-Cross Skip Install" FORCE)
    add_subdirectory(libs/SPIRV-Cross)
    target_link_libraries(${PROJECT_NAME} spirv-cross-glsl spirv-cross-core)
    add_compile_definitions(SPIRV_CROSS_ENABLED)
endif()

# install
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

# package
set(CPACK_PACKAGE_NAME "GPUpad")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "GPUpad")
set(CPACK_PACKAGE_VENDOR "Albert Kalchmair")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "GPUpad")
set(CPACK_PACKAGE_EXECUTABLES "gpupad;GPUpad")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/houmain/gpupad")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
string(REGEX MATCHALL "[0-9]+" VERSION_LIST "${GIT_COMMIT_LAST_TAG}")
list(GET VERSION_LIST 0 CPACK_PACKAGE_VERSION_MAJOR)
list(GET VERSION_LIST 1 CPACK_PACKAGE_VERSION_MINOR)
list(GET VERSION_LIST 2 CPACK_PACKAGE_VERSION_PATCH)

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
