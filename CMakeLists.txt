cmake_minimum_required(VERSION 3.0.0)
project(gpupad LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

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
  src/render/CompositorSync.cpp
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
  src/session/ExpressionLineEdit.cpp
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

add_compile_definitions(QAPPLICATION_CLASS=QApplication)
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE src libs)

if(NOT WIN32)
	target_include_directories(${PROJECT_NAME} PRIVATE /usr/include/libdrm)
	target_link_libraries(${PROJECT_NAME} drm)
else ()
	target_link_libraries(${PROJECT_NAME} dwmapi)
endif()

find_package(Qt5Widgets CONFIG REQUIRED)
find_package(Qt5OpenGL CONFIG REQUIRED)
find_package(Qt5Qml CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} Qt5::Widgets Qt5::OpenGL Qt5::Qml)

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
