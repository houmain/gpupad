#pragma once

#include "RenderTask.h"
#include "opengl/GLProcessSource.h"

class ProcessSource final : public RenderTask
{
    Q_OBJECT
public:
    explicit ProcessSource(QObject *parent = nullptr) : RenderTask(parent) { }

    ~ProcessSource() override { releaseResources(); }

    void setFileName(QString fileName) { mImpl.setFileName(fileName); }

    void setSourceType(SourceType sourceType)
    {
        mImpl.setSourceType(sourceType);
    }

    void setValidateSource(bool validate) { mImpl.setValidateSource(validate); }

    void setProcessType(QString processType)
    {
        mImpl.setProcessType(processType);
    }

    void clearMessages() { mImpl.clearMessages(); }

Q_SIGNALS:
    void outputChanged(QString output);

private:
    bool initialize() override
    {
        return (renderer().api() == RenderAPI::OpenGL);
    }

    void prepare(bool itemsChanged, EvaluationType) override
    {
        mImpl.prepare();
    }

    void render() override { mImpl.render(); }

    void finish() override { Q_EMIT outputChanged(mImpl.output()); }

    void release() override { mImpl.release(); }

    GLProcessSource mImpl;
};
