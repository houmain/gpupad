#pragma once

#include "EditActions.h"
#include <QList>
#include <QMetaObject>

class IEditor
{
public:
    virtual ~IEditor() = default;
    virtual QList<QMetaObject::Connection> connectEditActions(
        const EditActions &actions) = 0;
    virtual QString fileName() const = 0;
    virtual void setFileName(QString fileName) = 0;
    virtual bool load() = 0;
    virtual bool save() = 0;
    virtual void setModified() = 0;
    virtual int tabifyGroup() const = 0;
};

