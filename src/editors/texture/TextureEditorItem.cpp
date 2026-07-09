#include "TextureEditorItem.h"
#include "render/RenderWidget.h"
#include <QMatrix4x4>
#include <QTransform>
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
  vec2 pickerFragCoord;
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

  mat4 transform = pc.transform;
#if !defined(VULKAN)
  transform[1][1] *= -1;
  transform[3][1] *= -1;
#endif
  vTexCoord = ((transform * vec4(pos, 0.0, 1.0)).xy + 1.0) / 2.0;

  if (pc.flipVertically == 0)
    vTexCoord.y = 1 - vTexCoord.y;

  gl_Position = vec4(pos, 0.0, 1.0);
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
  vec2 pickerFragCoord;
  float mappingOffset;
  float mappingFactor;
  uint colorMask;
} pc;

layout(binding = 0) uniform SAMPLER uTexture;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 oColor;
writeonly layout(binding = 1, rgba32f) uniform image1D uPickerColor;

#else // !defined(VULKAN)

#ifdef GL_ARB_texture_cube_map_array
#extension GL_ARB_texture_cube_map_array: enable
#endif

#ifdef GL_ARB_shader_image_load_store
#extension GL_ARB_shader_image_load_store: enable
#extension GL_ARB_shader_image_size: enable
writeonly layout(rgba32f) uniform image1D uPickerColor;
#endif

uniform SAMPLER uTexture;
uniform vec2 uSize;
uniform float uLevel;
uniform float uLayer;
uniform int uFace;
uniform int uSample;
uniform int uSamples;
uniform vec2 uPickerFragCoord;
uniform float uMappingOffset;
uniform float uMappingFactor;
uniform int uColorMask;

in vec2 vTexCoord;
out vec4 oColor;

struct Params {
  vec2 size;
  float level;
  float layer;
  int face;
  int samp;
  int samples;
  vec2 pickerFragCoord;
  float mappingOffset;
  float mappingFactor;
  int colorMask;
};
Params pc = Params(
  uSize,
  uLevel,
  uLayer,
  uFace,
  uSample,
  uSamples,
  uPickerFragCoord,
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

  if ((pc.colorMask & 1) != 0) color.r = 0.0;
  if ((pc.colorMask & 2) != 0) color.g = 0.0;
  if ((pc.colorMask & 4) != 0) color.b = 0.0;
  if ((pc.colorMask & 8) != 0) color.a = 1.0;

#if LINEAR_TO_SRGB
  color = vec4(linearToSrgb(color.rgb), color.a);
#endif

#if PICKER_ENABLED && defined(GL_ARB_shader_image_load_store)
  if (gl_FragCoord.xy == pc.pickerFragCoord)
    imageStore(uPickerColor, 0, color);
#endif

  color.rgb = (color.rgb + vec3(pc.mappingOffset)) * pc.mappingFactor;

  oColor = color;
}
)";

TextureEditorItem::TextureEditorItem(QWindow *window) : QObject(window) { }

TextureEditorItem::~TextureEditorItem() = default;

void TextureEditorItem::setImage(TextureData image)
{
    const auto w = image.width();
    const auto h = image.height();
    mBoundingRect = { -w / 2, -h / 2, w, h };
    mImage = std::move(image);
    if (!mImage.isNull())
        mUpload = true;
    mTextureSamples = 1;
    update();
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
    static auto sTargetVersions = std::map<Texture::Target, TargetVersion>{
        { Texture::Target::Target1D,
            { "sampler1D", "textureLod(S, TC.x, pc.level)" } },
        { Texture::Target::Target1DArray,
            { "sampler1DArray",
                "textureLod(S, vec2(TC.x, pc.layer), pc.level)" } },
        { Texture::Target::Target2D,
            { "sampler2D", "textureLod(S, TC, pc.level)" } },
        { Texture::Target::Target2DArray,
            { "sampler2DArray",
                "textureLod(S, vec3(TC, pc.layer), pc.level)" } },
        { Texture::Target::Target3D,
            { "sampler3D", "textureLod(S, vec3(TC, pc.layer), pc.level)" } },
        { Texture::Target::TargetCubeMap,
            { "samplerCube",
                "textureLod(S, getCubeTexCoord(TC, pc.face), pc.level)" } },
        { Texture::Target::TargetCubeMapArray,
            { "samplerCubeArray",
                "textureLod(S, vec4(getCubeTexCoord(TC, pc.face), "
                "pc.layer), pc.level)" } },
        { Texture::Target::Target2DMultisample,
            { "sampler2DMS", "texelFetch(S, ivec2(TC * (pc.size - 1)), s)" } },
        { Texture::Target::Target2DMultisampleArray,
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
        + fragmentShaderSource;
}

bool TextureEditorItem::canLinearFilter(Texture::Format format)
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

QWindow &TextureEditorItem::window()
{
    return *static_cast<QWindow *>(parent());
}

void TextureEditorItem::update()
{
    window().requestUpdate();
}

QMatrix4x4 TextureEditorItem::getTransform(const QSizeF &bounds,
    const QPointF &offset)
{
    const auto dpr = window().devicePixelRatio();
    const auto renderSize = QSizeF(window().size());
    const auto width = std::max(renderSize.width() * dpr, 1.0);
    const auto height = std::max(renderSize.height() * dpr, 1.0);
    const auto sx = width / bounds.width();
    const auto sy = height / bounds.height();
    const auto x = (width - offset.x() - bounds.width()) / bounds.width();
    const auto y = (height - offset.y() - bounds.height()) / bounds.height();
    return QMatrix4x4(QTransform(sx, 0, 0, 0, sy, 0, x, y, 1));
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
    params.pickerFragCoord = {
        static_cast<float>(mMousePosition.x() + 0.5f),
        static_cast<float>(mMousePosition.y() + 0.5f),
    };
    params.mappingOffset = static_cast<float>(-mMappingRange.minimum);
    params.mappingFactor = static_cast<float>(1 / mMappingRange.range());
    params.colorMask = mColorMask;
    return params;
}

void TextureEditorItem::setColorMask(unsigned int colorMask)
{
    if (mColorMask != colorMask) {
        mColorMask = colorMask;
        update();
    }
}
