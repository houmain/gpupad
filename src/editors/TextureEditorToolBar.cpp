
#include "TextureEditorToolBar.h"
#include "ui_TextureEditorToolBar.h"

TextureEditorToolBar::TextureEditorToolBar(QWidget *parent)
    : QWidget(parent)
    , mUi(new Ui::TextureEditorToolBar())
{
    mUi->setupUi(this);

    connect(mUi->level, qOverload<double>(&QDoubleSpinBox::valueChanged),
        [&](double level) { Q_EMIT levelChanged(level); });
    connect(mUi->z, qOverload<double>(&QDoubleSpinBox::valueChanged),
        [&](double layer) { Q_EMIT layerChanged(layer); });
    connect(mUi->layer, qOverload<int>(&QSpinBox::valueChanged),
        [&](int layer) { Q_EMIT layerChanged(layer); });
    connect(mUi->sample, qOverload<int>(&QSpinBox::valueChanged),
        this, &TextureEditorToolBar::sampleChanged);
    connect(mUi->face, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &TextureEditorToolBar::faceChanged);
    connect(mUi->filter, &QCheckBox::stateChanged,
        [&](int state) { Q_EMIT filterChanged(state != 0); });
    connect(mUi->flipVertically, &QCheckBox::stateChanged,
        [&](int state) { Q_EMIT flipVerticallyChanged(state != 0); });
}

TextureEditorToolBar::~TextureEditorToolBar() 
{
    delete mUi;
}

void TextureEditorToolBar::setMaxLevel(int maxLevel)
{
    mUi->level->setMaximum(maxLevel);
    mUi->labelLevel->setVisible(maxLevel);
    mUi->level->setVisible(maxLevel);
}

void TextureEditorToolBar::setLevel(float level)
{
    mUi->level->setValue(level);
}

void TextureEditorToolBar::setMaxLayer(int maxLayer, int maxDepth)
{
    mUi->layer->setMaximum(maxLayer);
    mUi->labelLayer->setVisible(maxLayer);
    mUi->layer->setVisible(maxLayer);

    const auto hasDepth = (maxDepth > 1);
    mUi->labelZ->setVisible(hasDepth);
    mUi->z->setVisible(hasDepth);
    mUi->z->setSingleStep(hasDepth ? 0.5 / (maxDepth - 1) : 1);
}

void TextureEditorToolBar::setLayer(float layer)
{
    mUi->z->setValue(layer);
    mUi->layer->setValue(static_cast<int>(layer));
}

void TextureEditorToolBar::setMaxSample(int maxSample)
{
    mUi->sample->setMinimum(-1);
    mUi->sample->setMaximum(maxSample);
    mUi->labelSample->setVisible(maxSample);
    mUi->sample->setVisible(maxSample);
}

void TextureEditorToolBar::setSample(int sample)
{
    mUi->sample->setValue(sample);
}

void TextureEditorToolBar::setMaxFace(int maxFace)
{
    mUi->labelFace->setVisible(maxFace);
    mUi->face->setVisible(maxFace);
}

void TextureEditorToolBar::setFace(int face)
{
    mUi->face->setCurrentIndex(face);
}

void TextureEditorToolBar::setCanFilter(bool canFilter)
{
    mUi->filter->setVisible(canFilter);
}

void TextureEditorToolBar::setFilter(bool filter)
{
    mUi->filter->setChecked(filter);
}

void TextureEditorToolBar::setCanFlipVertically(bool canFlip)
{
    mUi->flipVertically->setVisible(canFlip);
}

void TextureEditorToolBar::setFlipVertically(bool flip)
{
    mUi->flipVertically->setChecked(flip);
}
