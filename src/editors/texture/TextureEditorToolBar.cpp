
#include "TextureEditorToolBar.h"
#include "ui_TextureEditorToolBar.h"

TextureEditorToolBar::TextureEditorToolBar(QWidget *parent)
    : QWidget(parent)
    , mUi(new Ui::TextureEditorToolBar())
{
    mUi->setupUi(this);

    connect(mUi->zoom, qOverload<int>(&QSpinBox::valueChanged),
        this, &TextureEditorToolBar::zoomChanged);
    connect(mUi->zoomToFit, &QToolButton::toggled,
        this, &TextureEditorToolBar::zoomToFitChanged);
    connect(mUi->level, qOverload<double>(&QDoubleSpinBox::valueChanged),
        [&](double level) { Q_EMIT levelChanged(level); });
    connect(mUi->layer, qOverload<double>(&QDoubleSpinBox::valueChanged),
        [&](double layer) { Q_EMIT layerChanged(layer); });
    connect(mUi->sample, qOverload<int>(&QSpinBox::valueChanged),
        this, &TextureEditorToolBar::sampleChanged);
    connect(mUi->face, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &TextureEditorToolBar::faceChanged);
    connect(mUi->filter, &QCheckBox::stateChanged,
        this, &TextureEditorToolBar::filterStateChanged);
    connect(mUi->flipVertically, &QCheckBox::stateChanged,
        [&](int state) { Q_EMIT flipVerticallyChanged(state != 0); });
}

TextureEditorToolBar::~TextureEditorToolBar() 
{
    delete mUi;
}

void TextureEditorToolBar::setZoom(int zoom)
{
    mUi->zoom->setValue(zoom);
}

void TextureEditorToolBar::setZoomToFit(bool fit)
{
    mUi->zoomToFit->setChecked(fit);
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
    const auto hasLayers = (maxLayer > 1 || maxDepth > 1);
    mUi->labelLayer->setVisible(hasLayers);
    mUi->layer->setVisible(hasLayers);
    mUi->layer->setMinimum(0);
    mUi->layer->setMaximum(std::max(maxLayer, maxDepth) - 1);
}

void TextureEditorToolBar::setLayer(float layer)
{
    mUi->layer->setValue(layer);
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

void TextureEditorToolBar::filterStateChanged(int state)
{
    const auto filter = (state != 0);
    mUi->level->setDecimals(filter ? 2 : 0);
    mUi->level->setSingleStep(filter ? 0.25 : 1.0);
    mUi->layer->setDecimals(filter ? 2 : 0);
    mUi->layer->setSingleStep(filter ? 0.25 : 1.0);
    Q_EMIT filterChanged(filter);
}
