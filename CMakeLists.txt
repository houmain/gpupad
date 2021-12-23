cmake_minimum_required(VERSION 3.12.0)
project(gpupad LANGUAGES CXX C)

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
  libs/tga/encoder.cpp
  libs/tga/decoder.cpp
  libs/tga/image_iterator.cpp
  libs/tga/stdio.cpp
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
  src/VideoManager.cpp
  src/InputState.cpp
  src/SourceType.cpp
  src/editors/BinaryEditor.cpp
  src/editors/BinaryEditorToolBar.cpp
  src/editors/BinaryEditorToolBar.ui
  src/editors/DockWindow.cpp
  src/editors/EditorManager.cpp
  src/editors/FindReplaceBar.cpp
  src/editors/FindReplaceBar.ui
  src/editors/SyntaxHighlighter.cpp
  src/editors/SyntaxGLSL.cpp
  src/editors/SyntaxHLSL.cpp
  src/editors/SyntaxJavaScript.cpp
  src/editors/TextureEditorToolBar.cpp
  src/editors/TextureEditorToolBar.ui
  src/editors/TextureEditor.cpp
  src/editors/TextureItem.cpp
  src/editors/TextureInfoBar.cpp
  src/editors/TextureInfoBar.ui
  src/editors/Histogram.cpp
  src/editors/SourceEditor.cpp
  src/editors/SourceEditorToolBar.cpp
  src/editors/SourceEditorToolBar.ui
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
  src/render/ComputeRange.cpp
  src/render/glslang.cpp
  src/scripting/GpupadScriptObject.cpp
  src/scripting/GpupadScriptObject.js
  src/scripting/MouseScriptObject.cpp
  src/scripting/KeyboardScriptObject.cpp
  src/scripting/ScriptEngine.cpp
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
    set(HEADERS ${HEADERS} src/resources.rc src/qt5.natvis)
endif()
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

add_compile_definitions(QAPPLICATION_CLASS=QApplication KTX_OPENGL)
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
    target_include_directories(${PROJECT_NAME} PRIVATE /usr/include/libdrm)
    target_link_libraries(${PROJECT_NAME} GL drm)
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} opengl32 dwmapi)
elseif(APPLE)
    find_package(OpenGL REQUIRED)
    include_directories(${OPENGL_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${OPENGL_LIBRARIES})
endif()

# link Qt
find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
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
        add_compile_definitions(QtMultimedia_FOUND)
    endif()
endif()

option(ENABLE_WEBENGINE "Enable QtWebEngine" OFF)
if(ENABLE_WEBENGINE)
    find_package(Qt${QT_VERSION_MAJOR}WebEngineWidgets CONFIG)
    if(Qt${QT_VERSION_MAJOR}WebEngineWidgets_FOUND)
        target_link_libraries(${PROJECT_NAME} Qt${QT_VERSION_MAJOR}::WebEngineWidgets)
        add_compile_definitions(QtWebEngineWidgets_FOUND)
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
if(NOT WIN32)
    install(TARGETS ${PROJECT_NAME} DESTINATION "bin")
    install(DIRECTORY share DESTINATION . COMPONENT Application)
    install(DIRECTORY samples DESTINATION "share/gpupad" COMPONENT Samples)
else()
    install(TARGETS ${PROJECT_NAME} DESTINATION . COMPONENT Application)
    install(DIRECTORY samples DESTINATION . COMPONENT Samples)

    get_target_property(uic_location Qt::uic IMPORTED_LOCATION)
    get_filename_component(_dir ${uic_location} DIRECTORY)
    set(deployqt "${_dir}/windeployqt.exe")
    if(NOT EXISTS ${deployqt})
        message(FATAL_ERROR "Failed to locate deployqt executable: [${deployqt}]")
    endif()

    install(CODE "set(deployqt \"${deployqt}\")" COMPONENT Application)
    install(CODE [===[
    execute_process(COMMAND "${deployqt}" --release --no-qmltooling --no-opengl-sw --no-system-d3d-compiler --no-translations --no-virtualkeyboard "${CMAKE_INSTALL_PREFIX}")
    ]===] COMPONENT Application)
endif()

# package
set(CPACK_PACKAGE_NAME "GPUpad")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "GPUpad")
set(CPACK_PACKAGE_VENDOR "Albert Kalchmair")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "GPUpad")
set(CPACK_PACKAGE_EXECUTABLES "gpupad;GPUpad")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/houmain/gpupad")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
string(REGEX MATCHALL "[0-9]+" VERSION_LIST "${GIT_COMMIT_LAST_TAG}")
list(GET VERSION_LIST 0 CPACK_PACKAGE_VERSION_MAJOR)
list(GET VERSION_LIST 1 CPACK_PACKAGE_VERSION_MINOR)
list(GET VERSION_LIST 2 CPACK_PACKAGE_VERSION_PATCH)

if(WIN32)
    set(CPACK_GENERATOR ${CPACK_GENERATOR} WIX)
    file(COPY "${CMAKE_SOURCE_DIR}/LICENSE"
        DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
    file(RENAME "${CMAKE_CURRENT_BINARY_DIR}/LICENSE"
        "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.txt")
    set(CPACK_RESOURCE_FILE_LICENSE  "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.txt")
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/src/images/icon.ico")
    set(CPACK_WIX_UPGRADE_GUID "13B245EB-E587-444F-95AA-D1AD22C22A26")
    include(CPack)
endif()
