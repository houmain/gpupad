#pragma once

#include "GLBuffer.h"
#include "GLProgram.h"

class GLStream
{
public:
    explicit GLStream(const Stream &stream);
    void setAttribute(int attributeIndex, const Field &column, GLBuffer *buffer,
        ScriptEngine &scriptEngine);

    bool bind(const GLProgram &program);
    void unbind(const GLProgram &program);
    const QSet<ItemId> &usedItems() const { return mUsedItems; }

private:
    struct GLAttribute
    {
        QSet<ItemId> usedItems;
        QString name;
        bool normalize{};
        int divisor{};
        GLBuffer *buffer{};
        Field::DataType type{};
        int count{};
        int stride{};
        int offset{};
    };

    bool validateAttribute(const GLAttribute &attribute) const;

    MessagePtrSet mMessages;
    QSet<ItemId> mUsedItems;
    QMap<int, GLAttribute> mAttributes;
};
