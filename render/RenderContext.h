#ifndef RENDERCONTEXT_H
#define RENDERCONTEXT_H

#include "session/Item.h"
#include "MessageList.h"
#include "GLObject.h"
#include <QList>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLFunctions_4_0_Core>
#include <QOpenGLFunctions_4_2_Core>
#include <QOpenGLFunctions_4_3_Core>

class RenderContext : public QOpenGLFunctions_3_3_Core
{
public:
    QSet<ItemId> &usedItems;
    MessageList &messages;
    QOpenGLFunctions_4_0_Core* gl40{ };
    QOpenGLFunctions_4_2_Core* gl42{ };
    QOpenGLFunctions_4_3_Core* gl43{ };

    RenderContext(QOpenGLContext& glContext, QSet<ItemId> *usedItems,
                  MessageList *messages)
        : usedItems(*usedItems)
        , messages(*messages)
    {
        if (!initializeOpenGLFunctions())
            messages->insert(MessageType::OpenGL33NotSupported);

        gl40 = glContext.versionFunctions<QOpenGLFunctions_4_0_Core>();
        gl42 = glContext.versionFunctions<QOpenGLFunctions_4_2_Core>();
        gl43 = glContext.versionFunctions<QOpenGLFunctions_4_3_Core>();
    }

    explicit operator bool() const { return isInitialized(); }
};

#endif // RENDERCONTEXT_H
