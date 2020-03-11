#include "TextureEditor.h"
#include "FileDialog.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include "render/GLShareSynchronizer.h"
#include <QGraphicsItem>
#include <QAction>
#include <QScrollBar>
#include <QWheelEvent>
#include <QOpenGLWidget>
#include <QOpenGLTexture>
#include <QOpenGLShader>
#include <QOpenGLFunctions_3_3_Core>
#include <map>
#include <cmath>

extern bool gZeroCopyPreview;

class ZeroCopyItem : public QGraphicsItem
{
static constexpr auto vertexShader = R"(
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

static constexpr auto fragmentShader = R"(

uniform SAMPLER uTexture;
uniform float uLayer;
uniform int uLevel;

in vec2 vTexCoord;
out vec4 oColor;

void main() {
  oColor = vec4(texture(uTexture, SAMPLECOORDS));
}
)";

    QOpenGLFunctions_3_3_Core mGL;
    QScopedPointer<QOpenGLShader> mVertexShader;
    std::map<QOpenGLTexture::Target, QOpenGLShaderProgram> mPrograms;
    GLuint mTextureId{ };
    TextureData mUploadImage;
    QRect mBoundingRect;
    QOpenGLTexture::Target mTarget{ };
    QOpenGLTexture::TextureFormat mFormat{ };
    GLuint mPreviewTextureId{ };
    bool mMagnifyLinear{ };

    bool updateTexture()
    {
        if (!mPreviewTextureId && !mUploadImage.isNull()) {
            // upload/replace texture
            if (!mUploadImage.upload(&mTextureId)) {
                mGL.glDeleteTextures(1, &mTextureId);
                mTextureId = GL_NONE;
            }
            mTarget = mUploadImage.target();
            mFormat = mUploadImage.format();
            mUploadImage = { };

            // last version is deleted in QGraphicsView destructor
        }
        return (mPreviewTextureId || mTextureId);
    }

    enum class FormatType {
        Float, UInt, Int
    };

    FormatType getFormatType(QOpenGLTexture::TextureFormat format)
    {
        using TF = QOpenGLTexture::TextureFormat;
        switch (format) {
            case TF::R8U: case TF::RG8U: case TF::RGB8U: case TF::RGBA8U:
            case TF::R16U: case TF::RG16U: case TF::RGB16U: case TF::RGBA16U:
            case TF::R32U: case TF::RG32U: case TF::RGB32U: case TF::RGBA32U:
            case TF::S8:
                return FormatType::UInt;
            case TF::R8I: case TF::RG8I: case TF::RGB8I: case TF::RGBA8I:
            case TF::R16I: case TF::RG16I: case TF::RGB16I: case TF::RGBA16I:
            case TF::R32I: case TF::RG32I: case TF::RGB32I: case TF::RGBA32I:
                return FormatType::Int;
            default:
                return FormatType::Float;
        }
    }

    QString buildFragmentShader(QOpenGLTexture::Target target, FormatType formatType)
    {
        struct TargetVersion {
            QString sampler;
            QString samplecoords;
        };
        struct FormatTypeVersion {
            QString prefix;
        };
        static auto sTargetVersions = std::map<QOpenGLTexture::Target, TargetVersion>{
            { QOpenGLTexture::Target1D, { "sampler1D", "vTexCoord.x" } },
            { QOpenGLTexture::Target1DArray, { "sampler1DArray", "vTexCoord.x, uLayer" } },
            { QOpenGLTexture::Target2D, { "sampler2D", "vTexCoord.xy" } },
            { QOpenGLTexture::Target2DArray, { "sampler2DArray", "vTexCoord.xy, uLayer" } },
            { QOpenGLTexture::Target3D, { "sampler2DArray", "vTexCoord.xy" } },
            { QOpenGLTexture::TargetCubeMap, { "samplerCube", "vec3(vTexCoord.xy, 0)" } },
            { QOpenGLTexture::TargetCubeMapArray,  { "samplerCubeArray", "vec3(vTexCoord.xy, 0), uLayer" } },
            { QOpenGLTexture::Target2DMultisample, { "sampler2DMS", "vTexCoord.xy" } },
            { QOpenGLTexture::Target2DMultisampleArray, { "sampler2DMSArray", "vTexCoord.xy, uLayer" } },
        };
        static auto sFormatTypeVersions = std::map<FormatType, FormatTypeVersion>{
            { FormatType::Float, { "" } },
            { FormatType::UInt, { "u" } },
            { FormatType::Int, { "i" } },
        };
        const auto targetVersion = sTargetVersions[target];
        const auto formatTypeVersion = sFormatTypeVersions[formatType];
        return "#version 330\n"
               "#define SAMPLER " + formatTypeVersion.prefix + targetVersion.sampler + "\n"
               "#define SAMPLECOORDS " + targetVersion.samplecoords + "\n" +
               fragmentShader;
    }

    QOpenGLShaderProgram &getProgram(QOpenGLTexture::Target target,
        QOpenGLTexture::TextureFormat format)
    {
        auto& program = mPrograms[target];
        if (!program.isLinked()) {
            const auto shader = buildFragmentShader(target, getFormatType(format));
            auto fragmentShader = new QOpenGLShader(QOpenGLShader::Fragment, &program);
            fragmentShader->compileSourceCode(shader);
            program.create();
            program.addShader(mVertexShader.get());
            program.addShader(fragmentShader);
            program.link();
        }
        return program;
    }

    void renderTexture(const QMatrix &transform)
    {
        mGL.glEnable(mTarget);

        if (mPreviewTextureId) {
            Singletons::glShareSynchronizer().beginUsage(mGL);
            mGL.glBindTexture(mTarget, mPreviewTextureId);

            // try to generate mipmaps
            mGL.glGenerateMipmap(mTarget);
            glGetError();
        }
        else {
            mGL.glBindTexture(mTarget, mTextureId);
        }

        mGL.glEnable(GL_BLEND);
        mGL.glBlendEquation(GL_FUNC_ADD);
        mGL.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        mGL.glTexParameteri(mTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        mGL.glTexParameteri(mTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        mGL.glTexParameteri(mTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        mGL.glTexParameteri(mTarget, GL_TEXTURE_MAG_FILTER,
            mMagnifyLinear ? GL_LINEAR : GL_NEAREST);

        auto& program = getProgram(mTarget, mFormat);
        program.bind();
        program.setUniformValue("uTexture", 0);
        program.setUniformValue("uTransform", transform);
        program.setUniformValue("uLevel", 0);
        program.setUniformValue("uLayer", 0);

        mGL.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        if (mPreviewTextureId) {
            mGL.glBindTexture(mTarget, 0);
            Singletons::glShareSynchronizer().endUsage(mGL);
        }
        mGL.glDisable(mTarget);
    }

public:
    void setImage(TextureData image)
    {
        prepareGeometryChange();
        const auto w = image.width();
        const auto h = image.height();
        mBoundingRect = { -w / 2, -h / 2, w, h };
        mUploadImage = std::move(image);
        mPreviewTextureId = GL_NONE;
        update();
    }

    void setPreviewTexture(QOpenGLTexture::Target target,
        QOpenGLTexture::TextureFormat format, GLuint textureId)
    {
        mTarget = target;
        mFormat = format;
        mPreviewTextureId = textureId;
        update();
    }

    GLuint resetTexture()
    {
        return std::exchange(mTextureId, GL_NONE);
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

        if (mPrograms.empty()) {
            mGL.initializeOpenGLFunctions();
            mVertexShader.reset(new QOpenGLShader(QOpenGLShader::Vertex));
            mVertexShader->compileSourceCode(vertexShader);
        }

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
        mZeroCopyItem = new ZeroCopyItem();
        scene()->addItem(mZeroCopyItem);
    }

    auto image = TextureData{ };
    image.create(QOpenGLTexture::Target2D,
        QOpenGLTexture::TextureFormat::RGBA8_UNorm, 1, 1, 1, 1);
    replace(std::move(image), false);
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

void TextureEditor::updatePreviewTexture(QOpenGLTexture::Target target,
    QOpenGLTexture::TextureFormat format, GLuint textureId)
{
    if (mZeroCopyItem)
        mZeroCopyItem->setPreviewTexture(target, format, textureId);
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
