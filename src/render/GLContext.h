#ifndef GLCONTEXT_H
#define GLCONTEXT_H

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLFunctions_4_0_Core>
#include <QOpenGLFunctions_4_2_Core>
#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLFunctions_4_5_Core>

class GLContext : public QOpenGLContext, public QOpenGLFunctions_3_3_Core
{
public:
    static GLContext &currentContext()
    {
        auto current = QOpenGLContext::currentContext();
        Q_ASSERT(current);
        return *static_cast<GLContext*>(current);
    }

    QOpenGLFunctions_4_0_Core *v4_0{ };
    QOpenGLFunctions_4_2_Core *v4_2{ };
    QOpenGLFunctions_4_3_Core *v4_3{ };
    QOpenGLFunctions_4_5_Core *v4_5{ };

    bool initializeOpenGLFunctions() override
    {
        if (!QOpenGLFunctions_3_3_Core::initializeOpenGLFunctions())
            return false;
        v4_0 = versionFunctions<QOpenGLFunctions_4_0_Core>();
        v4_2 = versionFunctions<QOpenGLFunctions_4_2_Core>();
        v4_3 = versionFunctions<QOpenGLFunctions_4_3_Core>();
        v4_5 = versionFunctions<QOpenGLFunctions_4_5_Core>();
        return true;
    }

    explicit operator bool()
    {
        return isInitialized();
    }
};

#endif // GLCONTEXT_H
