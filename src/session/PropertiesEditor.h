#pragma once

#include "FileDialog.h"
#include "Item.h"
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QMetaEnum>
#include <QScrollArea>

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
} // namespace Ui

class QStackedWidget;
class QDataWidgetMapper;
class QTimer;
class IEditor;
class SessionModel;
class TextureProperties;
class BindingProperties;
class CallProperties;
class AttachmentProperties;
class SessionProperties;

class PropertiesEditor final : public QScrollArea
{
    Q_OBJECT
public:
    explicit PropertiesEditor(QWidget *parent = nullptr);
    ~PropertiesEditor();

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
    QString getItemName(ItemId itemId) const;

private:
    void updateBlockWidgets(const QModelIndex &index);
    void updateTargetWidgets(const QModelIndex &index);
    void deduceBlockOffset();
    void deduceBlockRowCount();
    void deduceShaderType();
    IEditor *openEditor(const FileItem &fileItem);
    void fillComboBoxes();
    void switchToCurrentFileItemDirectory();

    SessionModel &mModel;
    QStackedWidget *mStack;
    QDataWidgetMapper *mMapper;
    QTimer *mSubmitTimer;
    QWidget *mRootProperties{};
    SessionProperties *mSessionProperties{};
    std::unique_ptr<Ui::GroupProperties> mGroupProperties;
    std::unique_ptr<Ui::BlockProperties> mBlockProperties;
    std::unique_ptr<Ui::BufferProperties> mBufferProperties;
    std::unique_ptr<Ui::FieldProperties> mFieldProperties;
    TextureProperties *mTextureProperties{};
    std::unique_ptr<Ui::ProgramProperties> mProgramProperties;
    std::unique_ptr<Ui::ShaderProperties> mShaderProperties;
    BindingProperties *mBindingProperties{};
    std::unique_ptr<Ui::AttributeProperties> mAttributeProperties;
    std::unique_ptr<Ui::TargetProperties> mTargetProperties;
    std::unique_ptr<Ui::StreamProperties> mStreamProperties;
    AttachmentProperties *mAttachmentProperties{};
    CallProperties *mCallProperties{};
    std::unique_ptr<Ui::ScriptProperties> mScriptProperties;
};

QString splitPascalCase(QString str);
void setFormVisibility(QFormLayout *layout, QLabel *label, QWidget *widget,
    bool visible);
void setFormEnabled(QLabel *label, QWidget *widget, bool enabled);

template <typename T>
void fillComboBox(QComboBox *c, bool keepPascalCase = false)
{
    auto metaType = QMetaEnum::fromType<T>();
    for (auto i = 0; i < metaType.keyCount(); ++i) {
        auto key = QString(metaType.key(i));
        if (!keepPascalCase)
            key = splitPascalCase(key);
        c->addItem(key, metaType.value(i));
    }
}

template <typename T>
void fillComboBox(QComboBox *c,
    std::initializer_list<std::pair<const char *, T>> items)
{
    for (const auto &kv : items)
        if (kv.first)
            c->addItem(kv.first, kv.second);
}
