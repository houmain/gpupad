#ifndef SESSIONPROPERTIES_H
#define SESSIONPROPERTIES_H

#include "Item.h"
#include "FileDialog.h"
#include <QScrollArea>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QMetaEnum>

namespace Ui {
class GroupProperties;
class BufferProperties;
class BlockProperties;
class FieldProperties;
class ProgramProperties;
class ShaderProperties;
class StreamProperties;
class AttributeProperties;
class TargetProperties;
class ScriptProperties;
}

class QStackedWidget;
class QDataWidgetMapper;
class QTimer;
class IEditor;
class SessionModel;
class TextureProperties;
class BindingProperties;
class CallProperties;
class AttachmentProperties;

class SessionProperties final : public QScrollArea
{
    Q_OBJECT
public:
    explicit SessionProperties(QWidget *parent = nullptr);
    ~SessionProperties();

    SessionModel &model() { return mModel; }
    void updateModel();
    QModelIndex currentModelIndex(int column = 0) const;
    QString currentItemName() const;
    QString currentItemFileName() const;
    IEditor *openItemEditor(const QModelIndex &index);
    void setCurrentModelIndex(const QModelIndex &index);
    void setCurrentItemFile(const QString &fileName);
    void saveCurrentItemFileAs(FileDialog::Options options);
    void openCurrentItemFile(FileDialog::Options options);
    QVariantList getFileNames(Item::Type type, bool addNull = false) const;
    QVariantList getItemIds(Item::Type type, bool addNull = false) const;
    QString findItemName(ItemId itemId) const;

private:
    void updateBlockWidgets(const QModelIndex &index);
    void updateTargetWidgets(const QModelIndex &index);
    void updateScriptWidgets(const QModelIndex &index);
    void updateScriptWidgets(bool hasFile);
    void deduceBlockOffset();
    void deduceBlockRowCount();
    IEditor* openEditor(const FileItem &fileItem);
    void fillComboBoxes();

    SessionModel &mModel;
    QStackedWidget *mStack;
    QDataWidgetMapper *mMapper;
    QTimer *mSubmitTimer;
    QScopedPointer<Ui::GroupProperties> mGroupProperties;
    QScopedPointer<Ui::BlockProperties> mBlockProperties;
    QScopedPointer<Ui::BufferProperties> mBufferProperties;
    QScopedPointer<Ui::FieldProperties> mFieldProperties;
    TextureProperties *mTextureProperties{ };
    QScopedPointer<Ui::ProgramProperties> mProgramProperties;
    QScopedPointer<Ui::ShaderProperties> mShaderProperties;
    BindingProperties *mBindingProperties{ };
    QScopedPointer<Ui::AttributeProperties> mAttributeProperties;
    QScopedPointer<Ui::TargetProperties> mTargetProperties;
    QScopedPointer<Ui::StreamProperties> mStreamProperties;
    AttachmentProperties *mAttachmentProperties{ };
    CallProperties *mCallProperties{ };
    QScopedPointer<Ui::ScriptProperties> mScriptProperties;
};

QString splitPascalCase(QString str);
void setFormVisibility(QFormLayout *layout, QLabel *label,
    QWidget *widget, bool visible);
void setFormEnabled(QLabel *label,
    QWidget *widget, bool enabled);

template <typename T>
void fillComboBox(QComboBox *c)
{
    auto metaType = QMetaEnum::fromType<T>();
    for (auto i = 0; i < metaType.keyCount(); ++i)
        c->addItem(splitPascalCase(metaType.key(i)),
            metaType.value(i));
}

template <typename T>
void fillComboBox(QComboBox *c, std::initializer_list<std::pair<const char*, T>> items)
{
    for (const auto &kv : items)
        if (kv.first)
            c->addItem(kv.first, kv.second);
}

#endif // SESSIONPROPERTIES_H
