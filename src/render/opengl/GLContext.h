#pragma once

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLFunctions_4_0_Core>
#include <QOpenGLFunctions_4_2_Core>
#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLVersionFunctionsFactory>

class GLContext final : public QOpenGLContext, public QOpenGLFunctions_3_3_Core
{
    Q_OBJECT
public:
    static GLContext &currentContext()
    {
        auto current = QOpenGLContext::currentContext();
        Q_ASSERT(current);
        return *static_cast<GLContext *>(current);
    }

    QOpenGLFunctions_4_0_Core *v4_0{};
    QOpenGLFunctions_4_2_Core *v4_2{};
    QOpenGLFunctions_4_3_Core *v4_3{};
    QOpenGLFunctions_4_5_Core *v4_5{};

    bool initializeOpenGLFunctions() override
    {
        if (!QOpenGLFunctions_3_3_Core::initializeOpenGLFunctions())
            return false;

        v4_0 = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_4_0_Core>();
        v4_2 = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_4_2_Core>();
        v4_3 = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_4_3_Core>();
        v4_5 = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_4_5_Core>();
        return true;
    }

    explicit operator bool() { return isInitialized(); }
};
