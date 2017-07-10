import qbs

Project {
  name: "GPUpad"
  minimumQbsVersion: "1.6.0"

  CppApplication {
    consoleApplication: false
    targetName: "gpupad"

    cpp.includePaths: [ "src", "libs" ]
    cpp.cxxLanguageVersion: "c++14"
    cpp.enableRtti: false
    cpp.defines: [ "QAPPLICATION_CLASS=QApplication" ]
    cpp.dynamicLibraries: {
        var dynamicLibraries = base;
        if (qbs.targetOS.contains("windows"))
          dynamicLibraries.push("advapi32");
        return dynamicLibraries;
    }

    Depends {
      name: "Qt";
      submodules: ["core", "widgets", "opengl", "qml"]
    }

    files: [
          "libs/SingleApplication/singleapplication.cpp",
          "libs/SingleApplication/singleapplication.h",
          "libs/SingleApplication/singleapplication_p.h",
          "src/EditActions.h",
          "src/FileDialog.cpp",
          "src/FileDialog.h",
          "src/MainWindow.cpp",
          "src/MainWindow.h",
          "src/MainWindow.ui",
          "src/MessageList.cpp",
          "src/MessageList.h",
          "src/MessageWindow.cpp",
          "src/MessageWindow.h",
          "src/FileCache.cpp",
          "src/FileCache.h",
          "src/Settings.cpp",
          "src/Settings.h",
          "src/Singletons.cpp",
          "src/Singletons.h",
          "src/SynchronizeLogic.cpp",
          "src/SynchronizeLogic.h",
          "src/editors/BinaryEditor.cpp",
          "src/editors/BinaryEditor.h",
          "src/editors/BinaryEditor_DataModel.h",
          "src/editors/BinaryEditor_EditableRegion.h",
          "src/editors/BinaryEditor_HexModel.h",
          "src/editors/BinaryEditor_SpinBoxDelegate.h",
          "src/editors/DockWindow.cpp",
          "src/editors/DockWindow.h",
          "src/editors/EditorManager.cpp",
          "src/editors/EditorManager.h",
          "src/editors/FindReplaceBar.cpp",
          "src/editors/FindReplaceBar.h",
          "src/editors/FindReplaceBar.ui",
          "src/editors/GlslHighlighter.cpp",
          "src/editors/GlslHighlighter.h",
          "src/editors/IEditor.h",
          "src/editors/ImageEditor.cpp",
          "src/editors/ImageEditor.h",
          "src/editors/JsHighlighter.cpp",
          "src/editors/JsHighlighter.h",
          "src/editors/SourceEditor.cpp",
          "src/editors/SourceEditor.h",
          "src/main.cpp",
          "src/render/GLBuffer.cpp",
          "src/render/GLBuffer.h",
          "src/render/GLCall.cpp",
          "src/render/GLCall.h",
          "src/render/GLContext.h",
          "src/render/GLFramebuffer.cpp",
          "src/render/GLFramebuffer.h",
          "src/render/GLItem.h",
          "src/render/GLObject.h",
          "src/render/GLProgram.cpp",
          "src/render/GLProgram.h",
          "src/render/GLShader.cpp",
          "src/render/GLShader.h",
          "src/render/GLTexture.cpp",
          "src/render/GLTexture.h",
          "src/render/GLVertexStream.cpp",
          "src/render/GLVertexStream.h",
          "src/render/RenderSession.cpp",
          "src/render/RenderSession.h",
          "src/render/RenderTask.cpp",
          "src/render/RenderTask.h",
          "src/render/Renderer.cpp",
          "src/render/Renderer.h",
          "src/render/ScriptEngine.cpp",
          "src/render/ScriptEngine.h",
          "src/session/AttachmentProperties.ui",
          "src/session/AttributeProperties.ui",
          "src/session/BindingProperties.cpp",
          "src/session/BindingProperties.h",
          "src/session/BindingProperties.ui",
          "src/session/BufferProperties.ui",
          "src/session/CallProperties.cpp",
          "src/session/CallProperties.h",
          "src/session/CallProperties.ui",
          "src/session/ColorPicker.h",
          "src/session/ColorPicker.cpp",
          "src/session/ColumnProperties.ui",
          "src/session/DataComboBox.cpp",
          "src/session/DataComboBox.h",
          "src/session/ExpressionEditor.cpp",
          "src/session/ExpressionEditor.h",
          "src/session/ExpressionMatrix.cpp",
          "src/session/ExpressionMatrix.h",
          "src/session/FramebufferProperties.ui",
          "src/session/GroupProperties.ui",
          "src/session/ImageProperties.ui",
          "src/session/Item.h",
          "src/session/ProgramProperties.ui",
          "src/session/ReferenceComboBox.cpp",
          "src/session/ReferenceComboBox.h",
          "src/session/SamplerProperties.ui",
          "src/session/ScriptProperties.ui",
          "src/session/SessionEditor.cpp",
          "src/session/SessionEditor.h",
          "src/session/SessionModel.cpp",
          "src/session/SessionModel.h",
          "src/session/SessionProperties.cpp",
          "src/session/SessionProperties.h",
          "src/resources.qrc",
          "src/session/ShaderProperties.ui",
          "src/session/TextureProperties.cpp",
          "src/session/TextureProperties.h",
          "src/session/TextureProperties.ui",
          "src/_version.h",
          "src/session/VertexStreamProperties.ui",
      ]

    Group {
      fileTagsFilter: product.type
      qbs.install: true
      qbs.installDir: "bin"
    }

    Group {
      name: "Share"
      files: [
            "share/**",
        ]
      qbs.installSourceBase: "share"
      qbs.install: true
      qbs.installDir: "share"
    }
  }
}
