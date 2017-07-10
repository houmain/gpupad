#ifndef SESSIONPROPERTIES_H
#define SESSIONPROPERTIES_H

#include "Item.h"
#include "FileDialog.h"
#include <QScrollArea>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>

namespace Ui {
class GroupProperties;
class BufferProperties;
class ColumnProperties;
class ImageProperties;
class SamplerProperties;
class ProgramProperties;
class ShaderProperties;
class VertexStreamProperties;
class AttributeProperties;
class FramebufferProperties;
class AttachmentProperties;
class ScriptProperties;
}

class QStackedWidget;
class QDataWidgetMapper;
class QTimer;
class SessionModel;
class TextureProperties;
class BindingProperties;
class CallProperties;

template <typename T>
void fill(QComboBox *c, std::initializer_list<std::pair<const char*, T>> items)
{
    for (const auto &kv : items)
        c->addItem(kv.first, kv.second);
}

inline void setFormVisibility(QFormLayout* layout, QLabel* label,
    QWidget* widget, bool visible)
{
    layout->removeWidget(label);
    layout->removeWidget(widget);
    if (visible)
        layout->addRow(label, widget);
    label->setVisible(visible);
    widget->setVisible(visible);
}

class SessionProperties : public QScrollArea
{
    Q_OBJECT
public:
    explicit SessionProperties(QWidget *parent = 0);
    ~SessionProperties();

    SessionModel &model() { return mModel; }
    QModelIndex currentModelIndex(int column = 0) const;
    void setCurrentModelIndex(const QModelIndex &index);
    void setCurrentItemFile(QString fileName);
    void selectCurrentItemFile(FileDialog::Options options);
    QVariantList getFileNames(ItemType type, bool addNull = false) const;
    QVariantList getItemIds(ItemType type, bool addNull = false) const;
    QString findItemName(ItemId itemId) const;

private slots:
    void updateImageWidgets(const QModelIndex &index);

private:
    void fillComboBoxes();
    QVariantList getColumnIds(ItemId bufferId) const;

    SessionModel &mModel;
    QStackedWidget *mStack;
    QDataWidgetMapper *mMapper;
    QTimer *mSubmitTimer;
    QScopedPointer<Ui::GroupProperties> mGroupProperties;
    QScopedPointer<Ui::BufferProperties> mBufferProperties;
    QScopedPointer<Ui::ColumnProperties> mColumnProperties;
    TextureProperties *mTextureProperties{ };
    QScopedPointer<Ui::ImageProperties> mImageProperties;
    QScopedPointer<Ui::SamplerProperties> mSamplerProperties;
    QScopedPointer<Ui::ProgramProperties> mProgramProperties;
    QScopedPointer<Ui::ShaderProperties> mShaderProperties;
    BindingProperties *mBindingProperties{ };
    QScopedPointer<Ui::AttributeProperties> mAttributeProperties;
    QScopedPointer<Ui::FramebufferProperties> mFramebufferProperties;
    QScopedPointer<Ui::VertexStreamProperties> mVertexStreamProperties;
    QScopedPointer<Ui::AttachmentProperties> mAttachmentProperties;
    CallProperties *mCallProperties{ };
    QScopedPointer<Ui::ScriptProperties> mScriptProperties;
};

#endif // SESSIONPROPERTIES_H
