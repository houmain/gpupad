#ifndef GL_STREAM_H
#define GL_STREAM_H

#include "GLProgram.h"
#include "GLBuffer.h"

class GLStream
{
public:
    explicit GLStream(const Stream &stream);
    void setAttribute(int attributeIndex,
        const Field &column, GLBuffer *buffer);

    void bind(const GLProgram &program);
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    struct GLAttribute
    {
        QSet<ItemId> usedItems;
        QString name;
        bool normalize;
        int divisor;
        GLBuffer *buffer;
        Field::DataType type;
        int count;
        int stride;
        int offset;
    };

    QSet<ItemId> mUsedItems;
    QMap<int, GLAttribute> mAttributes;
};

#endif // GL_STREAM_H
