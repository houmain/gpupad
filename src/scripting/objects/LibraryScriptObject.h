#pragma once

#include <QQmlPropertyMap>
#include <QJSValue>
#include <memory>

namespace dllreflect {
    class Library;
}

class LibraryScriptObject_Callable : public QObject
{
    Q_OBJECT
public:
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
