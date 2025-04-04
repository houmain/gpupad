#pragma once

#include "GLBuffer.h"
#include "GLProgram.h"

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

class GLStream
{
public:
    explicit GLStream(const Stream &stream);
    void setAttribute(int attributeIndex, const Field &column, GLBuffer *buffer,
        ScriptEngine &scriptEngine);

    ItemId itemId() const { return mItemId; }
    const GLAttribute *findAttribute(const QString &name) const;

private:
    bool validateAttribute(const GLAttribute &attribute) const;

    ItemId mItemId{};
    MessagePtrSet mMessages;
    QMap<int, GLAttribute> mAttributes;
};
