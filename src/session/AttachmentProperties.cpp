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
        { "Min", Attachment::Min },
        { "Max", Attachment::Max },
        { "Subtract", Attachment::Subtract },
        { "Reverse Subtract", Attachment::ReverseSubtract },
    });
    mUi->blendAlphaEq->setModel(mUi->blendColorEq->model());

    fill<Attachment::BlendFactor>(mUi->blendColorSource, {
        { "Zero", Attachment::Zero },
        { "One", Attachment::One },
        { "Source Color", Attachment::SrcColor },
        { "1 - Source Color", Attachment::OneMinusSrcColor },
        { "Source Alpha", Attachment::SrcAlpha },
        { "1 - Source Alpha", Attachment::OneMinusSrcAlpha },
        {  "Destination Alpha", Attachment::DstAlpha },
        { "1 - Destination Alpha", Attachment::OneMinusDstAlpha },
        { "Destination Color", Attachment::DstColor },
        { "1 - Destination Color", Attachment::OneMinusDstColor },
        { "Source Alpha Saturate", Attachment::SrcAlphaSaturate },
        { "Constant Color", Attachment::ConstantColor },
        { "1 - Constant Color", Attachment::OneMinusConstantColor },
        { "Constant Alpha", Attachment::ConstantAlpha },
        { "1 - Constant Alpha", Attachment::OneMinusConstantAlpha },
    });
    mUi->blendColorDest->setModel(mUi->blendColorSource->model());
    mUi->blendAlphaSource->setModel(mUi->blendColorSource->model());
    mUi->blendAlphaDest->setModel(mUi->blendColorSource->model());

    fill<Attachment::ComparisonFunction>(mUi->depthCompareFunc, {
        { "Always", Attachment::Always },
        { "Never", Attachment::Never },
        { "Less", Attachment::Less },
        { "Equal", Attachment::Equal },
        { "Less Equal", Attachment::LessEqual },
        { "Greater", Attachment::Greater },
        { "Not Equal", Attachment::NotEqual },
        { "Greater Equal", Attachment::GreaterEqual },
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

Texture::Type AttachmentProperties::currentType() const
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
    mapper.addMapping(mUi->colorWriteMask, SessionModel::AttachmentColorWriteMask);

    mapper.addMapping(mUi->depthCompareFunc, SessionModel::AttachmentDepthCompareFunc);
    mapper.addMapping(mUi->depthBias, SessionModel::AttachmentDepthBias);
    mapper.addMapping(mUi->depthClamp, SessionModel::AttachmentDepthClamp);
    mapper.addMapping(mUi->depthWrite, SessionModel::AttachmentDepthWrite);

    // TODO: switch bindings between front and back
    mapper.addMapping(mUi->stencilCompareFunc, SessionModel::AttachmentStencilFrontCompareFunc);
    mapper.addMapping(mUi->stencilReference, SessionModel::AttachmentStencilFrontReference);
    mapper.addMapping(mUi->stencilFailOp, SessionModel::AttachmentStencilFrontFailOp);
    mapper.addMapping(mUi->stencilDepthFailOp, SessionModel::AttachmentStencilFrontDepthFailOp);
    mapper.addMapping(mUi->stencilDepthPassOp, SessionModel::AttachmentStencilFrontDepthPassOp);
    mapper.addMapping(mUi->stencilWriteMask, SessionModel::AttachmentStencilFrontWriteMask);
}

void AttachmentProperties::updateWidgets()
{
    const auto type = currentType();
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
    setFormVisibility(mUi->formLayout, mUi->labelDepthBias, mUi->depthBias, depth);
    setFormVisibility(mUi->formLayout, mUi->labelDepthClamp, mUi->depthClamp, depth);
    setFormVisibility(mUi->formLayout, mUi->labelDepthWrite, mUi->depthWrite, depth);

    setFormVisibility(mUi->formLayout, mUi->labelStencilSide, mUi->tabStencilSide, stencil);
    setFormVisibility(mUi->formLayout, mUi->labelStencilCompareFunc, mUi->stencilCompareFunc, stencil);
    setFormVisibility(mUi->formLayout, mUi->labelStencilReference, mUi->stencilReference, stencil);
    setFormVisibility(mUi->formLayout, mUi->labelStencilFailOp, mUi->stencilFailOp, stencil);
    setFormVisibility(mUi->formLayout, mUi->labelStencilDepthPassOp, mUi->stencilDepthPassOp, stencil);
    setFormVisibility(mUi->formLayout, mUi->labelStencilDepthFailOp, mUi->stencilDepthFailOp, stencil);
    setFormVisibility(mUi->formLayout, mUi->labelStencilWriteMask, mUi->stencilWriteMask, stencil);
}
