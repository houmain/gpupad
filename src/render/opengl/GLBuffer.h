#pragma once

#include "GLItem.h"

class GLBuffer
{
public:
    explicit GLBuffer(int size);
    GLBuffer(const Buffer &buffer, GLRenderSession &renderSession);
    void updateUntitledFilename(const GLBuffer &rhs);
    bool operator==(const GLBuffer &rhs) const;

    ItemId itemId() const { return mItemId; }
    const QByteArray &data() const { return mData; }
    const QString &fileName() const { return mFileName; }
    const QSet<ItemId> &usedItems() const { return mUsedItems; }
    int size() const { return mSize; }

    QByteArray &getWriteableData();
    void clear();
    void copy(GLBuffer &source);
    bool swap(GLBuffer &other);
    GLuint getReadOnlyBufferId();
    GLuint getReadWriteBufferId();
    void bindReadOnly(GLenum target);
    void bindIndexedRange(GLenum target, int index, int offset, int size,
        bool readonly);
    void unbind(GLenum target);
    bool download(bool checkModification);

private:
    void reload();
    void createBuffer();
    void upload();

    MessagePtrSet mMessages;
    ItemId mItemId{};
    QString mFileName;
    int mSize{};
    QByteArray mData;
    QSet<ItemId> mUsedItems;
    GLObject mBufferObject;
    bool mSystemCopyModified{};
    bool mDeviceCopyModified{};
};
