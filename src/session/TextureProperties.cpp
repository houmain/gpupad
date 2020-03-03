#include "TextureProperties.h"
#include "SessionProperties.h"
#include "ui_TextureProperties.h"
#include "Singletons.h"
#include "session/SessionModel.h"
#include "editors/EditorManager.h"
#include "FileCache.h"
#include <QDataWidgetMapper>

namespace {
    enum FormatType
    {
        R,
        RG,
        RGB,
        RGBA,
        R_Integer,
        RG_Integer,
        RGB_Integer,
        RGBA_Integer,
        DepthStencil,
    };

    enum FormatData
    {
        Normalized8,
        Normalized16,
        SignedNormalized8,
        SignedNormalized16,
        UnsignedInt8,
        SignedInt8,
        UnsignedInt16,
        SignedInt16,
        UnsignedInt32,
        SignedInt32,
        Float16,
        Float32,
        Normalized332,
        Normalized4,
        Normalized555_1,
        Normalized565,
        Normalized9_E5,
        sRGB8,
        Float11_11_10,
        UnsignedInt10_10_10_2,
        D16,
        D24,
        D32,
        D32F,
        D24_S8,
        D32F_S8,
        S8,
    };

    std::map<Texture::Format, std::pair<FormatType, FormatData>> gTypeDataByFormat;
    std::map<std::pair<FormatType, FormatData>, Texture::Format> gFormatByTypeData;
    std::map<FormatType, std::vector<FormatData>> gDataByType;

    void buildFormatMappings()
    {
#define ADD(FORMAT, TYPE, DATA) \
            gTypeDataByFormat[FORMAT] = std::make_pair(TYPE, DATA); \
            gFormatByTypeData[std::make_pair(TYPE, DATA)] = FORMAT; \
            gDataByType[TYPE].push_back(DATA);

        ADD(Texture::Format::R8_UNorm, FormatType::R, FormatData::Normalized8)
        ADD(Texture::Format::R8_SNorm, FormatType::R, FormatData::SignedNormalized8)
        ADD(Texture::Format::R16_UNorm, FormatType::R, FormatData::Normalized16)
        ADD(Texture::Format::R16_SNorm, FormatType::R, FormatData::SignedNormalized16)
        ADD(Texture::Format::R16F, FormatType::R, FormatData::Float16)
        ADD(Texture::Format::R32F, FormatType::R, FormatData::Float32)

        ADD(Texture::Format::R8I, FormatType::R_Integer, FormatData::SignedInt8)
        ADD(Texture::Format::R8U, FormatType::R_Integer, FormatData::UnsignedInt8)
        ADD(Texture::Format::R16I, FormatType::R_Integer, FormatData::SignedInt16)
        ADD(Texture::Format::R16U, FormatType::R_Integer, FormatData::UnsignedInt16)
        ADD(Texture::Format::R32I, FormatType::R_Integer, FormatData::SignedInt32)
        ADD(Texture::Format::R32U, FormatType::R_Integer, FormatData::UnsignedInt32)

        ADD(Texture::Format::RG8_UNorm, FormatType::RG, FormatData::Normalized8)
        ADD(Texture::Format::RG8_SNorm, FormatType::RG, FormatData::SignedNormalized8)
        ADD(Texture::Format::RG16_UNorm, FormatType::RG, FormatData::Normalized16)
        ADD(Texture::Format::RG16_SNorm, FormatType::RG, FormatData::SignedNormalized16)
        ADD(Texture::Format::RG16F, FormatType::RG, FormatData::Float16)
        ADD(Texture::Format::RG32F, FormatType::RG, FormatData::Float32)

        ADD(Texture::Format::RG8I, FormatType::RG_Integer, FormatData::SignedInt8)
        ADD(Texture::Format::RG8U, FormatType::RG_Integer, FormatData::UnsignedInt8)
        ADD(Texture::Format::RG16I, FormatType::RG_Integer, FormatData::SignedInt16)
        ADD(Texture::Format::RG16U, FormatType::RG_Integer, FormatData::UnsignedInt16)
        ADD(Texture::Format::RG32I, FormatType::RG_Integer, FormatData::SignedInt32)
        ADD(Texture::Format::RG32U, FormatType::RG_Integer, FormatData::UnsignedInt32)

        ADD(Texture::Format::RG3B2, FormatType::RGB, FormatData::Normalized332)
//        ADD(GL_RGB4, FormatType::RGB, FormatData::Normalized4)
//        ADD(GL_RGB5, FormatType::RGB, FormatData::Normalized5)
        ADD(Texture::Format::R5G6B5, FormatType::RGB, FormatData::Normalized565)
        ADD(Texture::Format::RGB8_UNorm, FormatType::RGB, FormatData::Normalized8)
        ADD(Texture::Format::RGB8_SNorm, FormatType::RGB, FormatData::SignedNormalized8)
//        ADD(GL_RGB10, FormatType::RGB, FormatData::Normalized10)
//        ADD(GL_RGB12, FormatType::RGB, FormatData::Normalized12)
        ADD(Texture::Format::SRGB8, FormatType::RGB, FormatData::sRGB8)
        ADD(Texture::Format::RGB9E5, FormatType::RGB, FormatData::Normalized9_E5);
        ADD(Texture::Format::RG11B10F, FormatType::RGB, FormatData::Float11_11_10)
        ADD(Texture::Format::RGB16_UNorm, FormatType::RGB, FormatData::Normalized16)
        ADD(Texture::Format::RGB16_SNorm, FormatType::RGB, FormatData::SignedNormalized16)
        ADD(Texture::Format::RGB16F, FormatType::RGB, FormatData::Float16)
        ADD(Texture::Format::RGB32F, FormatType::RGB, FormatData::Float32)

        ADD(Texture::Format::RGB8I, FormatType::RGB_Integer, FormatData::SignedInt8)
        ADD(Texture::Format::RGB8U, FormatType::RGB_Integer, FormatData::UnsignedInt8)
        ADD(Texture::Format::RGB16I, FormatType::RGB_Integer, FormatData::SignedInt16)
        ADD(Texture::Format::RGB16U, FormatType::RGB_Integer, FormatData::UnsignedInt16)
        ADD(Texture::Format::RGB32I, FormatType::RGB_Integer, FormatData::SignedInt32)
        ADD(Texture::Format::RGB32U, FormatType::RGB_Integer, FormatData::UnsignedInt32)

//        ADD(GL_RGBA2, FormatType::RGBA, FormatData::Normalized2)
        ADD(Texture::Format::RGBA4, FormatType::RGBA, FormatData::Normalized4)
        ADD(Texture::Format::RGB5A1, FormatType::RGBA, FormatData::Normalized555_1)
        ADD(Texture::Format::RGBA8_UNorm, FormatType::RGBA, FormatData::Normalized8)
        ADD(Texture::Format::RGBA8_SNorm, FormatType::RGBA, FormatData::SignedNormalized8)
//        ADD(GL_RGB10_A2, FormatType::RGBA, FormatData::Normalized10_10_10_2)
//        ADD(GL_RGBA12, FormatType::RGBA, FormatData::Normalized12)
        ADD(Texture::Format::RGBA16_UNorm, FormatType::RGBA, FormatData::Normalized16)
        ADD(Texture::Format::RGBA16_SNorm, FormatType::RGBA, FormatData::SignedNormalized16)
        ADD(Texture::Format::SRGB8_Alpha8, FormatType::RGBA, FormatData::sRGB8)
        ADD(Texture::Format::RGBA16F, FormatType::RGBA, FormatData::Float16)
        ADD(Texture::Format::RGBA32F, FormatType::RGBA, FormatData::Float32)

        ADD(Texture::Format::RGBA8I, FormatType::RGBA_Integer, FormatData::SignedInt8)
        ADD(Texture::Format::RGBA8U, FormatType::RGBA_Integer, FormatData::UnsignedInt8)
        ADD(Texture::Format::RGB10A2, FormatType::RGBA_Integer, FormatData::UnsignedInt10_10_10_2)
        ADD(Texture::Format::RGBA16I, FormatType::RGBA_Integer, FormatData::SignedInt16)
        ADD(Texture::Format::RGBA16U, FormatType::RGBA_Integer, FormatData::UnsignedInt16)
        ADD(Texture::Format::RGBA32I, FormatType::RGBA_Integer, FormatData::SignedInt32)
        ADD(Texture::Format::RGBA32U, FormatType::RGBA_Integer, FormatData::UnsignedInt32)

        ADD(Texture::Format::D16, FormatType::DepthStencil, FormatData::D16)
        ADD(Texture::Format::D24, FormatType::DepthStencil, FormatData::D24)
        ADD(Texture::Format::D32, FormatType::DepthStencil, FormatData::D32)
        ADD(Texture::Format::D32F, FormatType::DepthStencil, FormatData::D32F)
        ADD(Texture::Format::D24S8, FormatType::DepthStencil, FormatData::D24_S8)
        ADD(Texture::Format::D32FS8X24, FormatType::DepthStencil, FormatData::D32F_S8)
        ADD(Texture::Format::S8, FormatType::DepthStencil, FormatData::S8)
#undef ADD
    }
} // namespace

TextureProperties::TextureProperties(SessionProperties *sessionProperties)
    : QWidget(sessionProperties)
    , mSessionProperties(*sessionProperties)
    , mUi(new Ui::TextureProperties)
{
    mUi->setupUi(this);

    buildFormatMappings();

    connect(mUi->fileNew, &QToolButton::clicked,
        [this]() {
            mSessionProperties.saveCurrentItemFileAs(FileDialog::TextureExtensions);
        });
    connect(mUi->fileBrowse, &QToolButton::clicked,
        [this]() {
            mSessionProperties.openCurrentItemFile(FileDialog::TextureExtensions);
            updateWidgets();
            applyFileFormat();
        });

    connect(mUi->file, &ReferenceComboBox::textRequired,
        [](auto data) { return FileDialog::getFileTitle(data.toString()); });
    connect(mUi->file, &ReferenceComboBox::listRequired,
        [this]() { return mSessionProperties.getFileNames(
            Item::Type::Texture, true); });
    connect(mUi->file, &ReferenceComboBox::currentDataChanged,
        this, &TextureProperties::updateWidgets);

    connect(mUi->target, &DataComboBox::currentDataChanged,
        this, &TextureProperties::updateWidgets);
    connect(mUi->formatType, &DataComboBox::currentDataChanged,
        this, &TextureProperties::updateFormatDataWidget);
    connect(mUi->formatData, &DataComboBox::currentDataChanged,
        this, &TextureProperties::updateFormat);
    connect(mUi->readonly, &QCheckBox::stateChanged,
        this, &TextureProperties::updateWidgets);

    fillComboBox<QOpenGLTexture::Target>(mUi->target, {
        { "1D Texture", QOpenGLTexture::Target1D },
        { "1D Texture Array", QOpenGLTexture::Target1DArray },
        { "2D Texture", QOpenGLTexture::Target2D },
        { "2D Texture Multisample", QOpenGLTexture::Target2DMultisample },
        { "2D Texture Array", QOpenGLTexture::Target2DArray },
        { "2D Texture Multisample Array", QOpenGLTexture::Target2DMultisampleArray },
        { "3D Texture", QOpenGLTexture::Target3D },
        { "CubeMap Texture", QOpenGLTexture::TargetCubeMap },
        { "CubeMap Texture Array", QOpenGLTexture::TargetCubeMapArray },
        { "Rectangle Texture", QOpenGLTexture::TargetRectangle },
        //{ "Buffer Texture", QOpenGLTexture::TargetBuffer },
    });

    fillComboBox<FormatType>(mUi->formatType, {
        { "R", FormatType::R },
        { "RG", FormatType::RG },
        { "RGB", FormatType::RGB },
        { "RGBA", FormatType::RGBA },
        { "R Integer", FormatType::R_Integer },
        { "RG Integer", FormatType::RG_Integer },
        { "RGB Integer", FormatType::RGB_Integer },
        { "RGBA Integer", FormatType::RGBA_Integer },
        { "Depth / Stencil", FormatType::DepthStencil },
    });

    updateWidgets();
}

TextureProperties::~TextureProperties()
{
    delete mUi;
}

TextureKind TextureProperties::currentTextureKind() const
{
    mSessionProperties.updateModel();

    if (auto texture = mSessionProperties.model().item<Texture>(
            mSessionProperties.currentModelIndex()))
        return getKind(*texture);
    return { };
}

bool TextureProperties::hasFile() const
{
    mSessionProperties.updateModel();

    if (auto texture = mSessionProperties.model().item<Texture>(
            mSessionProperties.currentModelIndex()))
        return !texture->fileName.isEmpty();
    return false;
}

void TextureProperties::addMappings(QDataWidgetMapper &mapper)
{
    mapper.addMapping(mUi->file, SessionModel::FileName);
    mapper.addMapping(mUi->target, SessionModel::TextureTarget);
    mapper.addMapping(mUi->readonly, SessionModel::TextureReadonly);
    mapper.addMapping(this, SessionModel::TextureFormat);
    mapper.addMapping(mUi->width, SessionModel::TextureWidth);
    mapper.addMapping(mUi->height, SessionModel::TextureHeight);
    mapper.addMapping(mUi->depth, SessionModel::TextureDepth);
    mapper.addMapping(mUi->layers, SessionModel::TextureLayers);
    mapper.addMapping(mUi->samples, SessionModel::TextureSamples);
}

void TextureProperties::setFormat(QVariant value) 
{
    const auto format = static_cast<Texture::Format>(value.toInt());
    if (mFormat != format) {
        mFormat = format;
        const auto typeData = gTypeDataByFormat[format];
        mUi->formatType->setCurrentData(typeData.first);
        mUi->formatData->setCurrentData(typeData.second);
        emit formatChanged();
    }
}

void TextureProperties::updateWidgets()
{
    setFormEnabled(mUi->labelReadonly, mUi->readonly, hasFile());
    if (!mUi->readonly->isEnabled())
      mUi->readonly->setChecked(false);

    const auto readonly = mUi->readonly->isChecked();
    setFormEnabled(mUi->labelFormat, mUi->formatType, !readonly);
    setFormEnabled(mUi->labelFormat, mUi->formatData, !readonly);
    setFormEnabled(mUi->labelWidth, mUi->width, !readonly);
    setFormEnabled(mUi->labelHeight, mUi->height, !readonly);
    setFormEnabled(mUi->labelDepth, mUi->depth, !readonly);
    setFormEnabled(mUi->labelLayers, mUi->layers, !readonly);

    const auto kind = currentTextureKind();
    setFormVisibility(mUi->formLayout, mUi->labelHeight, mUi->height, kind.dimensions > 1);
    setFormVisibility(mUi->formLayout, mUi->labelDepth, mUi->depth, kind.dimensions > 2);
    setFormVisibility(mUi->formLayout, mUi->labelLayers, mUi->layers, kind.array);
    setFormVisibility(mUi->formLayout, mUi->labelSamples, mUi->samples, kind.multisample);
}

void TextureProperties::updateFormatDataWidget(QVariant formatType)
{
    auto data = gDataByType[static_cast<FormatType>(formatType.toInt())];
    auto current = mUi->formatData->currentData();

    mSuspendUpdateFormat = true;

    mUi->formatData->clear();

    auto formats = std::initializer_list<std::pair<const char*, FormatData>> {
        { "3/3/2 Bit", FormatData::Normalized332 },
        { "4 Bit", FormatData::Normalized4 },
        { "5/6/5 Bit", FormatData::Normalized565 },
        { "5/5/5/1 Bit", FormatData::Normalized555_1 },
        { "8 Bit", FormatData::Normalized8 },
        { "8 Bit sRGB", FormatData::sRGB8 },
        { "8 Bit Signed", FormatData::SignedNormalized8 },
        { "8 Bit Unsigned Int", FormatData::UnsignedInt8 },
        { "8 Bit Signed Int", FormatData::SignedInt8 },
        { "9 Bit, 5 Bit Exponent", FormatData::Normalized9_E5 },
        { "10/10/10/2 Bit Unsigned Int", FormatData::UnsignedInt10_10_10_2 },
        { "11/11/10 Bit Float", FormatData::Float11_11_10 },
        { "16 Bit", FormatData::Normalized16 },
        { "16 Bit Signed", FormatData::SignedNormalized16 },
        { "16 Bit Float", FormatData::Float16 },
        { "16 Bit Unsigned Int", FormatData::UnsignedInt16 },
        { "16 Bit Signed Int", FormatData::SignedInt16 },
        { "32 Bit Float", FormatData::Float32 },
        { "32 Bit Unsigned Int", FormatData::UnsignedInt32 },
        { "32 Bit Signed Int", FormatData::SignedInt32 },
        { "16 Bit Depth", FormatData::D16 },
        { "24 Bit Depth", FormatData::D24 },
        { "32 Bit Depth", FormatData::D32 },
        { "32 Bit Float Depth", FormatData::D32F },
        { "24 Bit Depth / 8 Bit Stencil", FormatData::D24_S8 },
        { "32 Bit Float Depth / 8 Bit Stencil", FormatData::D32F_S8 },
        { "8 Bit Stencil", FormatData::S8 },
    };
    for (auto kv : formats)
        if (std::find(data.cbegin(), data.cend(), kv.second) != data.cend())
            mUi->formatData->addItem(kv.first, kv.second);

    mUi->formatData->setCurrentIndex(-1);

    mSuspendUpdateFormat = false;

    auto index = mUi->formatData->findData(current);
    mUi->formatData->setCurrentIndex(index == -1 ? 0 : index);
}

void TextureProperties::updateFormat(QVariant formatData)
{
    if (!mSuspendUpdateFormat) {
        auto type = static_cast<FormatType>(
            mUi->formatType->currentData().toInt());
        auto data = static_cast<FormatData>(formatData.toInt());
        setFormat(gFormatByTypeData[std::make_pair(type, data)]);
    }
}

void TextureProperties::applyFileFormat()
{
    auto fileName = mUi->file->currentData().toString();
    auto texture = TextureData();
    if (Singletons::fileCache().getTexture(fileName, &texture)) {
        mUi->readonly->setChecked(true);
        setFormat(texture.format());
        mUi->width->setValue(texture.width());
        mUi->height->setValue(texture.height());
        mUi->depth->setValue(texture.depth());
    }
}
