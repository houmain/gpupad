#pragma once

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLFunctions_4_0_Core>
#include <QOpenGLFunctions_4_2_Core>
#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLFunctions_4_5_Core>

#if (QT_VERSION > QT_VERSION_CHECK(6, 0, 0))
# include <QOpenGLVersionFunctionsFactory>
#endif

class GLContext final : public QOpenGLContext, public QOpenGLFunctions_3_3_Core
{
    Q_OBJECT
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

#if (QT_VERSION > QT_VERSION_CHECK(6, 0, 0))
        v4_0 = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_4_0_Core>();
        v4_2 = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_4_2_Core>();
        v4_3 = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_4_3_Core>();
        v4_5 = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_4_5_Core>();
#else
        v4_0 = versionFunctions<QOpenGLFunctions_4_0_Core>();
        v4_2 = versionFunctions<QOpenGLFunctions_4_2_Core>();
        v4_3 = versionFunctions<QOpenGLFunctions_4_3_Core>();
        v4_5 = versionFunctions<QOpenGLFunctions_4_5_Core>();
#endif
        return true;
    }

    explicit operator bool()
    {
        return isInitialized();
    }
};

