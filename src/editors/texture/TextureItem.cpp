#include "TextureItem.h"
#include "GLWidget.h"
#include "Singletons.h"
#include "render/GLShareSynchronizer.h"
#include "render/ComputeRange.h"
#include <optional>
#include <cmath>
#include <array>

namespace 
{
    const auto vertexShaderSource = R"(
#version 330

uniform mat4 uTransform;
uniform bool uFlipVertically;
out vec2 vTexCoord;

const vec2 data[4]= vec2[] (
  vec2(-1.0,  1.0),
  vec2(-1.0, -1.0),
  vec2( 1.0,  1.0),
  vec2( 1.0, -1.0)
);

void main() {
  vec2 pos = data[gl_VertexID];
  vTexCoord = (pos + 1.0) / 2.0;
  if (!uFlipVertically)
    pos.y *= -1;
  gl_Position = uTransform * vec4(pos, 0.0, 1.0);
}
)";

    const auto fragmentShaderSource = R"(

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
uniform int uColorMask;

in vec2 vTexCoord;
out vec4 oColor;

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
  for (int s = uSample; s < uSample + uSamples; ++s)
    color += vec4(SAMPLE);
  color /= float(uSamples);
  color = MAPPING;
  color = SWIZZLE;

  if ((uColorMask & 1) != 0) color.r = 0.0;
  if ((uColorMask & 2) != 0) color.g = 0.0;
  if ((uColorMask & 4) != 0) color.b = 0.0;
  if ((uColorMask & 8) != 0) color.a = 1.0;

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

    color.rgb = (color.rgb + vec3(uMappingOffset)) * uMappingFactor;
  }
#endif

#if LINEAR_TO_SRGB
  color = vec4(linearToSrgb(color.rgb), color.a);
#endif

  oColor = color;
}
)";

    struct ProgramDesc 
    {
        QOpenGLTexture::Target target{ };
        QOpenGLTexture::TextureFormat format{ };
        bool picker{ };
        bool histogram{ };

        friend bool operator<(const ProgramDesc& a, const ProgramDesc& b) {
            return std::tie(a.target, a.format, a.picker, a.histogram) < 
                   std::tie(b.target, b.format, b.picker, b.histogram);
        }
    };

    QString buildFragmentShader(const ProgramDesc &desc)
    {
        struct TargetVersion {
            QString sampler;
            QString sample;
        };
        struct DataTypeVersion {
            QString prefix;
            QString mapping;
        };
        static auto sTargetVersions = std::map<QOpenGLTexture::Target, TargetVersion>{
            { QOpenGLTexture::Target1D, { "sampler1D", "textureLod(S, TC.x, uLevel)" } },
            { QOpenGLTexture::Target1DArray, { "sampler1DArray", "textureLod(S, vec2(TC.x, uLayer), uLevel)" } },
            { QOpenGLTexture::Target2D, { "sampler2D", "textureLod(S, TC, uLevel)" } },
            { QOpenGLTexture::Target2DArray, { "sampler2DArray", "textureLod(S, vec3(TC, uLayer), uLevel)" } },
            { QOpenGLTexture::Target3D, { "sampler3D", "textureLod(S, vec3(TC, uLayer), uLevel)" } },
            { QOpenGLTexture::TargetCubeMap, { "samplerCube", "textureLod(S, getCubeTexCoord(TC, uFace), uLevel)" } },
            { QOpenGLTexture::TargetCubeMapArray,  { "samplerCubeArray", "textureLod(S, vec4(getCubeTexCoord(TC, uFace), uLayer), uLevel)" } },
            { QOpenGLTexture::Target2DMultisample, { "sampler2DMS", "texelFetch(S, ivec2(TC * (uSize - 1)), s)" } },
            { QOpenGLTexture::Target2DMultisampleArray, { "sampler2DMSArray", "texelFetch(S, ivec3(TC * (uSize - 1), uLayer), s)" } },
        };
        static auto sDataTypeVersions = std::map<TextureDataType, DataTypeVersion>{
            { TextureDataType::Normalized, { "", "color" } },
            { TextureDataType::Normalized_sRGB, { "", "color" } },
            { TextureDataType::Float, { "", "color" } },
            { TextureDataType::Uint8, { "u", "color / 255.0" } },
            { TextureDataType::Uint16, { "u", "color / 65535.0" } },
            { TextureDataType::Uint32, { "u", "color / 4294967295.0" } },
            { TextureDataType::Uint_10_10_10_2, { "u", "color / vec4(vec3(1023.0), 3.0)" } },
            { TextureDataType::Int8, { "i", "color / 127.0" } },
            { TextureDataType::Int16, { "i", "color / 32767.0" } },
            { TextureDataType::Int32, { "i", "color / 2147483647.0" } },
            { TextureDataType::Compressed, { "", "color" } },
        };
        static auto sComponetSwizzle = std::map<int, QString>{
            { 1, "vec4(vec3(color.r), 1)" },
            { 2, "vec4(vec3(color.rg, 0), 1)" },
            { 3, "vec4(color.rgb, 1)" },
            { 4, "color" },
        };
        const auto dataType = getTextureDataType(desc.format);
        const auto targetVersion = sTargetVersions[desc.target];
        const auto dataTypeVersion = sDataTypeVersions[dataType];
        const auto swizzle = sComponetSwizzle[getTextureComponentCount(desc.format)];
        const auto linearToSrgb = (dataType == TextureDataType::Normalized_sRGB ||
                                   dataType == TextureDataType::Float);
        return "#version 330\n"
               "#define S uTexture\n"
               "#define TC vTexCoord\n"
               "#define SAMPLER " + dataTypeVersion.prefix + targetVersion.sampler + "\n"
               "#define SAMPLE " + targetVersion.sample + "\n" +
               "#define MAPPING " + dataTypeVersion.mapping + "\n" +
               "#define SWIZZLE " + swizzle + "\n" +
               "#define LINEAR_TO_SRGB " + QString::number(linearToSrgb) + "\n" +
               "#define PICKER_ENABLED " + QString::number(desc.picker) + "\n" +
               "#define HISTOGRAM_ENABLED " + QString::number(desc.histogram) + "\n" +
               fragmentShaderSource;
    }

    bool buildProgram(QOpenGLShaderProgram &program, const ProgramDesc &desc)
    {
        program.create();
        auto vertexShader = new QOpenGLShader(QOpenGLShader::Vertex, &program);
        auto fragmentShader = new QOpenGLShader(QOpenGLShader::Fragment, &program);
        vertexShader->compileSourceCode(vertexShaderSource);
        fragmentShader->compileSourceCode(buildFragmentShader(desc));
        program.addShader(vertexShader);
        program.addShader(fragmentShader);
        return program.link();
    }
} // namespace

//-------------------------------------------------------------------------

class TextureItem::ProgramCache
{
public:
    QOpenGLShaderProgram *getProgram(const ProgramDesc &desc)
    {
        auto &program = mPrograms[desc];
        if (!program.isLinked())
            if (!buildProgram(program, desc)) {
                mPrograms.erase(desc);
                return nullptr;
            }
        return &program;
    }

private:
    std::map<ProgramDesc, QOpenGLShaderProgram> mPrograms;
};

//-------------------------------------------------------------------------

TextureItem::TextureItem(GLWidget *widget)
    : QObject(widget)
    , mProgramCache(new ProgramCache())
{
    setHistogramBinCount(1);
}

TextureItem::~TextureItem() = default;

void TextureItem::releaseGL()
{
    auto& gl = widget().gl();
    gl.glDeleteTextures(1, &mImageTextureId);

    mProgramCache.reset();
}

void TextureItem::setImage(TextureData image)
{
    const auto w = image.width();
    const auto h = image.height();
    mBoundingRect = { -w / 2, -h / 2, w, h };
    mImage = std::move(image);
    mUpload = true;
    mPreviewTextureId = GL_NONE;
    update();
}

void TextureItem::setPreviewTexture(QOpenGLTexture::Target target,
    QOpenGLTexture::TextureFormat format, GLuint textureId)
{
    if (!mImage.isNull()) {
        mPreviewTarget = target;
        mPreviewFormat = format;
        mPreviewTextureId = textureId;
        update();
    }
}

void TextureItem::setMousePosition(const QPointF &mousePosition)
{
    mMousePosition = mousePosition;
    if (mPickerEnabled)
        update();
}

void TextureItem::setMappingRange(const Range &range)
{
    if (mMappingRange != range) {
        mMappingRange = range;
        update();
    }
}

void TextureItem::setHistogramBinCount(int count)
{
    count = qMax(count / 3, 1) * 3;

    if (mHistogramBins.size() != count) {
        mHistogramBins.resize(count);
        if (mHistogramEnabled)
            update();
    }          
}

void TextureItem::setHistogramBounds(const Range &bounds) 
{
    if (mHistogramBounds != bounds) {
        mHistogramBounds = bounds;
        if (mHistogramEnabled)
            update();
    }
}

void TextureItem::computeHistogramBounds() 
{
    if (!mComputeRange) {
        mComputeRange = new ComputeRange(this);
        connect(mComputeRange, &ComputeRange::rangeComputed,
            this, &TextureItem::histogramBoundsComputed);
    }

    mComputeRange->setImage(
        mPreviewTextureId ? mPreviewTextureId : mImageTextureId,
        mImage, 
        static_cast<int>(mLevel), 
        static_cast<int>(mLayer),
        mFace);

    mComputeRange->update();
}

GLWidget &TextureItem::widget() 
{
    return *qobject_cast<GLWidget*>(parent());
}

void TextureItem::update()
{
    widget().update();
}

void TextureItem::paintGL(const QMatrix4x4 &transform)
{
    Q_ASSERT(glGetError() == GL_NO_ERROR);

    if (updateTexture())
        renderTexture(transform);

    Q_ASSERT(glGetError() == GL_NO_ERROR);
}

bool TextureItem::updateTexture()
{
    if (!mPreviewTextureId && std::exchange(mUpload, false)) {
        // upload/replace texture
        widget().gl().glDeleteTextures(1, &mImageTextureId);
        mImageTextureId = GL_NONE;
        mImage.upload(&mImageTextureId);
        // last version is deleted in QGraphicsView destructor
    }
    return (mPreviewTextureId || mImageTextureId);
}

bool TextureItem::renderTexture(const QMatrix4x4 &transform)
{
    Q_ASSERT(glGetError() == GL_NO_ERROR);
    auto &gl = widget().gl();

    // WORKAROUND: renderer can delete the texture without resetting it
    if (mPreviewTextureId && !gl.glIsTexture(mPreviewTextureId))
        mPreviewTextureId = GL_NONE;

    auto target = mImage.target();
    auto format = mImage.format();
    if (mPreviewTextureId) {
        Singletons::glShareSynchronizer().beginUsage(gl);
        target = mPreviewTarget;
        format = mPreviewFormat;
        gl.glBindTexture(target, mPreviewTextureId);
    }
    else {
        gl.glBindTexture(target, mImageTextureId);
    }

    gl.glEnable(GL_BLEND);
    gl.glBlendEquation(GL_FUNC_ADD);
    gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (target != QOpenGLTexture::Target2DMultisample &&
        target != QOpenGLTexture::Target2DMultisampleArray) {

        gl.glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl.glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        if (mMagnifyLinear) {
            gl.glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl.glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        }
        else {
            gl.glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            gl.glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        }
    }

    const auto desc = ProgramDesc{
        target,
        format,
        mPickerEnabled,
        mHistogramEnabled
    };
    if (auto *program = mProgramCache->getProgram(desc)) {
        program->bind();
        program->setUniformValue("uTexture", 0);
        program->setUniformValue("uTransform", transform);
        program->setUniformValue("uSize", QSizeF(mBoundingRect.size()));
        program->setUniformValue("uLevel", mLevel);
        program->setUniformValue("uFace", mFace);
        program->setUniformValue("uLayer", mLayer / mImage.depth());
        const auto resolve = (mSample < 0);
        program->setUniformValue("uSample", std::max(0, (resolve ? 0 : mSample)));
        program->setUniformValue("uSamples", std::max(1, (resolve ? mImage.samples() : 1)));
        program->setUniformValue("uFlipVertically", mFlipVertically);
        program->setUniformValue("uMappingOffset", 
            static_cast<float>(-mMappingRange.minimum));
        program->setUniformValue("uMappingFactor", 
            static_cast<float>(1 / mMappingRange.range()));
        program->setUniformValue("uColorMask", mColorMask);

#if GL_VERSION_4_2
        if (mPickerEnabled) {
            if (auto gl42 = widget().gl42()) {
                if (!mPickerTexture.isCreated()) {
                    mPickerTexture.setSize(1, 1);
                    mPickerTexture.setFormat(QOpenGLTexture::RGBA32F);
                    mPickerTexture.allocateStorage();
                }
                gl42->glBindImageTexture(1, mPickerTexture.textureId(), 0, GL_FALSE, 
                    0, GL_WRITE_ONLY, GL_RGBA32F);
                program->setUniformValue("uPickerColor", 1);
                program->setUniformValue("uPickerFragCoord", 
                    mMousePosition + QPointF(0.5, 0.5));
            }
        }
        if (mHistogramEnabled) {
            if (auto gl42 = widget().gl42()) {
                if (!mHistogramTexture.isCreated() ||
                     mHistogramTexture.width() != mHistogramBins.size()) {
                    mHistogramTexture.destroy();
                    mHistogramTexture.setSize(mHistogramBins.size());
                    mHistogramTexture.setFormat(QOpenGLTexture::R32U);
                    mHistogramTexture.allocateStorage();
                }
                gl42->glBindImageTexture(2, mHistogramTexture.textureId(), 0, GL_FALSE, 
                    0, GL_READ_WRITE, GL_R32UI);
                program->setUniformValue("uHistogram", 2);
                program->setUniformValue("uHistogramOffset", 
                    static_cast<float>(-mHistogramBounds.minimum));
                const auto scaleToBins = mHistogramBins.size() / 3 - 1;
                program->setUniformValue("uHistogramFactor", 
                    static_cast<float>(1 / mHistogramBounds.range() * scaleToBins));
            }
        }
#endif

        gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);  

#if GL_VERSION_4_2
        if (auto gl42 = widget().gl42())
            gl42->glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);

        auto pickerColor = QVector4D{ };
        if (mPickerEnabled) {
            gl.glBindTexture(mPickerTexture.target(), mPickerTexture.textureId());
            gl.glGetTexImage(mPickerTexture.target(), 0,  
                GL_RGBA, GL_FLOAT, &pickerColor);
        }
        Q_EMIT pickerColorChanged(pickerColor);

        if (mHistogramEnabled) {
            gl.glBindTexture(mHistogramTexture.target(), mHistogramTexture.textureId());
            gl.glGetTexImage(mHistogramTexture.target(), 0,  
                GL_RED_INTEGER, GL_UNSIGNED_INT, mHistogramBins.data());
            updateHistogram();
        }
#endif
    }

    if (mPreviewTextureId) {
        gl.glBindTexture(target, 0);
        Singletons::glShareSynchronizer().endUsage(gl);
    }
    return (glGetError() == GL_NO_ERROR);
}

void TextureItem::updateHistogram()
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
    const auto scale = std::array<qreal, 3>{ 
        1.0 / maxValue[0], 
        1.0 / maxValue[1], 
        1.0 / maxValue[2] 
    };
    for (auto i = 0, c = 0; i < mHistogramBins.size(); ++i, c = (c + 1) % 3) {
        const auto value = (mHistogramBins[i] - mPrevHistogramBins[i]);
        histogram[i] = (1.0 - (value * scale[c]));
    }
    mPrevHistogramBins.swap(mHistogramBins);

    Q_EMIT histogramChanged(histogram);
}

void TextureItem::setColorMask(unsigned int colorMask)
{
    if (mColorMask != colorMask) {
        mColorMask = colorMask;
        update();
    }
}