#pragma once

#include "session/Item.h"
#include "MessageList.h"

namespace spirvCross
{

struct Interface
{
    struct EntryPoint
    {
        QString name;
        QString mode;
    };

    struct Input
    {
        QString type;
        QString name;
        uint32_t location;
    };

    struct Output
    {
        QString type;
        QString name;
        uint32_t location;
    };

    struct Texture
    {
        QString type;
        QString name;
        uint32_t set;
        uint32_t binding;
    };

    struct Image
    {
        QString type;
        QString name;
        uint32_t set;
        uint32_t binding;
        QString format;
    };

    struct Field
    {
        QString type;
        QString name;
        int offset;
        int matrixStride;
    };

    struct Buffer
    {
        QString type;
        QList<Field> members;
        QString name;
        int blockSize;
        uint32_t set;
        uint32_t binding;
    };

    QList<EntryPoint> entryPoints;
    QList<Input> inputs;
    QList<Output> outputs;
    QList<Texture> textures;
    QList<Image> images;
    QList<Buffer> buffers;
};

Interface getInterface(const std::vector<uint32_t> &spirv);

QString generateGLSL(std::vector<uint32_t> spirv, 
    const QString &fileName, MessagePtrSet &messages);

} // namespace