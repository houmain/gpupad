#pragma once

#include <QWidget>
#include "BinaryEditor.h"

namespace Ui { class BinaryEditorToolBar; }

class BinaryEditorToolBar final : public QWidget
{
    Q_OBJECT
public:
    explicit BinaryEditorToolBar(QWidget *parent);
    ~BinaryEditorToolBar() override;

    void setBlocks(const QList<BinaryEditor::Block> &blocks);
    void setCurrentBlockIndex(int index);

Q_SIGNALS:
    void blockIndexChanged(int index);

private:
    Ui::BinaryEditorToolBar *mUi;
};


