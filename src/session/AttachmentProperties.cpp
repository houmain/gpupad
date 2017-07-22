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
    mapper.addMapping(mUi->depthNear, SessionModel::AttachmentDepthNear);
    mapper.addMapping(mUi->depthFar, SessionModel::AttachmentDepthFar);
    mapper.addMapping(mUi->depthBiasSlope, SessionModel::AttachmentDepthBiasSlope);
    mapper.addMapping(mUi->depthBiasConst, SessionModel::AttachmentDepthBiasConst);
    mapper.addMapping(mUi->depthClamp, SessionModel::AttachmentDepthClamp);
    mapper.addMapping(mUi->depthWrite, SessionModel::AttachmentDepthWrite);

    // TODO: switch bindings between front and back
    mapper.addMapping(mUi->stencilCompareFunc, SessionModel::AttachmentStencilFrontCompareFunc);
    mapper.addMapping(mUi->stencilReference, SessionModel::AttachmentStencilFrontReference);
    mapper.addMapping(mUi->stencilReadMask, SessionModel::AttachmentStencilFrontReadMask);
    mapper.addMapping(mUi->stencilFailOp, SessionModel::AttachmentStencilFrontFailOp);
    mapper.addMapping(mUi->stencilDepthFailOp, SessionModel::AttachmentStencilFrontDepthFailOp);
    mapper.addMapping(mUi->stencilDepthPassOp, SessionModel::AttachmentStencilFrontDepthPassOp);
    mapper.addMapping(mUi->stencilWriteMask, SessionModel::AttachmentStencilFrontWriteMask);
}

void AttachmentProperties::updateWidgets()
{
    const auto type = currentTextureType();
    const auto color = (type == Texture::Type::Color);
    const auto depth = (type == Texture::Type::Depth || type == Texture::Type::DepthStencil);
    const auto stencil = (type == Texture::Type::Stencil || type == Texture::Type::DepthStencil);

    setFormVisibility(mUi->formLayout, mUi->labelBlendColorEq, mUi->blendColorEq, color);
    setFormVisibility(mUi->formLayout, mUi->labelBlendColorSource, mUi->blendColorSource, color);
    setFormVisibility(mUi->formLayout, mUi->labelBlendColorDest, mUi->blendColorDest, color);
    setFormVisibility(mUi->formLayout, mUi->labelBlendAlphaEq, mUi->blendAlphaEq, color);
    setFormVisibility(mUi->formLayout, mUi->labelBlendAlphaSource, mUi->blendAlphaSource, color);
    setFormVisibility(mUi->formLayout, mUi->labelBlendAlphaDest, mUi->blendAlphaDest, color);
    setFormVisibility(mUi->formLayout, mUi->labelColorWriteMask, mUi->colorWriteMask, color);

    setFormVisibility(mUi->formLayout, mUi->labelDepthCompareFunc, mUi->depthCompareFunc, depth);
    setFormVisibility(mUi->formLayout, mUi->labelDepthNear, mUi->depthNear, depth);
    setFormVisibility(mUi->formLayout, mUi->labelDepthFar, mUi->depthFar, depth);
    setFormVisibility(mUi->formLayout, mUi->labelDepthBiasSlope, mUi->depthBiasSlope, depth);
    setFormVisibility(mUi->formLayout, mUi->labelDepthBiasConst, mUi->depthBiasConst, depth);
    setFormVisibility(mUi->formLayout, mUi->labelDepthClamp, mUi->depthClamp, depth);
    setFormVisibility(mUi->formLayout, mUi->labelDepthWrite, mUi->depthWrite, depth);

    setFormVisibility(mUi->formLayout, mUi->labelStencilSide, mUi->tabStencilSide, stencil);
    setFormVisibility(mUi->formLayout, mUi->labelStencilReference, mUi->stencilReference, stencil);
    setFormVisibility(mUi->formLayout, mUi->labelStencilReadMask, mUi->stencilReadMask, stencil);
    setFormVisibility(mUi->formLayout, mUi->labelStencilCompareFunc, mUi->stencilCompareFunc, stencil);
    setFormVisibility(mUi->formLayout, mUi->labelStencilFailOp, mUi->stencilFailOp, stencil);
    setFormVisibility(mUi->formLayout, mUi->labelStencilDepthFailOp, mUi->stencilDepthFailOp, stencil);
    setFormVisibility(mUi->formLayout, mUi->labelStencilDepthPassOp, mUi->stencilDepthPassOp, stencil);
    setFormVisibility(mUi->formLayout, mUi->labelStencilWriteMask, mUi->stencilWriteMask, stencil);
}
