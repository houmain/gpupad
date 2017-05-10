#include "TextureProperties.h"
#include "SessionProperties.h"
#include "ui_TextureProperties.h"
#include "Singletons.h"
#include "editors/EditorManager.h"

namespace {
    enum FormatType
    {
        R,
        RG,
        RGB,
        RGBA,
        sRGB,
        Packed,
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

        // Packed formats
        RGB9E5,
        RG11B10F,
        RG3B2,
        R5G6B5,
        RGB5A1,
        RGBA4,
        RGB10A2,

        // Depth formats
        D16,
        D24,
        D24S8,
        D32,
        D32F,
        D32FS8X24,
        S8,

        // sRGB formats
        SRGB8,
        SRGB8_Alpha8,
    };

    std::map<QOpenGLTexture::TextureFormat,
        std::pair<FormatType, FormatData>> gTypeDataByFormat;
    std::map<std::pair<FormatType, FormatData>,
        QOpenGLTexture::TextureFormat> gFormatByTypeData;
    std::map<FormatType, std::vector<FormatData>> gDataByType;

    void buildFormatMappings()
    {
#define ADD(FORMAT, TYPE, DATA) \
            gTypeDataByFormat[FORMAT] = std::make_pair(TYPE, DATA); \
            gFormatByTypeData[std::make_pair(TYPE, DATA)] = FORMAT; \
            gDataByType[TYPE].push_back(DATA);

        ADD(QOpenGLTexture::R8_UNorm, FormatType::R, FormatData::Normalized8)
        ADD(QOpenGLTexture::RG8_UNorm, FormatType::RG, FormatData::Normalized8)
        ADD(QOpenGLTexture::RGB8_UNorm, FormatType::RGB, FormatData::Normalized8)
        ADD(QOpenGLTexture::RGBA8_UNorm, FormatType::RGBA, FormatData::Normalized8)
        ADD(QOpenGLTexture::R16_UNorm, FormatType::R, FormatData::Normalized16)
        ADD(QOpenGLTexture::RG16_UNorm, FormatType::RG, FormatData::Normalized16)
        ADD(QOpenGLTexture::RGB16_UNorm, FormatType::RGB, FormatData::Normalized16)
        ADD(QOpenGLTexture::RGBA16_UNorm, FormatType::RGBA, FormatData::Normalized16)
        ADD(QOpenGLTexture::R8_SNorm, FormatType::R, FormatData::SignedNormalized8)
        ADD(QOpenGLTexture::RG8_SNorm, FormatType::RG, FormatData::SignedNormalized8)
        ADD(QOpenGLTexture::RGB8_SNorm, FormatType::RGB, FormatData::SignedNormalized8)
        ADD(QOpenGLTexture::RGBA8_SNorm, FormatType::RGBA, FormatData::SignedNormalized8)
        ADD(QOpenGLTexture::R16_SNorm, FormatType::R, FormatData::SignedNormalized16)
        ADD(QOpenGLTexture::RG16_SNorm, FormatType::RG, FormatData::SignedNormalized16)
        ADD(QOpenGLTexture::RGB16_SNorm, FormatType::RGB, FormatData::SignedNormalized16)
        ADD(QOpenGLTexture::RGBA16_SNorm, FormatType::RGBA, FormatData::SignedNormalized16)
        ADD(QOpenGLTexture::R8U, FormatType::R, FormatData::UnsignedInt8)
        ADD(QOpenGLTexture::RG8U, FormatType::RG, FormatData::UnsignedInt8)
        ADD(QOpenGLTexture::RGB8U, FormatType::RGB, FormatData::UnsignedInt8)
        ADD(QOpenGLTexture::RGBA8U, FormatType::RGBA, FormatData::UnsignedInt8)
        ADD(QOpenGLTexture::R16U, FormatType::R, FormatData::UnsignedInt16)
        ADD(QOpenGLTexture::RG16U, FormatType::RG, FormatData::UnsignedInt16)
        ADD(QOpenGLTexture::RGB16U, FormatType::RGB, FormatData::UnsignedInt16)
        ADD(QOpenGLTexture::RGBA16U, FormatType::RGBA, FormatData::UnsignedInt16)
        ADD(QOpenGLTexture::R32U, FormatType::R, FormatData::UnsignedInt32)
        ADD(QOpenGLTexture::RG32U, FormatType::RG, FormatData::UnsignedInt32)
        ADD(QOpenGLTexture::RGB32U, FormatType::RGB, FormatData::UnsignedInt32)
        ADD(QOpenGLTexture::RGBA32U, FormatType::RGBA, FormatData::UnsignedInt32)
        ADD(QOpenGLTexture::R8I, FormatType::R, FormatData::SignedInt8)
        ADD(QOpenGLTexture::RG8I, FormatType::RG, FormatData::SignedInt8)
        ADD(QOpenGLTexture::RGB8I, FormatType::RGB, FormatData::SignedInt8)
        ADD(QOpenGLTexture::RGBA8I, FormatType::RGBA, FormatData::SignedInt8)
        ADD(QOpenGLTexture::R16I, FormatType::R, FormatData::SignedInt16)
        ADD(QOpenGLTexture::RG16I, FormatType::RG, FormatData::SignedInt16)
        ADD(QOpenGLTexture::RGB16I, FormatType::RGB, FormatData::SignedInt16)
        ADD(QOpenGLTexture::RGBA16I, FormatType::RGBA, FormatData::SignedInt16)
        ADD(QOpenGLTexture::R32I, FormatType::R, FormatData::SignedInt32)
        ADD(QOpenGLTexture::RG32I, FormatType::RG, FormatData::SignedInt32)
        ADD(QOpenGLTexture::RGB32I, FormatType::RGB, FormatData::SignedInt32)
        ADD(QOpenGLTexture::RGBA32I, FormatType::RGBA, FormatData::SignedInt32)
        ADD(QOpenGLTexture::R16F, FormatType::R, FormatData::Float16)
        ADD(QOpenGLTexture::RG16F, FormatType::RG, FormatData::Float16)
        ADD(QOpenGLTexture::RGB16F, FormatType::RGB, FormatData::Float16)
        ADD(QOpenGLTexture::RGBA16F, FormatType::RGBA, FormatData::Float16)
        ADD(QOpenGLTexture::R32F, FormatType::R, FormatData::Float32)
        ADD(QOpenGLTexture::RG32F, FormatType::RG, FormatData::Float32)
        ADD(QOpenGLTexture::RGB32F, FormatType::RGB, FormatData::Float32)
        ADD(QOpenGLTexture::RGBA32F, FormatType::RGBA, FormatData::Float32)

        ADD(QOpenGLTexture::RGB9E5, FormatType::Packed, FormatData::RGB9E5)
        ADD(QOpenGLTexture::RG11B10F, FormatType::Packed, FormatData::RG11B10F)
        ADD(QOpenGLTexture::RG3B2, FormatType::Packed, FormatData::RG3B2)
        ADD(QOpenGLTexture::R5G6B5, FormatType::Packed, FormatData::R5G6B5)
        ADD(QOpenGLTexture::RGB5A1, FormatType::Packed, FormatData::RGB5A1)
        ADD(QOpenGLTexture::RGBA4, FormatType::Packed, FormatData::RGBA4)
        ADD(QOpenGLTexture::RGB10A2, FormatType::Packed, FormatData::RGB10A2)

        ADD(QOpenGLTexture::D16, FormatType::DepthStencil, FormatData::D16)
        ADD(QOpenGLTexture::D24, FormatType::DepthStencil, FormatData::D24)
        ADD(QOpenGLTexture::D24S8, FormatType::DepthStencil, FormatData::D24S8)
        ADD(QOpenGLTexture::D32, FormatType::DepthStencil, FormatData::D32)
        ADD(QOpenGLTexture::D32F, FormatType::DepthStencil, FormatData::D32F)
        ADD(QOpenGLTexture::D32FS8X24, FormatType::DepthStencil, FormatData::D32FS8X24)
        ADD(QOpenGLTexture::S8, FormatType::DepthStencil, FormatData::S8)

        ADD(QOpenGLTexture::SRGB8, FormatType::sRGB, FormatData::SRGB8)
        ADD(QOpenGLTexture::SRGB8_Alpha8, FormatType::sRGB, FormatData::SRGB8_Alpha8)
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
        [this]() { mSessionProperties.setCurrentItemFile(
            Singletons::editorManager().openNewImageEditor()); });
    connect(mUi->fileBrowse, &QToolButton::clicked,
        [this]() { mSessionProperties.selectCurrentItemFile(
            FileDialog::ImageExtensions); });

    connect(mUi->file, &ReferenceComboBox::textRequired,
        [](auto data) { return FileDialog::getFileTitle(data.toString()); });
    connect(mUi->file, &ReferenceComboBox::listRequired,
        [this]() { return mSessionProperties.getFileNames(
            ItemType::Texture, true); });

    connect(mUi->target, &DataComboBox::currentDataChanged,
        this, &TextureProperties::updateWidgets);
    connect(mUi->formatType, &DataComboBox::currentDataChanged,
        this, &TextureProperties::updateFormatDataWidget);
    connect(mUi->formatData, &DataComboBox::currentDataChanged,
        this, &TextureProperties::updateFormat);

    fill<QOpenGLTexture::Target>(mUi->target, {
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

    fill<FormatType>(mUi->formatType, {
        { "R", FormatType::R },
        { "RG", FormatType::RG },
        { "RGB", FormatType::RGB },
        { "RGBA", FormatType::RGBA },
        { "sRGB", FormatType::sRGB },
        { "Packed", FormatType::Packed },
        { "Depth/Stencil", FormatType::DepthStencil },
    });

    updateWidgets();
}

TextureProperties::~TextureProperties()
{
    delete mUi;
}

QWidget *TextureProperties::fileWidget() const { return mUi->file; }
QWidget *TextureProperties::targetWidget() const { return mUi->target; }
QWidget *TextureProperties::widthWidget() const { return mUi->width; }
QWidget *TextureProperties::heightWidget() const { return mUi->height; }
QWidget *TextureProperties::depthWidget() const { return mUi->depth; }

void TextureProperties::setFormat(QVariant value) {
    auto format = static_cast<Texture::Format>(value.toInt());
    if (mFormat != format) {
        mFormat = format;
        auto typeData = gTypeDataByFormat[format];
        mUi->formatType->setCurrentData(typeData.first);
        mUi->formatData->setCurrentData(typeData.second);
        emit formatChanged();
    }
}

void TextureProperties::updateWidgets()
{
    auto dimensions = 0;
    auto target = static_cast<QOpenGLTexture::Target>(
        mUi->target->currentData().toInt());

    switch (target) {
        case QOpenGLTexture::Target1D:
        case QOpenGLTexture::TargetBuffer:
            dimensions = 1;
            break;

        case QOpenGLTexture::Target1DArray:
        case QOpenGLTexture::Target2D:
        case QOpenGLTexture::TargetCubeMap:
        case QOpenGLTexture::Target2DMultisample:
        case QOpenGLTexture::TargetRectangle:
            dimensions = 2;
            break;

        case QOpenGLTexture::Target2DArray:
        case QOpenGLTexture::Target2DMultisampleArray:
        case QOpenGLTexture::Target3D:
        case QOpenGLTexture::TargetCubeMapArray:
            dimensions = 3;
            break;
    }
    setFormVisibility(mUi->formLayout, mUi->labelHeight, mUi->height, dimensions > 1);
    setFormVisibility(mUi->formLayout, mUi->labelDepth, mUi->depth, dimensions > 2);
}

void TextureProperties::updateFormatDataWidget(QVariant formatType)
{
    auto data = gDataByType[static_cast<FormatType>(formatType.toInt())];
    auto index = mUi->formatData->currentIndex();

    mSuspendUpdateFormat = true;

    mUi->formatData->clear();
    for (auto kv : std::initializer_list<std::pair<const char*, FormatData>> {
        { "8 Bit Normalized", FormatData::Normalized8 },
        { "16 Bit Normalized", FormatData::Normalized16 },
        { "8 Bit Signed Normalized", FormatData::SignedNormalized8 },
        { "16 Bit Signed Normalized", FormatData::SignedNormalized16 },
        { "8 Bit Unsigned Int", FormatData::UnsignedInt8 },
        { "8 Bit Signed Int", FormatData::SignedInt8 },
        { "16 Bit Unsigned Int", FormatData::UnsignedInt16 },
        { "16 Bit Signed Int", FormatData::SignedInt16 },
        { "32 Bit Unsigned Int", FormatData::UnsignedInt32 },
        { "32 Bit Signed Int", FormatData::SignedInt32 },
        { "16 Bit Float", FormatData::Float16 },
        { "32 Bit Float", FormatData::Float32 },

        {"RGB9E5", FormatData::RGB9E5 },
        {"RG11B10F", FormatData::RG11B10F },
        {"RG3B2", FormatData::RG3B2 },
        {"R5G6B5", FormatData::R5G6B5 },
        {"RGB5A1", FormatData::RGB5A1 },
        {"RGBA4", FormatData::RGBA4 },
        {"RGB10A2", FormatData::RGB10A2 },

        {"D16", FormatData::D16 },
        {"D24", FormatData::D24 },
        {"D24S8", FormatData::D24S8 },
        {"D32", FormatData::D32 },
        {"D32F", FormatData::D32F },
        {"D32FS8X24", FormatData::D32FS8X24 },
        {"S8", FormatData::S8 },

        {"SRGB8",FormatData::SRGB8 },
        {"SRGB8_Alpha8",FormatData::SRGB8_Alpha8 },
    }) {
        if (std::find(data.cbegin(), data.cend(), kv.second) != data.cend())
            mUi->formatData->addItem(kv.first, kv.second);
    }

    mUi->formatData->setCurrentIndex(-1);

    mSuspendUpdateFormat = false;

    if (index < mUi->formatData->count())
        mUi->formatData->setCurrentIndex(index);
}

void TextureProperties::updateFormat(QVariant formatData)
{
    if (!mSuspendUpdateFormat) {
        auto type = static_cast<FormatType>(mUi->formatType->currentData().toInt());
        auto data = static_cast<FormatData>(formatData.toInt());
        setFormat(gFormatByTypeData[std::make_pair(type, data)]);
    }
}
