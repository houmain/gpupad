import qbs

Project {
  name: "GPUpad"
  minimumQbsVersion: "1.6.0"

  CppApplication {
    consoleApplication: false
    targetName: "gpupad"
    files: [
          "src/EditActions.h",
          "src/MainWindow.cpp",
          "src/MainWindow.h",
          "src/MainWindow.ui",
          "src/MessageList.cpp",
          "src/MessageList.h",
          "src/MessageWindow.cpp",
          "src/MessageWindow.h",
          "libs/SingleApplication/singleapplication.cpp",
          "libs/SingleApplication/singleapplication.h",
          "libs/SingleApplication/singleapplication_p.h",
          "src/Singletons.cpp",
          "src/Singletons.h",
          "src/SynchronizeLogic.cpp",
          "src/SynchronizeLogic.h",
          "src/files/BinaryEditor.cpp",
          "src/files/BinaryEditor.h",
          "src/files/DockWindow.cpp",
          "src/files/DockWindow.h",
          "src/files/FileDialog.cpp",
          "src/files/FileDialog.h",
          "src/files/FileManager.cpp",
          "src/files/FileManager.h",
          "src/files/BinaryFile.cpp",
          "src/files/BinaryFile.h",
          "src/files/FileTypes.h",
          "src/files/FindReplaceBar.cpp",
          "src/files/FindReplaceBar.h",
          "src/files/FindReplaceBar.ui",
          "src/files/GlslHighlighter.cpp",
          "src/files/GlslHighlighter.h",
          "src/files/ImageEditor.cpp",
          "src/files/ImageEditor.h",
          "src/files/ImageFile.cpp",
          "src/files/ImageFile.h",
          "src/files/SourceEditor.cpp",
          "src/files/SourceEditor.h",
          "src/files/SourceEditorSettings.cpp",
          "src/files/SourceEditorSettings.h",
          "src/files/SourceFile.cpp",
          "src/files/SourceFile.h",
          "src/main.cpp",
          "src/render/GLBuffer.cpp",
          "src/render/GLBuffer.h",
          "src/render/GLFramebuffer.cpp",
          "src/render/GLFramebuffer.h",
          "src/render/GLObject.h",
          "src/render/GLPrimitives.cpp",
          "src/render/GLPrimitives.h",
          "src/render/GLProgram.cpp",
          "src/render/GLProgram.h",
          "src/render/GLShader.cpp",
          "src/render/GLShader.h",
          "src/render/GLTexture.cpp",
          "src/render/GLTexture.h",
          "src/render/PrepareContext.h",
          "src/render/RenderCall.cpp",
          "src/render/RenderCall.h",
          "src/render/RenderContext.h",
          "src/render/RenderTask.cpp",
          "src/render/RenderTask.h",
          "src/render/Renderer.cpp",
          "src/render/Renderer.h",
          "src/render/ShaderCompiler.cpp",
          "src/render/ShaderCompiler.h",
          "src/session/AttachmentProperties.ui",
          "src/session/AttributeProperties.ui",
          "src/session/BindingProperties.cpp",
          "src/session/BindingProperties.h",
          "src/session/BindingProperties.ui",
          "src/session/BufferProperties.ui",
          "src/session/CallProperties.ui",
          "src/session/ColumnProperties.ui",
          "src/session/DataComboBox.cpp",
          "src/session/DataComboBox.h",
          "src/session/ExpressionEditor.cpp",
          "src/session/ExpressionEditor.h",
          "src/session/FramebufferProperties.ui",
          "src/session/GroupProperties.ui",
          "src/session/Item.h",
          "src/session/PrimitivesProperties.ui",
          "src/session/ProgramProperties.ui",
          "src/session/ReferenceComboBox.cpp",
          "src/session/ReferenceComboBox.h",
          "src/session/SamplerProperties.ui",
          "src/session/SessionEditor.cpp",
          "src/session/SessionEditor.h",
          "src/session/SessionModel.cpp",
          "src/session/SessionModel.h",
          "src/session/SessionProperties.cpp",
          "src/session/SessionProperties.h",
          "src/resources.qrc",
          "src/session/ShaderProperties.ui",
          "src/session/StateProperties.ui",
          "src/session/TextureProperties.cpp",
          "src/session/TextureProperties.h",
          "src/session/TextureProperties.ui",
          "src/_version.h",
      ]
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
