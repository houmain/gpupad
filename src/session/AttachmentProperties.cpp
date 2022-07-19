#include "AttachmentProperties.h"
#include "SessionModel.h"
#include "SessionProperties.h"
#include "ui_AttachmentProperties.h"
#include <QDataWidgetMapper>
#include <QCheckBox>

AttachmentProperties::AttachmentProperties(SessionProperties *sessionProperties)
    : QWidget(sessionProperties)
    , mSessionProperties(*sessionProperties)
    , mUi(new Ui::AttachmentProperties)
{
    mUi->setupUi(this);

    QCoreApplication::setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles, true);
    mUi->tabDepthStencil->setStyleSheet("QTabWidget::pane { border: none; }");

    connect(mUi->texture, &ReferenceComboBox::listRequired,
        [this]() { return mSessionProperties.getItemIds(Item::Type::Texture); });
    connect(mUi->texture, &ReferenceComboBox::textRequired,
        [this](QVariant data) { return mSessionProperties.getItemName(data.toInt()); });
    connect(mUi->texture, &ReferenceComboBox::currentDataChanged,
        this, &AttachmentProperties::updateWidgets);

    fillComboBox<Attachment::BlendEquation>(mUi->blendColorEq);
    mUi->blendAlphaEq->setModel(mUi->blendColorEq->model());

    fillComboBox<Attachment::BlendFactor>(mUi->blendColorSource);
    mUi->blendColorDest->setModel(mUi->blendColorSource->model());

    fillComboBox<Attachment::BlendFactor>(mUi->blendAlphaSource);
    mUi->blendAlphaDest->setModel(mUi->blendAlphaSource->model());

    fillComboBox<Attachment::ComparisonFunc>(mUi->depthComparisonFunc);
    mUi->depthComparisonFunc->removeItem(0);
    mUi->stencilComparisonFunc->setModel(mUi->depthComparisonFunc->model());
    mUi->stencilBackComparisonFunc->setModel(mUi->depthComparisonFunc->model());

    fillComboBox<Attachment::StencilOperation>(mUi->stencilFailOp);
    mUi->stencilDepthFailOp->setModel(mUi->stencilFailOp->model());
    mUi->stencilDepthPassOp->setModel(mUi->stencilFailOp->model());
    mUi->stencilBackFailOp->setModel(mUi->stencilFailOp->model());
    mUi->stencilBackDepthFailOp->setModel(mUi->stencilFailOp->model());
    mUi->stencilBackDepthPassOp->setModel(mUi->stencilFailOp->model());

    // TODO: set layer to -1 when disabled...
    mUi->layered->setVisible(false);

    updateWidgets();
}

AttachmentProperties::~AttachmentProperties()
{
    delete mUi;
}

TextureKind AttachmentProperties::currentTextureKind() const
{
    if (auto texture = mSessionProperties.model().findItem<Texture>(
            mUi->texture->currentData().toInt()))
        return getKind(*texture);
    return { };
}

void AttachmentProperties::showEvent(QShowEvent *event)
{
    updateWidgets();
    QWidget::showEvent(event);
}

void AttachmentProperties::addMappings(QDataWidgetMapper &mapper)
{
    mapper.addMapping(mUi->texture, SessionModel::AttachmentTextureId);
    mapper.addMapping(mUi->level, SessionModel::AttachmentLevel);
    mapper.addMapping(mUi->layer, SessionModel::AttachmentLayer);

    mapper.addMapping(mUi->blendColorEq, SessionModel::AttachmentBlendColorEq);
    mapper.addMapping(mUi->blendColorSource, SessionModel::AttachmentBlendColorSource);
    mapper.addMapping(mUi->blendColorDest, SessionModel::AttachmentBlendColorDest);
    mapper.addMapping(mUi->blendAlphaEq, SessionModel::AttachmentBlendAlphaEq);
    mapper.addMapping(mUi->blendAlphaSource, SessionModel::AttachmentBlendAlphaSource);
    mapper.addMapping(mUi->blendAlphaDest, SessionModel::AttachmentBlendAlphaDest);

    // TODO: generate mask from checkboxes
    //mapper.addMapping(mUi->colorWriteMask, SessionModel::AttachmentColorWriteMask);

    mapper.addMapping(mUi->depthComparisonFunc, SessionModel::AttachmentDepthComparisonFunc);
    mapper.addMapping(mUi->depthOffsetSlope, SessionModel::AttachmentDepthOffsetSlope);
    mapper.addMapping(mUi->depthOffsetConstant, SessionModel::AttachmentDepthOffsetConstant);
    mapper.addMapping(mUi->depthClamp, SessionModel::AttachmentDepthClamp);
    mapper.addMapping(mUi->depthWrite, SessionModel::AttachmentDepthWrite);

    mapper.addMapping(mUi->stencilComparisonFunc, SessionModel::AttachmentStencilFrontComparisonFunc);
    mapper.addMapping(mUi->stencilReference, SessionModel::AttachmentStencilFrontReference);
    mapper.addMapping(mUi->stencilReadMask, SessionModel::AttachmentStencilFrontReadMask);
    mapper.addMapping(mUi->stencilFailOp, SessionModel::AttachmentStencilFrontFailOp);
    mapper.addMapping(mUi->stencilDepthFailOp, SessionModel::AttachmentStencilFrontDepthFailOp);
    mapper.addMapping(mUi->stencilDepthPassOp, SessionModel::AttachmentStencilFrontDepthPassOp);
    mapper.addMapping(mUi->stencilWriteMask, SessionModel::AttachmentStencilFrontWriteMask);

    mapper.addMapping(mUi->stencilBackComparisonFunc, SessionModel::AttachmentStencilBackComparisonFunc);
    mapper.addMapping(mUi->stencilBackReference, SessionModel::AttachmentStencilBackReference);
    mapper.addMapping(mUi->stencilBackReadMask, SessionModel::AttachmentStencilBackReadMask);
    mapper.addMapping(mUi->stencilBackFailOp, SessionModel::AttachmentStencilBackFailOp);
    mapper.addMapping(mUi->stencilBackDepthFailOp, SessionModel::AttachmentStencilBackDepthFailOp);
    mapper.addMapping(mUi->stencilBackDepthPassOp, SessionModel::AttachmentStencilBackDepthPassOp);
    mapper.addMapping(mUi->stencilBackWriteMask, SessionModel::AttachmentStencilBackWriteMask);
}

void AttachmentProperties::updateWidgets()
{
    auto kind = currentTextureKind();
    setFormVisibility(mUi->formLayout, mUi->labelLevel, mUi->level, kind.color);
    setFormVisibility(mUi->formLayout, mUi->labelLayer, mUi->layerWidget, kind.array);

    setFormVisibility(mUi->formLayout, mUi->labelBlendColorEq, mUi->blendColorEq, kind.color);
    setFormVisibility(mUi->formLayout, mUi->labelBlendColorSource, mUi->blendColorSource, kind.color);
    setFormVisibility(mUi->formLayout, mUi->labelBlendColorDest, mUi->blendColorDest, kind.color);
    setFormVisibility(mUi->formLayout, mUi->labelBlendAlphaEq, mUi->blendAlphaEq, kind.color);
    setFormVisibility(mUi->formLayout, mUi->labelBlendAlphaSource, mUi->blendAlphaSource, kind.color);
    setFormVisibility(mUi->formLayout, mUi->labelBlendAlphaDest, mUi->blendAlphaDest, kind.color);

    // TODO: implement
    setFormVisibility(mUi->formLayout, mUi->labelColorWriteMask, mUi->colorWriteMask, false && kind.color);

    static const QStringList tabTitles = {
        mUi->tabDepthStencil->tabText(0),
        mUi->tabDepthStencil->tabText(1),
        mUi->tabDepthStencil->tabText(2)
    };
    mUi->tabDepthStencil->setVisible(kind.depth || kind.stencil);
    mUi->tabDepthStencil->clear();
    if (kind.stencil) {
        mUi->tabDepthStencil->addTab(mUi->tabStencilBack, tabTitles[0]);
        mUi->tabDepthStencil->addTab(mUi->tabStencilFront, tabTitles[1]);
    }
    if (kind.depth)
        mUi->tabDepthStencil->addTab(mUi->tabDepth, tabTitles[2]);
}
