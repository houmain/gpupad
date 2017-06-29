#ifndef GLCONTEXT_H
#define GLCONTEXT_H

#include "GLObject.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLFunctions_4_0_Core>
#include <QOpenGLFunctions_4_2_Core>
#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLFunctions_4_5_Core>

struct GLContext : public QOpenGLFunctions_3_3_Core
{
    QOpenGLFunctions_4_0_Core *gl40{ };
    QOpenGLFunctions_4_2_Core *gl42{ };
    QOpenGLFunctions_4_3_Core *gl43{ };
    QOpenGLFunctions_4_5_Core *gl45{ };

    GLContext(QOpenGLContext &glContext)
    {
        initializeOpenGLFunctions();
        gl40 = glContext.versionFunctions<QOpenGLFunctions_4_0_Core>();
        gl42 = glContext.versionFunctions<QOpenGLFunctions_4_2_Core>();
        gl43 = glContext.versionFunctions<QOpenGLFunctions_4_3_Core>();
        gl45 = glContext.versionFunctions<QOpenGLFunctions_4_5_Core>();
    }

    explicit operator bool() const { return isInitialized(); }
};

#endif // GLCONTEXT_H
