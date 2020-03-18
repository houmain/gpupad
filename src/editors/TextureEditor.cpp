#include "TextureEditor.h"
#include "FileDialog.h"
#include "Singletons.h"
#include "FileCache.h"
#include "SynchronizeLogic.h"
#include "render/GLContext.h"
#include "session/Item.h"
#include "TextureItem.h"
#include <QAction>
#include <QOpenGLWidget>
#include <QWheelEvent>
#include <QScrollBar>

extern bool gZeroCopyPreview;

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
        mTextureItem = new TextureItem();
        scene()->addItem(mTextureItem);
    }

    setZoom(mZoom);
    updateTransform(1.0);
}

TextureEditor::~TextureEditor() 
{
    if (mTextureItem) {
        if (auto texture = mTextureItem->resetTexture()) {
            auto glWidget = qobject_cast<QOpenGLWidget*>(viewport());
            auto context = glWidget->context();
            auto surface = context->surface();
            if (context->makeCurrent(surface)) {
                auto& gl = *context->functions();
                gl.glDeleteTextures(1, &texture);
                mTextureItem->releaseGL();
                context->doneCurrent();
            }
        }
    }
    delete scene();

    if (isModified())
        Singletons::fileCache().invalidateEditorFile(mFileName);
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
    if (!Singletons::fileCache().getTexture(mFileName, &image))
        return false;

    replace(image);
    setModified(false);
    return true;
}

bool TextureEditor::reload()
{
    auto image = TextureData();
    if (!load(mFileName, &image))
        return false;

    replace(image);
    setModified(false);
    return true;
}

bool TextureEditor::save()
{
    if (!mTexture.save(fileName()))
        return false;

    setModified(false);
    return true;
}

void TextureEditor::replace(TextureData texture, bool invalidateFileCache)
{
    if (texture == mTexture)
        return;

    mTexture = texture;
    if (mTextureItem) {
        mTextureItem->setImage(mTexture);
        setBounds(mTextureItem->boundingRect().toRect());
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

    if (invalidateFileCache)
        Singletons::fileCache().invalidateEditorFile(mFileName);
}

void TextureEditor::updatePreviewTexture(
    QOpenGLTexture::Target target, GLuint textureId)
{
    if (mTextureItem)
        mTextureItem->setPreviewTexture(target, textureId);
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

    if (mTextureItem)
        mTextureItem->setMagnifyLinear(scale <= 4);
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
