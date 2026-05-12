#include "TextureEditorItem.h"
#include "Singletons.h"
#include "render/ComputeRange.h"
#include "render/RenderWindow.h"

#include <QMatrix4x4>
#include <algorithm>
#include <array>
#include <map>

namespace {
    struct TargetVersion
    {
        QString sampler;
        QString sample;
    };

    struct SampleTypeVersion
    {
        QString prefix;
        QString mapping;
    };
} // namespace

QString TextureEditorItem::vertexShaderSource = R"(
#version 450
#if defined(VULKAN)

layout(push_constant) uniform Params {
  mat4 transform;
  vec2 size;
  float level;
  float layer;
  int face;
  int samp;
  int samples;
  int flipVertically;
  float mappingOffset;
  float mappingFactor;
  uint colorMask;
} pc;

layout(location = 0) out vec2 vTexCoord;

#else // !defined(VULKAN)

uniform mat4 uTransform;
uniform int uFlipVertically;
out vec2 vTexCoord;

struct Params {
  mat4 transform;
  int flipVertically;
};
Params pc = Params(uTransform, uFlipVertically);

#endif // !defined(VULKAN)

const vec2 data[4] = vec2[] (
  vec2(-1.0,  1.0),
  vec2(-1.0, -1.0),
  vec2( 1.0,  1.0),
  vec2( 1.0, -1.0)
);

void main() {
#if defined(VULKAN)
  vec2 pos = data[gl_VertexIndex];
#else
  vec2 pos = data[gl_VertexID];
#endif
  vTexCoord = (pos + 1.0) / 2.0;
  if (pc.flipVertically != 0)
    pos.y *= -1;
  gl_Position = pc.transform * vec4(pos, 0.0, 1.0);
}
)";

const QString fragmentShaderSource = R"(
#if defined(VULKAN)

layout(push_constant) uniform Params {
  mat4 transform;
  vec2 size;
  float level;
  float layer;
  int face;
  int samp;
  int samples;
  int flipVertically;
  float mappingOffset;
  float mappingFactor;
  uint colorMask;
} pc;

layout(set = 0, binding = 0) uniform SAMPLER uTexture;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 oColor;

#else // !defined(VULKAN)

#ifdef GL_ARB_texture_cube_map_array
#extension GL_ARB_texture_cube_map_array: enable
#endif

#ifdef GL_ARB_shader_image_load_store
#extension GL_ARB_shader_image_load_store: enable
#extension GL_ARB_shader_image_size: enable
writeonly layout(rgba32f) uniform image1D uPickerColor;
uniform vec2 uPickerFragCoord;
layout(r32ui) uniform uimage1D uHistogram;
uniform float uHistogramOffset;
uniform float uHistogramFactor;
#endif

uniform SAMPLER uTexture;
uniform vec2 uSize;
uniform float uLevel;
uniform float uLayer;
uniform int uFace;
uniform int uSample;
uniform int uSamples;
uniform float uMappingOffset;
uniform float uMappingFactor;
uniform uint uColorMask;

in vec2 vTexCoord;
out vec4 oColor;

struct Params {
  vec2 size;
  float level;
  float layer;
  int face;
  int samp;
  int samples;
  float mappingOffset;
  float mappingFactor;
  uint colorMask;
};
Params pc = Params(
  uSize,
  uLevel,
  uLayer,
  uFace,
  uSample,
  uSamples,
  uMappingOffset,
  uMappingFactor,
  uColorMask);

#endif // !defined(VULKAN)

float linearToSrgb(float value) {
  if (value > 0.0031308)
    return 1.055 * pow(value, 1.0 / 2.4) - 0.055;
  return 12.92 * value;
}
vec3 linearToSrgb(vec3 value) {
  return vec3(
    linearToSrgb(value.r),
    linearToSrgb(value.g),
    linearToSrgb(value.b)
  );
}
vec3 getCubeTexCoord(vec2 tc, int face) {
  float rx = 0.5 - tc.x;
  float ry = 0.5 - tc.y;
  float rz = 0.5;
  return vec3[](
    vec3(-rz, -ry, rx),
    vec3( rz, -ry, rx),
    vec3( rx,  rz, ry),
    vec3( rx, -rz, ry),
    vec3( rx, -ry, rz),
    vec3(-rx, -ry, rz)
  )[face];
}

void main() {
  vec4 color = vec4(0);
  for (int s = pc.samp; s < pc.samp + pc.samples; ++s)
    color += vec4(SAMPLE);
  color /= float(pc.samples);
  color = MAPPING;
  color = SWIZZLE;

  if ((pc.colorMask & 1u) != 0u) color.r = 0.0;
  if ((pc.colorMask & 2u) != 0u) color.g = 0.0;
  if ((pc.colorMask & 4u) != 0u) color.b = 0.0;
  if ((pc.colorMask & 8u) != 0u) color.a = 1.0;

#if LINEAR_TO_SRGB
  color = vec4(linearToSrgb(color.rgb), color.a);
#endif

#if !defined(VULKAN)
#if PICKER_ENABLED && defined(GL_ARB_shader_image_load_store)
  if (gl_FragCoord.xy == uPickerFragCoord)
    imageStore(uPickerColor, 0, color);
#endif

#if HISTOGRAM_ENABLED && defined(GL_ARB_shader_image_load_store)
  if (clamp(TC, vec2(0), vec2(1)) == TC) {
    ivec3 offset = ivec3((color.rgb + vec3(uHistogramOffset)) * uHistogramFactor + vec3(0.5));
    offset = clamp(offset, ivec3(0), ivec3(imageSize(uHistogram) / 3 - 1)) * 3 + ivec3(0, 1, 2);
    imageAtomicAdd(uHistogram, offset.r, 1u);
    imageAtomicAdd(uHistogram, offset.g, 1u);
    imageAtomicAdd(uHistogram, offset.b, 1u);

    color.rgb = (color.rgb + vec3(pc.mappingOffset)) * pc.mappingFactor;
  }
#endif
#endif // !defined(VULKAN)

  oColor = color;
}
)";

TextureEditorItem::TextureEditorItem(RenderWindow *widget) : QObject(widget)
{
    setHistogramBinCount(1);
}

TextureEditorItem::~TextureEditorItem() = default;

void TextureEditorItem::setImage(TextureData image)
{
    Q_ASSERT(!image.isNull());
    const auto w = image.width();
    const auto h = image.height();
    mBoundingRect = { -w / 2, -h / 2, w, h };
    mImage = std::move(image);
    mUpload = true;
    mTextureSamples = 1;
    imageChanged();
    render();
}

bool TextureEditorItem::downloadImage(TextureData *image)
{
    Q_ASSERT(image);
    if (mImage.isNull())
        return false;

    *image = mImage;
    return true;
}

bool TextureEditorItem::canFilter() const
{
    return (!mImage.isNull() && canLinearFilter(mImage.format())
        && mTextureSamples == 1);
}

QString TextureEditorItem::buildFragmentShader(const ShaderDesc &desc)
{
    static auto sTargetVersions =
        std::map<QOpenGLTexture::Target, TargetVersion>{
            { QOpenGLTexture::Target1D,
                { "sampler1D", "textureLod(S, TC.x, pc.level)" } },
            { QOpenGLTexture::Target1DArray,
                { "sampler1DArray",
                    "textureLod(S, vec2(TC.x, pc.layer), pc.level)" } },
            { QOpenGLTexture::Target2D,
                { "sampler2D", "textureLod(S, TC, pc.level)" } },
            { QOpenGLTexture::Target2DArray,
                { "sampler2DArray",
                    "textureLod(S, vec3(TC, pc.layer), pc.level)" } },
            { QOpenGLTexture::Target3D,
                { "sampler3D",
                    "textureLod(S, vec3(TC, pc.layer), pc.level)" } },
            { QOpenGLTexture::TargetCubeMap,
                { "samplerCube",
                    "textureLod(S, getCubeTexCoord(TC, pc.face), pc.level)" } },
            { QOpenGLTexture::TargetCubeMapArray,
                { "samplerCubeArray",
                    "textureLod(S, vec4(getCubeTexCoord(TC, pc.face), "
                    "pc.layer), pc.level)" } },
            { QOpenGLTexture::Target2DMultisample,
                { "sampler2DMS",
                    "texelFetch(S, ivec2(TC * (pc.size - 1)), s)" } },
            { QOpenGLTexture::Target2DMultisampleArray,
                { "sampler2DMSArray",
                    "texelFetch(S, ivec3(TC * (pc.size - 1), pc.layer), s)" } },
        };
    static auto sSampleTypeVersions =
        std::map<TextureSampleType, SampleTypeVersion>{
            { TextureSampleType::Normalized, { "", "color" } },
            { TextureSampleType::Normalized_sRGB, { "", "color" } },
            { TextureSampleType::Float, { "", "color" } },
            { TextureSampleType::Uint8, { "u", "color / 255.0" } },
            { TextureSampleType::Uint16, { "u", "color / 65535.0" } },
            { TextureSampleType::Uint32, { "u", "color / 4294967295.0" } },
            { TextureSampleType::Uint_10_10_10_2,
                { "u", "color / vec4(vec3(1023.0), 3.0)" } },
            { TextureSampleType::Int8, { "i", "color / 127.0" } },
            { TextureSampleType::Int16, { "i", "color / 32767.0" } },
            { TextureSampleType::Int32, { "i", "color / 2147483647.0" } },
        };
    static auto sComponetSwizzle = std::map<int, QString>{
        { 1, "vec4(vec3(color.r), 1)" },
        { 2, "vec4(vec3(color.rg, 0), 1)" },
        { 3, "vec4(color.rgb, 1)" },
        { 4, "color" },
    };
    const auto sampleType = getTextureSampleType(desc.format);
    const auto targetVersion = sTargetVersions[desc.target];
    const auto sampleTypeVersion = sSampleTypeVersions[sampleType];
    const auto swizzle =
        sComponetSwizzle[getTextureComponentCount(desc.format)];
    const auto linearToSrgb = (sampleType == TextureSampleType::Normalized_sRGB
        || sampleType == TextureSampleType::Float);
    return "#version 450\n"
           "#define S uTexture\n"
           "#define TC vTexCoord\n"
           "#define SAMPLER "
        + sampleTypeVersion.prefix + targetVersion.sampler
        + "\n"
          "#define SAMPLE "
        + targetVersion.sample + "\n" + "#define MAPPING "
        + sampleTypeVersion.mapping + "\n" + "#define SWIZZLE " + swizzle + "\n"
        + "#define LINEAR_TO_SRGB " + QString::number(linearToSrgb) + "\n"
        + "#define PICKER_ENABLED " + QString::number(desc.picker) + "\n"
        + "#define HISTOGRAM_ENABLED " + QString::number(desc.histogram) + "\n"
        + fragmentShaderSource;
}

bool TextureEditorItem::canLinearFilter(QOpenGLTexture::TextureFormat format)
{
    switch (getTextureSampleType(format)) {
    case TextureSampleType::Int8:
    case TextureSampleType::Int16:
    case TextureSampleType::Int32:
    case TextureSampleType::Uint8:
    case TextureSampleType::Uint16:
    case TextureSampleType::Uint32:
    case TextureSampleType::Uint_10_10_10_2: return false;
    default:                                 return true;
    }
}

void TextureEditorItem::setMousePosition(const QPointF &mousePosition)
{
    mMousePosition = mousePosition;
    if (mPickerEnabled)
        update();
}

void TextureEditorItem::setMappingRange(const Range &range)
{
    if (mMappingRange != range) {
        mMappingRange = range;
        update();
    }
}

void TextureEditorItem::setHistogramBinCount(int count)
{
    count = qMax(count / 3, 1) * 3;

    if (mHistogramBins.size() != count) {
        mHistogramBins.resize(count);
        if (mHistogramEnabled)
            update();
    }
}

void TextureEditorItem::setHistogramBounds(const Range &bounds)
{
    if (mHistogramBounds != bounds) {
        mHistogramBounds = bounds;
        if (mHistogramEnabled)
            update();
    }
}

void TextureEditorItem::computeHistogramBounds()
{
    if (!mComputeRange) {
        mComputeRange = new ComputeRange(Singletons::glRenderer(), this);
        connect(mComputeRange, &ComputeRange::rangeComputed, this,
            &TextureEditorItem::histogramBoundsComputed);
    }

    //mComputeRange->setImage(mImage.getTarget(mTextureSamples),
    //    (mPreviewTextureId ? mPreviewTextureId : mImageTextureId), mImage,
    //    static_cast<int>(mLevel), static_cast<int>(mLayer), mFace);

    mComputeRange->update();
}

RenderWindow &TextureEditorItem::window()
{
    return *qobject_cast<RenderWindow *>(parent());
}

void TextureEditorItem::render()
{
    window().update();
}

void TextureEditorItem::update()
{
    window().requestUpdate();
}

auto TextureEditorItem::getParams(const QMatrix4x4 &transform,
    int textureSamples) const -> Params
{
    auto params = Params{};
    std::copy_n(transform.constData(), 16, params.transform.begin());
    params.width = static_cast<float>(mBoundingRect.width());
    params.height = static_cast<float>(mBoundingRect.height());
    params.level = mLevel;
    params.layer = mLayer / static_cast<float>(std::max(mImage.depth(), 1));
    params.face = mFace;
    const auto resolve = (mSample < 0);
    params.sample = std::max(0, resolve ? 0 : mSample);
    params.samples = std::max(1, resolve ? textureSamples : 1);
    params.flipVertically = (mFlipVertically ? 1 : 0);
    params.mappingOffset = static_cast<float>(-mMappingRange.minimum);
    params.mappingFactor = static_cast<float>(1 / mMappingRange.range());
    params.colorMask = mColorMask;
    return params;
}

void TextureEditorItem::imageChanged() { }

void TextureEditorItem::updateHistogram()
{
    if (mPrevHistogramBins.size() != mHistogramBins.size()) {
        // cannot compute delta since prev is still undefined - trigger another update
        mPrevHistogramBins = mHistogramBins;
        return update();
    }

    auto maxValue = std::array<quint32, 3>{ 1, 1, 1 };
    for (auto i = 0, c = 0; i < mHistogramBins.size(); ++i, c = (c + 1) % 3) {
        const auto value = (mHistogramBins[i] - mPrevHistogramBins[i]);
        maxValue[c] = qMax(maxValue[c], value);
    }

    auto histogram = QVector<qreal>(mHistogramBins.size());
    const auto scale = std::array<qreal, 3>{ 1.0 / maxValue[0],
        1.0 / maxValue[1], 1.0 / maxValue[2] };
    for (auto i = 0, c = 0; i < mHistogramBins.size(); ++i, c = (c + 1) % 3) {
        const auto value = (mHistogramBins[i] - mPrevHistogramBins[i]);
        histogram[i] = (1.0 - (value * scale[c]));
    }
    mPrevHistogramBins.swap(mHistogramBins);

    Q_EMIT histogramChanged(histogram);
}

void TextureEditorItem::setColorMask(unsigned int colorMask)
{
    if (mColorMask != colorMask) {
        mColorMask = colorMask;
        update();
    }
}
