#include "TextureItem.h"
#include <QOpenGLWidget>
#include <QOpenGLDebugMessage>
#include "Singletons.h"
#include "render/GLShareSynchronizer.h"
#include <optional>
#include <cmath>

namespace {
  static constexpr auto vertexShaderSource = R"(
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

static constexpr auto fragmentShaderSource = R"(

#ifdef GL_ARB_texture_cube_map_array
#extension GL_ARB_texture_cube_map_array: enable
#endif

#ifdef GL_ARB_shader_image_load_store
#extension GL_ARB_shader_image_load_store: enable
layout(rgba32f) uniform image1D uPickerColor;
uniform vec2 uPickerFragCoord;
#endif

uniform SAMPLER uTexture;
uniform vec2 uSize;
uniform float uLevel;
uniform float uLayer;
uniform int uFace;
uniform int uSample;
uniform int uSamples;
uniform bool uPickerEnabled;

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
  for (int sample = uSample; sample < uSample + uSamples; ++sample)
    color += vec4(SAMPLE);
  color /= float(uSamples);

#ifdef GL_ARB_shader_image_load_store
  if (uPickerEnabled && gl_FragCoord.xy == uPickerFragCoord)
    imageStore(uPickerColor, 0, color);
#endif

  color = MAPPING;
  color = SWIZZLE;
  oColor = color;
}
)";

    QString buildFragmentShader(QOpenGLTexture::Target target,
        QOpenGLTexture::TextureFormat format)
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
            { QOpenGLTexture::Target2DMultisample, { "sampler2DMS", "texelFetch(S, ivec2(TC * uSize), sample)" } },
            { QOpenGLTexture::Target2DMultisampleArray, { "sampler2DMSArray", "texelFetch(S, ivec3(TC * uSize, uLayer), sample)" } },
        };
        static auto sDataTypeVersions = std::map<TextureDataType, DataTypeVersion>{
            { TextureDataType::Normalized, { "", "color" } },
            { TextureDataType::Normalized_sRGB, { "", "vec4(linearToSrgb(color.rgb), color.a)" } },
            { TextureDataType::Float, { "", "vec4(linearToSrgb(color.rgb), color.a)" } },
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
        const auto dataType = getTextureDataType(format);
        const auto targetVersion = sTargetVersions[target];
        const auto dataTypeVersion = sDataTypeVersions[dataType];
        const auto swizzle = sComponetSwizzle[getTextureComponentCount(format)];
        return "#version 330\n"
               "#define S uTexture\n"
               "#define TC vTexCoord\n"
               "#define SAMPLER " + dataTypeVersion.prefix + targetVersion.sampler + "\n"
               "#define SAMPLE " + targetVersion.sample + "\n" +
               "#define MAPPING " + dataTypeVersion.mapping + "\n" +
               "#define SWIZZLE " + swizzle + "\n" +
               fragmentShaderSource;
    }

    bool buildProgram(QOpenGLShaderProgram &program,
        QOpenGLTexture::Target target, QOpenGLTexture::TextureFormat format)
    {
        program.create();
        auto vertexShader = new QOpenGLShader(QOpenGLShader::Vertex, &program);
        auto fragmentShader = new QOpenGLShader(QOpenGLShader::Fragment, &program);
        vertexShader->compileSourceCode(vertexShaderSource);
        fragmentShader->compileSourceCode(buildFragmentShader(target, format));
        program.addShader(vertexShader);
        program.addShader(fragmentShader);
        return program.link();
    }
} // namespace

//-------------------------------------------------------------------------

class ZeroCopyContext final : public QObject
{
public:
    explicit ZeroCopyContext(QObject *parent = nullptr);

    QOpenGLFunctions_3_3_Core &gl();
    QOpenGLFunctions_4_2_Core *gl42();
    QOpenGLShaderProgram *getProgram(QOpenGLTexture::Target target,
        QOpenGLTexture::TextureFormat format);

private:
    using ProgramKey = std::tuple<QOpenGLTexture::Target, QOpenGLTexture::TextureFormat>;
    void handleDebugMessage(const QOpenGLDebugMessage &message);

    QOpenGLFunctions_3_3_Core mGL;
    std::optional<QOpenGLFunctions_4_2_Core> mGL42;
    QOpenGLDebugLogger mDebugLogger;
    std::map<ProgramKey, QOpenGLShaderProgram> mPrograms;
};

ZeroCopyContext::ZeroCopyContext(QObject *parent)
    : QObject(parent)
{
    mGL.initializeOpenGLFunctions();
    mGL42.emplace();
    if (!mGL42->initializeOpenGLFunctions())
        mGL42.reset();

#if !defined(NDEBUG)
    if (mDebugLogger.initialize()) {
        mDebugLogger.disableMessages(QOpenGLDebugMessage::AnySource,
            QOpenGLDebugMessage::AnyType, QOpenGLDebugMessage::NotificationSeverity);
        QObject::connect(&mDebugLogger, &QOpenGLDebugLogger::messageLogged,
            this, &ZeroCopyContext::handleDebugMessage);
        mDebugLogger.startLogging(QOpenGLDebugLogger::SynchronousLogging);
    }
#endif
}

QOpenGLFunctions_3_3_Core &ZeroCopyContext::gl()
{
    return mGL;
}

QOpenGLFunctions_4_2_Core *ZeroCopyContext::gl42()
{
    return (mGL42.has_value() ? &mGL42.value() : nullptr);
}

QOpenGLShaderProgram *ZeroCopyContext::getProgram(QOpenGLTexture::Target target,
    QOpenGLTexture::TextureFormat format)
{
    const auto key = std::make_tuple(target, format);
    auto &program = mPrograms[key];
    if (!program.isLinked())
        if (!buildProgram(program, target, format)) {
            mPrograms.erase(key);
            return nullptr;
        }
    return &program;
}

void ZeroCopyContext::handleDebugMessage(const QOpenGLDebugMessage &message)
{
    const auto text = message.message();
    qDebug() << text;
}

//-------------------------------------------------------------------------

TextureItem::TextureItem(QObject *parent)
    : QObject(parent)
{
}

TextureItem::~TextureItem()
{
    Q_ASSERT(!mContext);
}

void TextureItem::releaseGL()
{
    mContext.reset();
}

void TextureItem::setImage(TextureData image)
{
    prepareGeometryChange();
    const auto w = image.width();
    const auto h = image.height();
    mBoundingRect = { -w / 2, -h / 2, w, h };
    mImage = std::move(image);
    mUpload = true;
    mPreviewTextureId = GL_NONE;
    update();
}

void TextureItem::setPreviewTexture(QOpenGLTexture::Target target, GLuint textureId)
{
    if (!mImage.isNull()) {
        mPreviewTarget = target;
        mPreviewTextureId = textureId;
        update();
    }
}

GLuint TextureItem::resetTexture()
{
    return std::exchange(mImageTextureId, GL_NONE);
}

void TextureItem::setMousePosition(const QPointF &mousePosition)
{
    mMousePosition = mousePosition;
    if (mPickerEnabled)
        update();
}

void TextureItem::paint(QPainter *painter,
    const QStyleOptionGraphicsItem *, QWidget *)
{
    Q_ASSERT(glGetError() == GL_NO_ERROR);
    painter->beginNativePainting();

    const auto s = mBoundingRect.size();
    const auto x = painter->clipBoundingRect().left() - (s.width() % 2 ? 0.5 : 0.0);
    const auto y = painter->clipBoundingRect().top() - (s.height() % 2 ? 0.5 : 0.0);
    const auto scale = painter->combinedTransform().m11();
    const auto width = static_cast<qreal>(painter->window().width());
    const auto height = static_cast<qreal>(painter->window().height());
    const auto transform = QTransform(
        s.width() / width * scale, 0, 0,
        0, -s.height() / height * scale, 0,
        2 * -(x * scale + width / 2) / width,
        2 * (y * scale + height / 2) / height, 1);

    if (updateTexture())
        renderTexture(transform);

    Q_ASSERT(glGetError() == GL_NO_ERROR);
    painter->endNativePainting();
}

ZeroCopyContext &TextureItem::context()
{
    if (!mContext)
        mContext.reset(new ZeroCopyContext());
    return *mContext;
}

bool TextureItem::updateTexture()
{
    if (!mPreviewTextureId && std::exchange(mUpload, false)) {
        // upload/replace texture
        context().gl().glDeleteTextures(1, &mImageTextureId);
        mImageTextureId = GL_NONE;
        mImage.upload(&mImageTextureId);
        // last version is deleted in QGraphicsView destructor
    }
    return (mPreviewTextureId || mImageTextureId);
}

bool TextureItem::renderTexture(const QMatrix4x4 &transform)
{
    Q_ASSERT(glGetError() == GL_NO_ERROR);
    QOpenGLVertexArrayObject::Binder vaoBinder(&mVao);
    auto &gl = context().gl();

    // WORKAROUND: renderer can delete the texture without resetting it
    if (mPreviewTextureId && !gl.glIsTexture(mPreviewTextureId))
        mPreviewTextureId = GL_NONE;

    auto target = mImage.target();
    if (mPreviewTextureId) {
        Singletons::glShareSynchronizer().beginUsage(gl);
        target = mPreviewTarget;
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

    if (auto *program = context().getProgram(target, mImage.format())) {
        program->bind();
        program->setUniformValue("uTexture", 0);
        program->setUniformValue("uTransform", transform);
        program->setUniformValue("uSize", QSizeF(mBoundingRect.size()));
        program->setUniformValue("uLevel", mLevel);
        program->setUniformValue("uFace", mFace);
        program->setUniformValue("uLayer", mLayer);
        const auto resolve = (mSample < 0);
        program->setUniformValue("uSample", std::max(0, (resolve ? 0 : mSample)));
        program->setUniformValue("uSamples", std::max(1, (resolve ? mImage.samples() : 1)));
        program->setUniformValue("uFlipVertically", mFlipVertically);
        program->setUniformValue("uPickerEnabled", mPickerEnabled);

        if (mPickerEnabled) {
            if (auto gl42 = context().gl42()) {
                if (!mPickerTexture.isCreated()) {
                    mPickerTexture.setSize(1, 1);
                    mPickerTexture.setFormat(QOpenGLTexture::RGBA32F);
                    mPickerTexture.allocateStorage();
                }
                gl.glBindTexture(mPickerTexture.target(), mPickerTexture.textureId());
                gl42->glBindImageTexture(1, mPickerTexture.textureId(), 0, GL_FALSE, 
                    0, GL_WRITE_ONLY, GL_RGBA32F);
                program->setUniformValue("uPickerColor", 1);
                program->setUniformValue("uPickerFragCoord", 
                    mMousePosition + QPointF(0.5, 0.5));
            }
        }
  
        gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);  

        auto pickerColor = QVector4D{ };
        if (mPickerEnabled) {
            gl.glBindTexture(mPickerTexture.target(), mPickerTexture.textureId());
            gl.glGetTexImage(mPickerTexture.target(), 0,  GL_RGBA, GL_FLOAT, &pickerColor);
        }
        Q_EMIT pickerColorChanged(pickerColor);
    }

    if (mPreviewTextureId) {
        gl.glBindTexture(target, 0);
        Singletons::glShareSynchronizer().endUsage(gl);
    }
    return (glGetError() == GL_NO_ERROR);
}
