import qbs

Project {
  name: "GPUpad"
  minimumQbsVersion: "1.6.0"

  CppApplication {
    consoleApplication: false
    targetName: "gpupad"
    files: [
          "EditActions.h",
          "MainWindow.cpp",
          "MainWindow.h",
          "MainWindow.ui",
          "MessageList.cpp",
          "MessageList.h",
          "MessageWindow.cpp",
          "MessageWindow.h",
          "SingleApplication/singleapplication.cpp",
          "SingleApplication/singleapplication.h",
          "SingleApplication/singleapplication_p.h",
          "Singletons.cpp",
          "Singletons.h",
          "SynchronizeLogic.cpp",
          "SynchronizeLogic.h",
          "files/BinaryEditor.cpp",
          "files/BinaryEditor.h",
          "files/DockWindow.cpp",
          "files/DockWindow.h",
          "files/FileDialog.cpp",
          "files/FileDialog.h",
          "files/FileManager.cpp",
          "files/FileManager.h",
          "files/BinaryFile.cpp",
          "files/BinaryFile.h",
          "files/FileTypes.h",
          "files/FindReplaceBar.cpp",
          "files/FindReplaceBar.h",
          "files/FindReplaceBar.ui",
          "files/GlslHighlighter.cpp",
          "files/GlslHighlighter.h",
          "files/ImageEditor.cpp",
          "files/ImageEditor.h",
          "files/ImageFile.cpp",
          "files/ImageFile.h",
          "files/SourceEditor.cpp",
          "files/SourceEditor.h",
          "files/SourceEditorSettings.cpp",
          "files/SourceEditorSettings.h",
          "files/SourceFile.cpp",
          "files/SourceFile.h",
          "main.cpp",
          "render/GLBuffer.cpp",
          "render/GLBuffer.h",
          "render/GLFramebuffer.cpp",
          "render/GLFramebuffer.h",
          "render/GLObject.h",
          "render/GLPrimitives.cpp",
          "render/GLPrimitives.h",
          "render/GLProgram.cpp",
          "render/GLProgram.h",
          "render/GLShader.cpp",
          "render/GLShader.h",
          "render/GLTexture.cpp",
          "render/GLTexture.h",
          "render/PrepareContext.h",
          "render/RenderCall.cpp",
          "render/RenderCall.h",
          "render/RenderContext.h",
          "render/RenderTask.cpp",
          "render/RenderTask.h",
          "render/Renderer.cpp",
          "render/Renderer.h",
          "render/ShaderCompiler.cpp",
          "render/ShaderCompiler.h",
          "session/AttachmentProperties.ui",
          "session/AttributeProperties.ui",
          "session/BindingProperties.cpp",
          "session/BindingProperties.h",
          "session/BindingProperties.ui",
          "session/BufferProperties.ui",
          "session/CallProperties.ui",
          "session/ColumnProperties.ui",
          "session/DataComboBox.cpp",
          "session/DataComboBox.h",
          "session/ExpressionEditor.cpp",
          "session/ExpressionEditor.h",
          "session/FramebufferProperties.ui",
          "session/GroupProperties.ui",
          "session/Item.h",
          "session/PrimitivesProperties.ui",
          "session/ProgramProperties.ui",
          "session/ReferenceComboBox.cpp",
          "session/ReferenceComboBox.h",
          "session/SamplerProperties.ui",
          "session/SessionEditor.cpp",
          "session/SessionEditor.h",
          "session/SessionModel.cpp",
          "session/SessionModel.h",
          "session/SessionProperties.cpp",
          "session/SessionProperties.h",
          "resources.qrc",
          "session/ShaderProperties.ui",
          "session/StateProperties.ui",
          "session/TextureProperties.cpp",
          "session/TextureProperties.h",
          "session/TextureProperties.ui",
          "version.h",
      ]
    cpp.includePaths: [ "." ]
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

    Group {
      fileTagsFilter: product.type
      qbs.install: true
      qbs.installDir: "bin"
    }

    Group {
      name: "Share"
      files: "share/**"
      qbs.installSourceBase: "share"
      qbs.install: true
      qbs.installDir: "share"
    }
  }
}
