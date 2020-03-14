#include "TextureEditor.h"
#include "FileDialog.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include "render/GLShareSynchronizer.h"
#include "session/Item.h"
#include <QGraphicsItem>
#include <QAction>
#include <QScrollBar>
#include <QWheelEvent>
#include <QOpenGLWidget>
#include <QOpenGLTexture>
#include <QOpenGLShader>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLDebugLogger>
#include <map>
#include <cmath>

extern bool gZeroCopyPreview;

namespace {
  static constexpr auto vertexShaderSource = R"(
#version 330

uniform mat4 uTransform;
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
  gl_Position = uTransform * vec4(pos, 0.0, 1.0);
}
)";

static constexpr auto fragmentShaderSource = R"(

uniform SAMPLER uTexture;
uniform vec2 uSize;
uniform float uLevel;
uniform float uLayer;
uniform float uFace;
uniform int uSample;

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

void main() {
  vec4 color = vec4(SAMPLE);
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
            { QOpenGLTexture::TargetCubeMap, { "samplerCube", "textureLod(S, vec3(TC, uFace), uLevel)" } },
            { QOpenGLTexture::TargetCubeMapArray,  { "samplerCubeArray", "textureLod(S, vec4(TC, uFace, uLayer), uLevel)" } },
            { QOpenGLTexture::Target2DMultisample, { "sampler2DMS", "texelFetch(S, ivec2(TC * uSize), uSample)" } },
            { QOpenGLTexture::Target2DMultisampleArray, { "sampler2DMSArray", "texelFetch(S, ivec3(TC * uSize, uLayer), uSample)" } },
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
        if (!vertexShader->compileSourceCode(vertexShaderSource))
            return false;
        if (!fragmentShader->compileSourceCode(buildFragmentShader(target, format)))
            return false;
        program.addShader(vertexShader);
        program.addShader(fragmentShader);
        return program.link();
    }
} // namespace

class ZeroCopyContext : public QObject
{
private:
    using ProgramKey = std::tuple<QOpenGLTexture::Target, QOpenGLTexture::TextureFormat>;

    QOpenGLFunctions_3_3_Core mGL;
    QOpenGLDebugLogger mDebugLogger;
    std::map<ProgramKey, QOpenGLShaderProgram> mPrograms;
    bool mInitialized{ };

    void initialize()
    {
        if (std::exchange(mInitialized, true))
            return;

        mGL.initializeOpenGLFunctions();

        if (mDebugLogger.initialize()) {
            mDebugLogger.disableMessages(QOpenGLDebugMessage::AnySource,
                QOpenGLDebugMessage::AnyType, QOpenGLDebugMessage::NotificationSeverity);
            QObject::connect(&mDebugLogger, &QOpenGLDebugLogger::messageLogged,
                this, &ZeroCopyContext::handleDebugMessage);
            mDebugLogger.startLogging(QOpenGLDebugLogger::SynchronousLogging);
        }
    }

    void handleDebugMessage(const QOpenGLDebugMessage &message)
    {
        const auto text = message.message();
        qDebug() << text;
    }

public:
    explicit ZeroCopyContext(QObject *parent)
        : QObject(parent)
    {
    }

    QOpenGLFunctions_3_3_Core &gl()
    {
        initialize();
        return mGL;
    }

    QOpenGLShaderProgram *getProgram(QOpenGLTexture::Target target,
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
};

class ZeroCopyItem : public QGraphicsItem
{
    ZeroCopyContext &mContext;
    QRect mBoundingRect;
    TextureData mImage;
    GLuint mImageTextureId{ };
    QOpenGLTexture::Target mPreviewTarget{ };
    GLuint mPreviewTextureId{ };
    bool mMagnifyLinear{ };
    bool mUpload{ };

    bool updateTexture()
    {
        if (!mPreviewTextureId && std::exchange(mUpload, false)) {
            // upload/replace texture
            if (!mImage.upload(&mImageTextureId)) {
                mContext.gl().glDeleteTextures(1, &mImageTextureId);
                mImageTextureId = GL_NONE;
            }
            // last version is deleted in QGraphicsView destructor
        }
        return (mPreviewTextureId || mImageTextureId);
    }

    bool renderTexture(const QMatrix &transform)
    {
        Q_ASSERT(glGetError() == GL_NO_ERROR);
        auto &gl = mContext.gl();

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
            if (mImage.levels() > 1) {
                gl.glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                gl.glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            }
            else {
                gl.glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                gl.glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            }
        }

        if (auto *program = mContext.getProgram(target, mImage.format())) {
            program->bind();
            program->setUniformValue("uTexture", 0);
            program->setUniformValue("uTransform", transform);
            program->setUniformValue("uSize",
                QPointF(mBoundingRect.width(), mBoundingRect.height()));
            program->setUniformValue("uLevel", 0.0f);
            program->setUniformValue("uFace", 0.0f);
            program->setUniformValue("uLayer", 0.0f);
            program->setUniformValue("uSample", 0);
            gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }

        if (mPreviewTextureId) {
            gl.glBindTexture(target, 0);
            Singletons::glShareSynchronizer().endUsage(gl);
        }
        return (glGetError() == GL_NO_ERROR);
    }

public:
    ZeroCopyItem(ZeroCopyContext *context)
        : mContext(*context)
    {
    }

    void setImage(TextureData image)
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

    void setPreviewTexture(QOpenGLTexture::Target target, GLuint textureId)
    {
        if (!mImage.isNull()) {
            mPreviewTarget = target;
            mPreviewTextureId = textureId;
            update();
        }
    }

    GLuint resetTexture()
    {
        return std::exchange(mImageTextureId, GL_NONE);
    }

    void setMagnifyLinear(bool magnifyLinear)
    {
        mMagnifyLinear = magnifyLinear;
    }

    QRectF boundingRect() const override
    {
        return mBoundingRect;
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        Q_ASSERT(glGetError() == GL_NO_ERROR);
        painter->beginNativePainting();

        const auto s = mBoundingRect.size();
        const auto x = painter->clipBoundingRect().left() - (s.width() % 2 ? 0.5 : 0.0);
        const auto y = painter->clipBoundingRect().top() - (s.height() % 2 ? 0.5 : 0.0);
        const auto scale = painter->combinedTransform().m11();
        const auto width = static_cast<qreal>(painter->window().width());
        const auto height = static_cast<qreal>(painter->window().height());
        const auto transform = QMatrix(
            s.width() / width * scale, 0,
            0, -s.height() / height * scale,
            2 * -(x * scale + width / 2) / width,
            2 * (y * scale + height / 2) / height);

        if (updateTexture())
            renderTexture(transform);

        Q_ASSERT(glGetError() == GL_NO_ERROR);
        painter->endNativePainting();
    }
};

//-------------------------------------------------------------------------

TextureEditor::TextureEditor(QString fileName, QWidget *parent)
    : QGraphicsView(parent)
    , mFileName(fileName)
{
    setTransformationAnchor(AnchorUnderMouse);

    if (gZeroCopyPreview) {
        setViewport(new QOpenGLWidget(this));
        setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    }

    setScene(new QGraphicsScene(this));

    auto pen = QPen();
    auto color = QColor(Qt::gray);
    pen.setWidth(1);
    pen.setCosmetic(true);
    pen.setColor(color);
    mBorder = new QGraphicsPathItem();
    mBorder->setPen(pen);
    mBorder->setBrush(Qt::NoBrush);
    mBorder->setZValue(1);
    scene()->addItem(mBorder);

    if (gZeroCopyPreview) {
        mZeroCopyContext = new ZeroCopyContext(this);
        mZeroCopyItem = new ZeroCopyItem(mZeroCopyContext);
        scene()->addItem(mZeroCopyItem);
    }

    setZoom(mZoom);
    updateTransform(1.0);
}

TextureEditor::~TextureEditor() 
{
    if (mZeroCopyItem) {
        if (auto texture = mZeroCopyItem->resetTexture()) {
            auto glWidget = qobject_cast<QOpenGLWidget*>(viewport());
            auto context = glWidget->context();
            auto surface = context->surface();
            if (context->makeCurrent(surface)) {
                auto& gl = *context->functions();
                gl.glDeleteTextures(1, &texture);
                context->doneCurrent();
            }
        }
    }
    delete scene();
}

QList<QMetaObject::Connection> TextureEditor::connectEditActions(
        const EditActions &actions)
{
    auto c = QList<QMetaObject::Connection>();

    actions.windowFileName->setText(fileName());
    actions.windowFileName->setEnabled(isModified());
    c += connect(this, &TextureEditor::fileNameChanged,
                 actions.windowFileName, &QAction::setText);
    c += connect(this, &TextureEditor::modificationChanged,
                 actions.windowFileName, &QAction::setEnabled);
    return c;
}

void TextureEditor::setFileName(QString fileName)
{
    mFileName = fileName;
    emit fileNameChanged(mFileName);
}

bool TextureEditor::load(const QString &fileName, TextureData *texture)
{
    if (FileDialog::isEmptyOrUntitled(fileName))
        return false;

    auto file = TextureData();
    if (!file.load(fileName))
        return false;

    *texture = file;
    return true;
}

bool TextureEditor::load()
{
    auto image = TextureData();
    if (!load(mFileName, &image))
        return false;

    replace(image, false);

    setModified(false);
    emit dataChanged();
    return true;
}

bool TextureEditor::save()
{
    if (!mTexture.save(fileName()))
        return false;

    setModified(false);
    emit dataChanged();
    return true;
}

void TextureEditor::replace(TextureData texture, bool emitDataChanged)
{
    if (texture == mTexture)
        return;

    mTexture = texture;
    if (mZeroCopyItem) {
        mZeroCopyItem->setImage(mTexture);
        setBounds(mZeroCopyItem->boundingRect().toRect());
    }
    else {
        delete mPixmapItem;
        auto pixmap = QPixmap::fromImage(mTexture.toImage(),
            Qt::NoOpaqueDetection | Qt::NoFormatConversion);
        mPixmapItem = new QGraphicsPixmapItem(pixmap);
        scene()->addItem(mPixmapItem);
        setBounds(pixmap.rect());
    }

    if (!FileDialog::isEmptyOrUntitled(mFileName))
        setModified(true);

    if (emitDataChanged)
        emit dataChanged();
}

void TextureEditor::updatePreviewTexture(
    QOpenGLTexture::Target target, GLuint textureId)
{
    if (mZeroCopyItem)
        mZeroCopyItem->setPreviewTexture(target, textureId);
}

void TextureEditor::setModified(bool modified)
{
    if (mModified != modified) {
        mModified = modified;
        emit modificationChanged(modified);
    }
}

void TextureEditor::wheelEvent(QWheelEvent *event)
{
    if (!event->modifiers()) {
        const auto min = -3;
        const auto max = 4;
        auto delta = (event->delta() > 0 ? 1 : -1);
        setZoom(std::max(min, std::min(mZoom + delta, max)));
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void TextureEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    setZoom(0);
    QGraphicsView::mouseDoubleClickEvent(event);
}

void TextureEditor::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MidButton) {
        mPan = true;
        mPanStartX = event->x();
        mPanStartY = event->y();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    updateMousePosition(event);

    QGraphicsView::mousePressEvent(event);
}

void TextureEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (mPan) {
        horizontalScrollBar()->setValue(
            horizontalScrollBar()->value() - (event->x() - mPanStartX));
        verticalScrollBar()->setValue(
            verticalScrollBar()->value() - (event->y() - mPanStartY));
        mPanStartX = event->x();
        mPanStartY = event->y();
        return;
    }

    updateMousePosition(event);

    QGraphicsView::mouseMoveEvent(event);
}

void TextureEditor::updateMousePosition(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
        Singletons::synchronizeLogic().setMousePosition({ 
            event->x() / static_cast<qreal>(width()),
            event->y() / static_cast<qreal>(height()),
        });
}

void TextureEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MidButton) {
        mPan = false;
        setCursor(Qt::ArrowCursor);
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void TextureEditor::setBounds(QRect bounds)
{
    if (bounds == mBounds)
        return;
    mBounds = bounds;

    auto inside = QPainterPath();
    inside.addRect(bounds);
    mBorder->setPath(inside);

    const auto margin = 15;
    bounds.adjust(-margin, -margin, margin, margin);
    setSceneRect(bounds);
}

void TextureEditor::setZoom(int zoom)
{
    if (mZoom == zoom)
        return;

    mZoom = zoom;
    const auto scale = (mZoom < 0 ?
        1.0 / (1 << (-mZoom)) :
        1 << (mZoom));
    updateTransform(scale);

    if (mZeroCopyItem)
        mZeroCopyItem->setMagnifyLinear(scale <= 4);
}

void TextureEditor::updateTransform(double scale)
{
    const auto transform = QTransform().scale(scale, scale);
    setTransform(transform);
    updateBackground(transform);
}

void TextureEditor::updateBackground(const QTransform& transform) 
{
    QPixmap pm(2, 2);
    QPainter pmp(&pm);
    pmp.fillRect(0, 0, 1, 1, 0x959595);
    pmp.fillRect(1, 1, 1, 1, 0x959595);
    pmp.fillRect(0, 1, 1, 1, 0x888888);
    pmp.fillRect(1, 0, 1, 1, 0x888888);
    pmp.end();

    auto brush = QBrush(pm);
    brush.setTransform(transform.inverted().translate(0, 1).scale(16, 16));
    setBackgroundBrush(brush);
}
