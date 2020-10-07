cmake_minimum_required(VERSION 3.0.0)
project(gpupad LANGUAGES CXX C)

set(GIT_COMMIT_LAST_TAG "unknown")
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
                -DQT_DISABLE_DEPRECATED_BEFORE=0x051200
                -DQT_NO_FOREACH)

set(SOURCES
  libs/SingleApplication/singleapplication.cpp
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
  src/FileDialog.cpp
  src/MainWindow.cpp
  src/MainWindow.ui
  src/MessageList.cpp
  src/MessageWindow.cpp
  src/OutputWindow.cpp
  src/FileCache.cpp
  src/Settings.cpp
  src/Singletons.cpp
  src/SynchronizeLogic.cpp
  src/TextureData.cpp
  src/VideoPlayer.cpp
  src/editors/BinaryEditor.cpp
  src/editors/DockWindow.cpp
  src/editors/EditorManager.cpp
  src/editors/FindReplaceBar.cpp
  src/editors/FindReplaceBar.ui
  src/editors/GlslHighlighter.cpp
  src/editors/TextureEditorToolBar.ui
  src/editors/TextureEditor.cpp
  src/editors/TextureItem.cpp
  src/editors/JsHighlighter.cpp
  src/editors/SourceEditor.cpp
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
  src/render/CompositorSync.cpp
  src/scripting/GpupadScriptObject.cpp
  src/scripting/InputScriptObject.cpp
  src/scripting/ScriptEngine.cpp
  src/scripting/CustomActions.cpp
  src/scripting/CustomActions.ui
  src/session/AttachmentProperties.cpp
  src/session/AttachmentProperties.ui
  src/session/AttributeProperties.ui
  src/session/BindingProperties.cpp
  src/session/BindingProperties.ui
  src/session/BufferProperties.ui
  src/session/CallProperties.cpp
  src/session/CallProperties.ui
  src/session/ColorPicker.cpp
  src/session/ColumnProperties.ui
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

file(GLOB_RECURSE HEADERS include *.h)
if(WIN32)
    set(HEADERS ${HEADERS} src/resources.rc qt5.natvis)
endif()
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})

add_compile_definitions(QAPPLICATION_CLASS=QApplication KTX_OPENGL)
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

option(OPTION_DEVELOPER_MODE "Developer Mode" OFF)
if (OPTION_DEVELOPER_MODE)
    add_definitions(-DQT_FORCE_ASSERTS)
    if (NOT MSVC)
        add_compile_options(-Wall -Wextra -Werror)
    endif()
endif()

target_include_directories(${PROJECT_NAME} PRIVATE src libs libs/KTX/include)

if(NOT WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE /usr/include/libdrm)
    target_link_libraries(${PROJECT_NAME} drm GL)
else ()
    target_link_libraries(${PROJECT_NAME} dwmapi opengl32)
endif()

find_package(Qt5Widgets CONFIG REQUIRED)
find_package(Qt5OpenGL CONFIG REQUIRED)
find_package(Qt5Qml CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} Qt5::Widgets Qt5::OpenGL Qt5::Qml)

find_package(Qt5Multimedia CONFIG)
if(Qt5Multimedia_FOUND)
    target_link_libraries(${PROJECT_NAME} Qt5::Multimedia)
    add_compile_definitions(Qt5Multimedia_FOUND)
endif()

if(NOT WIN32)
    install(TARGETS ${PROJECT_NAME} DESTINATION "bin")
    install(DIRECTORY share DESTINATION .)
    install(DIRECTORY samples DESTINATION "share/gpupad")
else()
    install(TARGETS ${PROJECT_NAME} DESTINATION . COMPONENT Runtime)
    install(DIRECTORY samples DESTINATION .)

    get_target_property(uic_location Qt5::uic IMPORTED_LOCATION)
    get_filename_component(_dir ${uic_location} DIRECTORY)
    set(deployqt "${_dir}/windeployqt.exe")
    if(NOT EXISTS ${deployqt})
        message(FATAL_ERROR "Failed to locate deployqt executable: [${deployqt}]")
    endif()

    install(CODE "set(deployqt \"${deployqt}\")" COMPONENT Runtime)
    install(CODE [===[
    execute_process(COMMAND "${deployqt}" --release --no-qmltooling --no-opengl-sw --no-system-d3d-compiler --no-translations "${CMAKE_INSTALL_PREFIX}")
    ]===] COMPONENT Runtime)
endif()
