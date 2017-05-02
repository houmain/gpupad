#include "ShaderCompiler.h"
#include "Singletons.h"
#include "files/FileManager.h"
#include "files/SourceFile.h"
#include "GLShader.h"

ShaderCompiler::ShaderCompiler(QString fileName, QObject* parent)
    : RenderTask(parent)
    , mFileName(fileName)
{
}

ShaderCompiler::~ShaderCompiler()
{
    releaseResources();
}

void ShaderCompiler::prepare()
{
    mMessages.clear();
    mMessages.setContext(mFileName);

    // TODO: improve - take from session...
    mShaderType = Shader::Type::Fragment;
    if (mFileName.endsWith(".vert") || mFileName.endsWith(".vs"))
        mShaderType = Shader::Type::Vertex;
    if (mFileName.endsWith(".comp"))
        mShaderType = Shader::Type::Compute;

    auto file = Singletons::fileManager().findSourceFile(mFileName);
    if (file)
        mSource = file->toPlainText();
}

void ShaderCompiler::render(QOpenGLContext &glContext)
{
    auto usedItems = QSet<ItemId>();
    if (auto context = RenderContext(glContext, &usedItems, &mMessages)) {
        auto shader = GLShader(mFileName, mShaderType, mSource);
        shader.compile(context);
    }
}
