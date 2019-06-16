cmake_minimum_required(VERSION 3.0.0)
project(gpupad LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include_directories(src libs)
add_compile_definitions(QAPPLICATION_CLASS=QApplication)

set(SOURCES
  libs/SingleApplication/singleapplication.cpp
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
  src/editors/BinaryEditor.cpp
  src/editors/DockWindow.cpp
  src/editors/EditorManager.cpp
  src/editors/FindReplaceBar.cpp
  src/editors/FindReplaceBar.ui
  src/editors/GlslHighlighter.cpp
  src/editors/ImageEditor.cpp
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
  src/render/RenderSession.cpp
  src/render/RenderTask.cpp
  src/render/Renderer.cpp
  src/render/ProcessSource.cpp
  src/scripting/GpupadScriptObject.cpp
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
  src/session/ExpressionEditor.cpp
  src/session/ExpressionMatrix.cpp
  src/session/GroupProperties.ui
  src/session/ImageProperties.ui
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

add_executable(gpupad WIN32 ${SOURCES} ${HEADERS})

find_package(Qt5Widgets CONFIG REQUIRED)
find_package(Qt5OpenGL CONFIG REQUIRED)
find_package(Qt5Qml CONFIG REQUIRED)
target_link_libraries(gpupad Qt5::Widgets Qt5::OpenGL Qt5::Qml)

if(NOT WIN32)
  install(TARGETS gpupad DESTINATION "bin")
  install(DIRECTORY share DESTINATION .)
  install(DIRECTORY samples DESTINATION "share/gpupad")
else()
  install(TARGETS gpupad DESTINATION .)
  install(DIRECTORY samples DESTINATION .)
endif()
