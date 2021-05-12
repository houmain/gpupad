
#include "BinaryEditorToolBar.h"
#include "ui_BinaryEditorToolBar.h"

BinaryEditorToolBar::BinaryEditorToolBar(QWidget *parent)
    : QWidget(parent)
    , mUi(new Ui::BinaryEditorToolBar())
{
    mUi->setupUi(this);

    connect(mUi->block,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this, &BinaryEditorToolBar::blockIndexChanged);
}

BinaryEditorToolBar::~BinaryEditorToolBar() 
{
    delete mUi;
}

void BinaryEditorToolBar::setBlocks(const QList<BinaryEditor::Block> &blocks)
{
    mUi->block->clear();
    for (const auto &block : blocks)
        mUi->block->addItem(block.name);
    mUi->labelBlock->setVisible(!blocks.empty());
    mUi->block->setVisible(!blocks.empty());
}

void BinaryEditorToolBar::setCurrentBlockIndex(int index)
{
    mUi->block->setCurrentIndex(index);
}
