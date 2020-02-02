#include "ImageEditor.h"
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

extern bool gZeroCopyPreview;

class ZeroCopyItem : public QGraphicsItem
{
static constexpr auto vertexShader = R"(
#version 330

uniform mat4 uTransform;
uniform bool uFlipY;
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
  if (uFlipY)
    vTexCoord.y = 1.0 - vTexCoord.y;
  gl_Position = uTransform * vec4(pos, 0.0, 1.0);
}
)";
static constexpr auto fragmentShader = R"(
#version 330

uniform sampler2D uTexture;
in vec2 vTexCoord;
out vec4 oColor;

void main() {
  oColor = texture(uTexture, vTexCoord);
}
)";

private:
    QOpenGLFunctions_3_3_Core mGL;
    QScopedPointer<QOpenGLShaderProgram> mProgram;
    QOpenGLTexture *mTexture{ };
    QMetaObject::Connection mTextureContextConnection;
    ImageData mUploadImage;
    QRect mBoundingRect;
    GLuint mPreviewTextureId{ };
    bool mPreviewFlipY{ };
    bool mMagnifyLinear{ };

public:
    void setImage(ImageData image)
    {
        prepareGeometryChange();
        mUploadImage = image;
        const auto w = image.width();
        const auto h = image.height();
        mBoundingRect = { -w / 2, -h / 2, w, h };
        mPreviewTextureId = GL_NONE;
        update();
    }

    void setPreviewTexture(GLuint textureId, bool flipY)
    {
        mPreviewTextureId = textureId;
        mPreviewFlipY = flipY;
        update();
    }

    QOpenGLTexture* resetTexture()
    {
        return std::exchange(mTexture, nullptr);
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
        if (!mProgram)
            initialize();

        if (!mPreviewTextureId && !mUploadImage.isNull()) {
            // delete previous version in current thread
            if (mTexture)
                delete mTexture;

            // upload texture
            mTexture = new QOpenGLTexture(mUploadImage.image(),
                QOpenGLTexture::MipMapGeneration::DontGenerateMipMaps);
            mUploadImage = { };

            // last version is deleted in QGraphicsView destructor
        }

        painter->beginNativePainting();

        mGL.glEnable(GL_BLEND);
        mGL.glBlendEquation(GL_ADD);
        mGL.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        auto flipY = false;
        if (mPreviewTextureId) {
            Singletons::glShareSynchronizer().beginUsage(mGL);
            mGL.glEnable(GL_TEXTURE_2D);
            mGL.glBindTexture(GL_TEXTURE_2D, mPreviewTextureId);
            mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                GL_CLAMP_TO_EDGE);
            mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                GL_CLAMP_TO_EDGE);
            mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                GL_LINEAR_MIPMAP_LINEAR);
            mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
               mMagnifyLinear ? GL_LINEAR : GL_NEAREST);
            mGL.glGenerateMipmap(GL_TEXTURE_2D);
            flipY = mPreviewFlipY;
        }
        else if (mTexture) {
            mTexture->bind();
            mTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
            mTexture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
            mTexture->setMagnificationFilter(mMagnifyLinear ?
                QOpenGLTexture::Linear : QOpenGLTexture::Nearest);
        }

        const auto scale = painter->combinedTransform().m11();
        const auto s = mBoundingRect.size();
        const auto cr = painter->clipBoundingRect();
        const auto w = painter->window();
        const auto transform = QMatrix(
            s.width() / static_cast<qreal>(w.width()) * scale, 0,
            0, -s.height() / static_cast<qreal>(w.height()) * scale,
            2 * -(cr.left() * scale + w.width() / 2) / static_cast<qreal>(w.width()),
            2 * (cr.top() * scale + w.height() / 2) / static_cast<qreal>(w.height()));

        mProgram->bind();
        mProgram->setUniformValue("uTexture", 0);
        mProgram->setUniformValue("uTransform", transform);
        mProgram->setUniformValue("uFlipY", flipY);

        mGL.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        if (mPreviewTextureId) {
            mGL.glBindTexture(GL_TEXTURE_2D, 0);
            mGL.glDisable(GL_TEXTURE_2D);
            Singletons::glShareSynchronizer().endUsage(mGL);
        }

        painter->endNativePainting();
    }

private:
    void initialize()
    {
        mGL.initializeOpenGLFunctions();

        auto vs = new QOpenGLShader(QOpenGLShader::Vertex);
        vs->compileSourceCode(vertexShader);

        auto fs = new QOpenGLShader(QOpenGLShader::Fragment);
        fs->compileSourceCode(fragmentShader);

        mProgram.reset(new QOpenGLShaderProgram());
        mProgram->addShader(vs);
        mProgram->addShader(fs);
        mProgram->link();
    }
};

//-------------------------------------------------------------------------

ImageEditor::ImageEditor(QString fileName, QWidget *parent)
    : QGraphicsView(parent)
    , mFileName(fileName)
{
    setTransformationAnchor(AnchorUnderMouse);

    if (gZeroCopyPreview) {
        setViewport(new QOpenGLWidget(this));
        setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    }

    setScene(new QGraphicsScene(this));

    if (gZeroCopyPreview) {
        mZeroCopyItem = new ZeroCopyItem();
        scene()->addItem(mZeroCopyItem);
    }

    auto color = QColor(Qt::black);
    color.setAlphaF(0.2);
    mInside = new QGraphicsPathItem();
    mInside->setPen(Qt::NoPen);
    mInside->setBrush(QBrush(color));
    mInside->setZValue(-1);
    scene()->addItem(mInside);

    auto pen = QPen();
    pen.setWidth(2);
    pen.setCosmetic(true);
    pen.setColor(Qt::gray);
    mBorder = new QGraphicsPathItem();
    mBorder->setPen(pen);
    mBorder->setBrush(Qt::NoBrush);
    mBorder->setZValue(1);
    scene()->addItem(mBorder);

    auto image = ImageData{ };
    image.create(1, 1, QOpenGLTexture::TextureFormat::RGBA8_UNorm);
    replace(std::move(image), false);
    setZoom(mZoom);
    updateTransform(1.0);
}

ImageEditor::~ImageEditor() 
{
    if (mZeroCopyItem) {
        if (auto texture = mZeroCopyItem->resetTexture()) {
            auto glWidget = qobject_cast<QOpenGLWidget*>(viewport());
            auto context = glWidget->context();
            auto surface = context->surface();
            if (context->makeCurrent(surface)) {
                delete texture;
                context->doneCurrent();
            }
        }
    }
    delete scene();
}

QList<QMetaObject::Connection> ImageEditor::connectEditActions(
        const EditActions &actions)
{
    auto c = QList<QMetaObject::Connection>();

    actions.windowFileName->setText(fileName());
    actions.windowFileName->setEnabled(isModified());
    c += connect(this, &ImageEditor::fileNameChanged,
                 actions.windowFileName, &QAction::setText);
    c += connect(this, &ImageEditor::modificationChanged,
                 actions.windowFileName, &QAction::setEnabled);
    return c;
}

void ImageEditor::setFileName(QString fileName)
{
    mFileName = fileName;
    emit fileNameChanged(mFileName);
}

bool ImageEditor::load(const QString &fileName, ImageData *image)
{
    if (FileDialog::isEmptyOrUntitled(fileName))
        return false;

    auto file = ImageData();
    if (!file.load(fileName))
        return false;

    *image = file;
    return true;
}

bool ImageEditor::load()
{
    auto image = ImageData();
    if (!load(mFileName, &image))
        return false;

    replace(image, false);

    setModified(false);
    emit dataChanged();
    return true;
}

bool ImageEditor::save()
{
    if (!mImage.save(fileName()))
        return false;

    setModified(false);
    emit dataChanged();
    return true;
}

void ImageEditor::replace(ImageData image, bool emitDataChanged)
{
    if (image.constBits() == mImage.constBits())
        return;

    mImage = image;
    if (mZeroCopyItem) {
        mZeroCopyItem->setImage(mImage);
        setBounds(mZeroCopyItem->boundingRect().toRect());
    }
    else {
        delete mPixmapItem;
        auto pixmap = QPixmap::fromImage(mImage.image(),
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

void ImageEditor::updatePreviewTexture(unsigned int textureId, bool flipY)
{
    if (mZeroCopyItem)
        mZeroCopyItem->setPreviewTexture(textureId, flipY);
}

void ImageEditor::setModified(bool modified)
{
    if (mModified != modified) {
        mModified = modified;
        emit modificationChanged(modified);
    }
}

void ImageEditor::wheelEvent(QWheelEvent *event)
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

void ImageEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    setZoom(0);
    QGraphicsView::mouseDoubleClickEvent(event);
}

void ImageEditor::mousePressEvent(QMouseEvent *event)
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

void ImageEditor::mouseMoveEvent(QMouseEvent *event)
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

void ImageEditor::updateMousePosition(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
        Singletons::synchronizeLogic().setMousePosition({ 
            event->x() / static_cast<qreal>(width()),
            event->y() / static_cast<qreal>(height()),
        });
}

void ImageEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MidButton) {
        mPan = false;
        setCursor(Qt::ArrowCursor);
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void ImageEditor::setBounds(QRect bounds)
{
    if (bounds == mBounds)
        return;
    mBounds = bounds;

    auto inside = QPainterPath();
    inside.addRect(bounds);
    mInside->setPath(inside);
    mBorder->setPath(inside);

    const auto margin = 15;
    bounds.adjust(-margin, -margin, margin, margin);
    setSceneRect(bounds);
}

void ImageEditor::setZoom(int zoom)
{
    if (mZoom == zoom)
        return;

    mZoom = zoom;
    auto scale = (mZoom < 0 ?
        1.0 / (1 << (-mZoom)) :
        1 << (mZoom));
    updateTransform(scale);

    if (mZeroCopyItem)
        mZeroCopyItem->setMagnifyLinear(scale <= 4);
}

void ImageEditor::updateTransform(double scale)
{
    auto transform = QTransform().scale(scale, scale);
    setTransform(transform);

    // update background checkers pattern
    QPixmap pm(2, 2);
    QPainter pmp(&pm);
    pmp.fillRect(0, 0, 1, 1, 0x999999);
    pmp.fillRect(1, 1, 1, 1, 0x999999);
    pmp.fillRect(0, 1, 1, 1, 0x888888);
    pmp.fillRect(1, 0, 1, 1, 0x888888);
    pmp.end();

    auto brush = QBrush(pm);
    brush.setTransform(transform.inverted().scale(8, 8));
    setBackgroundBrush(brush);
}
