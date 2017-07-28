#include "AttachmentProperties.h"
#include "SessionModel.h"
#include "SessionProperties.h"
#include "ui_AttachmentProperties.h"
#include <QDataWidgetMapper>

AttachmentProperties::AttachmentProperties(SessionProperties *sessionProperties)
    : QWidget(sessionProperties)
    , mSessionProperties(*sessionProperties)
    , mUi(new Ui::AttachmentProperties)
{
    mUi->setupUi(this);

    connect(mUi->texture, &ReferenceComboBox::listRequired,
        [this]() { return mSessionProperties.getItemIds(ItemType::Texture); });
    connect(mUi->texture, &ReferenceComboBox::textRequired,
        [this](QVariant data) { return mSessionProperties.findItemName(data.toInt()); });
    connect(mUi->texture, &ReferenceComboBox::currentDataChanged,
        this, &AttachmentProperties::updateWidgets);

    fill<Attachment::BlendEquation>(mUi->blendColorEq, {
        { "Add", Attachment::Add },
        { "Minimum", Attachment::Min },
        { "Maximum", Attachment::Max },
        { "Subtract", Attachment::Subtract },
        { "Reverse Subtract", Attachment::ReverseSubtract },
    });
    mUi->blendAlphaEq->setModel(mUi->blendColorEq->model());

    fill<Attachment::BlendFactor>(mUi->blendColorSource, {
        { "Zero", Attachment::Zero },
        { "One", Attachment::One },
        { "Source", Attachment::SrcColor },
        { "One Minus Source", Attachment::OneMinusSrcColor },
        { "Source Alpha", Attachment::SrcAlpha },
        { "One Minus Source Alpha", Attachment::OneMinusSrcAlpha },
        { "Source Alpha Saturate", Attachment::SrcAlphaSaturate },
        { "Destination", Attachment::DstColor },
        { "One Minus Destination", Attachment::OneMinusDstColor },
        { "Destination Alpha", Attachment::DstAlpha },
        { "One Minus Destination Alpha", Attachment::OneMinusDstAlpha },
        { "Constant", Attachment::ConstantColor },
        { "One Minus Constant", Attachment::OneMinusConstantColor },
        { "Constant Alpha", Attachment::ConstantAlpha },
        { "One Minus Constant Alpha", Attachment::OneMinusConstantAlpha },
    });
    mUi->blendColorDest->setModel(mUi->blendColorSource->model());

    fill<Attachment::BlendFactor>(mUi->blendAlphaSource, {
        { "Zero", Attachment::Zero },
        { "One", Attachment::One },
        { "Source", Attachment::SrcColor },
        { "One Minus Source", Attachment::OneMinusSrcColor },
        { "Destination", Attachment::DstColor },
        { "One Minus Destination", Attachment::OneMinusDstColor },
        { "Constant", Attachment::ConstantColor },
        { "One Minus Constant", Attachment::OneMinusConstantColor },
    });
    mUi->blendAlphaDest->setModel(mUi->blendAlphaSource->model());

    fill<Attachment::ComparisonFunction>(mUi->depthCompareFunc, {
        { "Always", Attachment::Always },
        { "Less", Attachment::Less },
        { "Less Equal", Attachment::LessEqual },
        { "Equal", Attachment::Equal },
        { "Greater Equal", Attachment::GreaterEqual },
        { "Greater", Attachment::Greater },
        { "Not Equal", Attachment::NotEqual },
        { "Never", Attachment::Never },
    });
    mUi->stencilCompareFunc->setModel(mUi->depthCompareFunc->model());
    mUi->stencilBackCompareFunc->setModel(mUi->depthCompareFunc->model());

    fill<Attachment::StencilOperation>(mUi->stencilFailOp, {
        { "Keep", Attachment::Keep },
        { "Reset", Attachment::Reset },
        { "Replace", Attachment::Replace },
        { "Increment", Attachment::Increment },
        { "Increment Wrap", Attachment::IncrementWrap },
        { "Decrement", Attachment::Decrement },
        { "Decrement Wrap", Attachment::DecrementWrap },
        { "Invert", Attachment::Invert },
    });
    mUi->stencilDepthFailOp->setModel(mUi->stencilFailOp->model());
    mUi->stencilDepthPassOp->setModel(mUi->stencilFailOp->model());
    mUi->stencilBackFailOp->setModel(mUi->stencilFailOp->model());
    mUi->stencilBackDepthFailOp->setModel(mUi->stencilFailOp->model());
    mUi->stencilBackDepthPassOp->setModel(mUi->stencilFailOp->model());

    updateWidgets();
}

AttachmentProperties::~AttachmentProperties()
{
    delete mUi;
}

Texture::Type AttachmentProperties::currentTextureType() const
{
    if (!mUi->texture->currentData().isNull())
        if (auto texture = castItem<Texture>(mSessionProperties.model().findItem(
                mUi->texture->currentData().toInt())))
            return texture->type();

    return Texture::Type::None;
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

    mapper.addMapping(mUi->blendColorEq, SessionModel::AttachmentBlendColorEq);
    mapper.addMapping(mUi->blendColorSource, SessionModel::AttachmentBlendColorSource);
    mapper.addMapping(mUi->blendColorDest, SessionModel::AttachmentBlendColorDest);
    mapper.addMapping(mUi->blendAlphaEq, SessionModel::AttachmentBlendAlphaEq);
    mapper.addMapping(mUi->blendAlphaSource, SessionModel::AttachmentBlendAlphaSource);
    mapper.addMapping(mUi->blendAlphaDest, SessionModel::AttachmentBlendAlphaDest);

    // TODO: generate mask from checkboxes
    //mapper.addMapping(mUi->colorWriteMask, SessionModel::AttachmentColorWriteMask);

    mapper.addMapping(mUi->depthCompareFunc, SessionModel::AttachmentDepthCompareFunc);
    mapper.addMapping(mUi->depthBiasSlope, SessionModel::AttachmentDepthBiasSlope);
    mapper.addMapping(mUi->depthBiasConst, SessionModel::AttachmentDepthBiasConst);
    mapper.addMapping(mUi->depthClamp, SessionModel::AttachmentDepthClamp);
    mapper.addMapping(mUi->depthWrite, SessionModel::AttachmentDepthWrite);

    mapper.addMapping(mUi->stencilCompareFunc, SessionModel::AttachmentStencilFrontCompareFunc);
    mapper.addMapping(mUi->stencilReference, SessionModel::AttachmentStencilFrontReference);
    mapper.addMapping(mUi->stencilReadMask, SessionModel::AttachmentStencilFrontReadMask);
    mapper.addMapping(mUi->stencilFailOp, SessionModel::AttachmentStencilFrontFailOp);
    mapper.addMapping(mUi->stencilDepthFailOp, SessionModel::AttachmentStencilFrontDepthFailOp);
    mapper.addMapping(mUi->stencilDepthPassOp, SessionModel::AttachmentStencilFrontDepthPassOp);
    mapper.addMapping(mUi->stencilWriteMask, SessionModel::AttachmentStencilFrontWriteMask);

    mapper.addMapping(mUi->stencilBackCompareFunc, SessionModel::AttachmentStencilBackCompareFunc);
    mapper.addMapping(mUi->stencilBackReference, SessionModel::AttachmentStencilBackReference);
    mapper.addMapping(mUi->stencilBackReadMask, SessionModel::AttachmentStencilBackReadMask);
    mapper.addMapping(mUi->stencilBackFailOp, SessionModel::AttachmentStencilBackFailOp);
    mapper.addMapping(mUi->stencilBackDepthFailOp, SessionModel::AttachmentStencilBackDepthFailOp);
    mapper.addMapping(mUi->stencilBackDepthPassOp, SessionModel::AttachmentStencilBackDepthPassOp);
    mapper.addMapping(mUi->stencilBackWriteMask, SessionModel::AttachmentStencilBackWriteMask);
}

void AttachmentProperties::updateWidgets()
{
    const auto type = currentTextureType();
    const auto color = (type == Texture::Type::Color);
    const auto depth = (type == Texture::Type::Depth || type == Texture::Type::DepthStencil);
    const auto stencil = (type == Texture::Type::Stencil || type == Texture::Type::DepthStencil);

    setFormVisibility(mUi->formLayout, mUi->labelLevel, mUi->level, color);

    setFormVisibility(mUi->formLayout, mUi->labelBlendColorEq, mUi->blendColorEq, color);
    setFormVisibility(mUi->formLayout, mUi->labelBlendColorSource, mUi->blendColorSource, color);
    setFormVisibility(mUi->formLayout, mUi->labelBlendColorDest, mUi->blendColorDest, color);
    setFormVisibility(mUi->formLayout, mUi->labelBlendAlphaEq, mUi->blendAlphaEq, color);
    setFormVisibility(mUi->formLayout, mUi->labelBlendAlphaSource, mUi->blendAlphaSource, color);
    setFormVisibility(mUi->formLayout, mUi->labelBlendAlphaDest, mUi->blendAlphaDest, color);
    setFormVisibility(mUi->formLayout, mUi->labelColorWriteMask, mUi->colorWriteMask, color);

    static const QList<QString> tabTitles = {
        mUi->tabDepthStencil->tabText(0),
        mUi->tabDepthStencil->tabText(1),
        mUi->tabDepthStencil->tabText(2)
    };
    mUi->tabDepthStencil->setVisible(depth || stencil);
    mUi->tabDepthStencil->clear();
    if (depth)
        mUi->tabDepthStencil->addTab(mUi->tabDepth, tabTitles[0]);
    if (stencil) {
        mUi->tabDepthStencil->addTab(mUi->tabStencilFront, tabTitles[1]);
        mUi->tabDepthStencil->addTab(mUi->tabStencilBack, tabTitles[2]);
    }
}
