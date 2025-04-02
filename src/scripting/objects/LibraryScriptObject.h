#pragma once

#include <QQmlPropertyMap>
#include <QJSValue>
#include <memory>
#include "dllreflect/include/dllreflect.h"

class LibraryScriptObject_Opaque : public QObject
{
    Q_OBJECT
public:
    LibraryScriptObject_Opaque(std::shared_ptr<void> library,
        dllreflect::Argument &&argument, QObject *parent = nullptr);
    ~LibraryScriptObject_Opaque();

    const dllreflect::Argument &argument() const { return mArgument; }
    bool checkType(dllreflect::Type type) const;

private:
    std::shared_ptr<void> mLibrary;
    dllreflect::Argument mArgument;
};

class LibraryScriptObject_Array : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int length READ length CONSTANT)

public:
    LibraryScriptObject_Array(QJSEngine *jsEngine,
        std::shared_ptr<void> library, dllreflect::Argument &&argument,
        QObject *parent = nullptr);
    ~LibraryScriptObject_Array();

    Q_INVOKABLE QJSValue toArray() const;

    dllreflect::Type type() const { return base(mArgument.type); }
    int length() const { return static_cast<int>(mArgument.count); }

    template <typename T = void>
    const T *data() const
    {
        return static_cast<const T *>(mArgument.data);
    }

private:
    QJSEngine *mJsEngine{};
    std::shared_ptr<void> mLibrary;
    dllreflect::Argument mArgument;
};

class LibraryScriptObject_Callable : public QObject
{
    Q_OBJECT
public:
    using Opaque = LibraryScriptObject_Opaque;
    using Array = LibraryScriptObject_Array;

    LibraryScriptObject_Callable(QJSEngine *engine,
        std::shared_ptr<dllreflect::Library> library,
        QObject *parent = nullptr);
    ~LibraryScriptObject_Callable();

    Q_INVOKABLE QJSValue call(int index, QVariantList arguments);

private:
    QJSEngine *mEngine{};
    std::shared_ptr<dllreflect::Library> mLibrary;
};

class LibraryScriptObject final : public QQmlPropertyMap
{
    Q_OBJECT
public:
    explicit LibraryScriptObject(QObject *parent = nullptr);

    bool load(QJSEngine *engine, const QString &fileName,
        const QStringList &searchPaths);
};
