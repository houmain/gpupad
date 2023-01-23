#include "TextureEditor.h"
#include "TextureEditorToolBar.h"
#include "TextureInfoBar.h"
#include "TextureItem.h"
#include "TextureBackground.h"
#include "GLWidget.h"
#include "FileDialog.h"
#include "Singletons.h"
#include "FileCache.h"
#include "SynchronizeLogic.h"
#include "InputState.h"
#include "Settings.h"
#include "getMousePosition.h"
#include "render/GLContext.h"
#include "session/Item.h"
#include <QAction>
#include <QApplication>
#include <QWheelEvent>
#include <QMimeData>
#include <QClipboard>
#include <QScrollBar>
#include <QMatrix4x4>
#include <cstring>

bool createFromRaw(const QByteArray &binary,
    const TextureEditor::RawFormat &r, TextureData *texture)
{
    if (!texture->create(r.target, r.format, r.width,
            r.height, r.depth, r.layers, r.samples))
        return false;

    if (!binary.isEmpty()) {
        std::memcpy(texture->getWriteonlyData(0, 0, 0), binary.constData(),
            std::min(static_cast<size_t>(binary.size()),
                     static_cast<size_t>(texture->getLevelSize(0))));
    }
    else {
        texture->clear();
    }
    return true;
}

TextureEditor::TextureEditor(QString fileName,
      TextureEditorToolBar *editorToolBar,
      TextureInfoBar* textureInfoBar,
      QWidget *parent)
    : QAbstractScrollArea(parent)
    , mEditorToolBar(*editorToolBar)
    , mTextureInfoBar(*textureInfoBar)
    , mFileName(fileName)
{
    mGLWidget = new GLWidget(this);
    setViewport(mGLWidget);
    mTextureItem = new TextureItem(mGLWidget);
    mTextureBackground = new TextureBackground(mGLWidget);

    connect(mGLWidget, &GLWidget::releasingGL, 
        this, &TextureEditor::releaseGL);
    connect(mGLWidget, &GLWidget::paintingGL, 
        this, &TextureEditor::paintGL);

    setAcceptDrops(false);
    setMouseTracking(true);
}

TextureEditor::~TextureEditor() 
{
    delete mGLWidget;

    if (isModified())
        Singletons::fileCache().handleEditorFileChanged(mFileName);
}

void TextureEditor::resizeEvent(QResizeEvent * event)
{
    mGLWidget->resizeEvent(event);
    if (mZoomToFit)
        zoomToFit();
    updateScrollBars();
}

void TextureEditor::paintEvent(QPaintEvent *event)
{
    mGLWidget->paintEvent(event);
} 

void TextureEditor::releaseGL() 
{
    mTextureBackground->releaseGL();
    mTextureItem->releaseGL();
}

QList<QMetaObject::Connection> TextureEditor::connectEditActions(
        const EditActions &actions)
{
    actions.copy->setEnabled(true);
    actions.findReplace->setEnabled(true);
    actions.windowFileName->setText(fileName());
    actions.windowFileName->setEnabled(isModified());

    auto c = QList<QMetaObject::Connection>();
    c += connect(actions.copy, &QAction::triggered,
                 this, &TextureEditor::copy);
    c += connect(this, &TextureEditor::fileNameChanged,
                 actions.windowFileName, &QAction::setText);
    c += connect(this, &TextureEditor::modificationChanged,
                 actions.windowFileName, &QAction::setEnabled);

    updateEditorToolBar();

    c += connect(&mEditorToolBar, &TextureEditorToolBar::zoomToFitChanged,
        this, &TextureEditor::setZoomToFit);
    c += connect(&mEditorToolBar, &TextureEditorToolBar::zoomChanged,
        this, &TextureEditor::setZoom);
    c += connect(this, &TextureEditor::zoomToFitChanged,
        &mEditorToolBar, &TextureEditorToolBar::setZoomToFit);
    c += connect(this, &TextureEditor::zoomChanged,
        &mEditorToolBar, &TextureEditorToolBar::setZoom);

    c += connect(&mEditorToolBar, &TextureEditorToolBar::levelChanged,
        mTextureItem, &TextureItem::setLevel);
    c += connect(&mEditorToolBar, &TextureEditorToolBar::layerChanged,
        mTextureItem, &TextureItem::setLayer);
    c += connect(&mEditorToolBar, &TextureEditorToolBar::sampleChanged,
        mTextureItem, &TextureItem::setSample);
    c += connect(&mEditorToolBar, &TextureEditorToolBar::faceChanged,
        mTextureItem, &TextureItem::setFace);
    c += connect(&mEditorToolBar, &TextureEditorToolBar::filterChanged,
        mTextureItem, &TextureItem::setMagnifyLinear);
    c += connect(&mEditorToolBar, &TextureEditorToolBar::flipVerticallyChanged,
        mTextureItem, &TextureItem::setFlipVertically);
    c += connect(mTextureItem, &TextureItem::pickerColorChanged,
        &mTextureInfoBar, &TextureInfoBar::setPickerColor);
    c += connect(mTextureItem, &TextureItem::histogramChanged,
        &mTextureInfoBar, &TextureInfoBar::updateHistogram);
    c += connect(&mTextureInfoBar, &TextureInfoBar::pickerEnabledChanged,
        mTextureItem, &TextureItem::setHistogramEnabled);
    c += connect(&mTextureInfoBar, &TextureInfoBar::mappingRangeChanged,
        mTextureItem, &TextureItem::setMappingRange);
    c += connect(&mTextureInfoBar, &TextureInfoBar::histogramBinCountChanged,
        mTextureItem, &TextureItem::setHistogramBinCount);
    c += connect(&mTextureInfoBar, &TextureInfoBar::histogramBoundsChanged,
        mTextureItem, &TextureItem::setHistogramBounds);
    c += connect(&mTextureInfoBar, &TextureInfoBar::autoRangeRequested,
        mTextureItem, &TextureItem::computeHistogramBounds);
    c += connect(mTextureItem, &TextureItem::histogramBoundsComputed,
        &mTextureInfoBar, &TextureInfoBar::setHistogramBounds);
    c += connect(mTextureItem, &TextureItem::histogramBoundsComputed,
        &mTextureInfoBar, &TextureInfoBar::setMappingRange);
    c += connect(&mTextureInfoBar, &TextureInfoBar::colorMaskChanged,
        mTextureItem, &TextureItem::setColorMask);

    mTextureItem->setHistogramEnabled(mTextureInfoBar.isPickerEnabled());
    mTextureItem->setHistogramBinCount(mTextureInfoBar.histogramBinCount());
    mTextureInfoBar.setHistogramBounds(mTextureItem->histogramBounds());
    mTextureInfoBar.setMappingRange(mTextureItem->mappingRange());
    mTextureInfoBar.setColorMask(mTextureItem->colorMask());
    return c;
}

void TextureEditor::updateEditorToolBar() 
{
    mEditorToolBar.setZoom(mZoom);
    mEditorToolBar.setZoomToFit(mZoomToFit);

    mEditorToolBar.setMaxLevel(std::max(mTexture.levels() - 1, 0));
    mEditorToolBar.setLevel(mTextureItem->level());

    mEditorToolBar.setMaxLayer(mTexture.layers(), mTexture.depth());
    mEditorToolBar.setLayer(mTextureItem->layer());

    // disabled for now, since all samples are identical after download
    mEditorToolBar.setMaxSample(0); // std::max(mTexture.samples() - 1, 0)
    mEditorToolBar.setSample(mTextureItem->sample());

    mEditorToolBar.setMaxFace(std::max(mTexture.faces() - 1, 0));
    mEditorToolBar.setFace(mTextureItem->face());

    mEditorToolBar.setCanFilter(!mTexture.isMultisample());
    mEditorToolBar.setFilter(mTextureItem->magnifyLinear());

    mEditorToolBar.setCanFlipVertically(
      mTexture.dimensions() == 2 || mTexture.isCubemap());
    mEditorToolBar.setFlipVertically(mTextureItem->flipVertically());
}

void TextureEditor::setFileName(QString fileName)
{
    if (mFileName != fileName) {
        mFileName = fileName;
        Q_EMIT fileNameChanged(mFileName);
    }
}

void TextureEditor::setRawFormat(RawFormat rawFormat)
{
    if (!std::memcmp(&mRawFormat, &rawFormat, sizeof(RawFormat)))
        return;

    mRawFormat = rawFormat;
    if (mTexture.isNull() || mIsRaw)
        load();
}

bool TextureEditor::load()
{
    auto texture = TextureData();
    auto isRaw = false;
    if (!Singletons::fileCache().getTexture(mFileName, false, &texture)) {
        auto binary = QByteArray();
        if (!Singletons::fileCache().getBinary(mFileName, &binary))
            if (!FileDialog::isEmptyOrUntitled(mFileName))
                return false;
        if (!createFromRaw(binary, mRawFormat, &texture))
            return false;
        isRaw = true;
    }

    if (mTexture.isNull()) {
        // automatically flip in editor when opening an image file
        if (!FileDialog::isEmptyOrUntitled(mFileName) && 
             texture.dimensions() == 2 && !texture.isCubemap()) {
            setFlipVertically(true);

            // automatically fit big images in window and enable filtering
            const auto big = 1024;
            if (texture.width() > big || texture.height() > big) {
                setZoomToFit(true);
                mTextureItem->setMagnifyLinear(true);
            }
        }
        else {
            setZoomToFit(true);
        }        
    }

    replace(texture);
    setModified(false);
    mIsRaw = isRaw;
    return true;
}

void TextureEditor::setFlipVertically(bool flipVertically)
{
    mTextureItem->setFlipVertically(flipVertically);
}

int TextureEditor::tabifyGroup() 
{
    // open untitled textures next to other editors
    return (FileDialog::isEmptyOrUntitled(mFileName) ? 1 : 0);
}

bool TextureEditor::save()
{
    if (!mTexture.save(fileName(), !mTextureItem->flipVertically()))
        return false;

    setModified(false);
    return true;
}

void TextureEditor::replace(TextureData texture, bool emitFileChanged)
{
    if (texture == mTexture)
        return;

    mTextureItem->setImage(texture);
    setBounds(mTextureItem->boundingRect().toRect());
    if (mTexture.isNull()) {
        horizontalScrollBar()->setSliderPosition(horizontalScrollBar()->minimum());
        verticalScrollBar()->setSliderPosition(verticalScrollBar()->minimum());
    }
    mTexture = texture;
    mIsRaw = false;

    if (emitFileChanged || !FileDialog::isEmptyOrUntitled(mFileName))
        setModified(true);

    Singletons::fileCache().handleEditorFileChanged(mFileName, emitFileChanged);

    if (qApp->focusWidget() == this)
      updateEditorToolBar();
}

void TextureEditor::copy()
{
    auto image = mTextureItem->image().toImage();
    if (!image.isNull()) {
        auto *data = new QMimeData();
        data->setImageData(image);
        QApplication::clipboard()->setMimeData(data);
    }
}

void TextureEditor::updatePreviewTexture(QOpenGLTexture::Target target,
    QOpenGLTexture::TextureFormat format, GLuint textureId)
{
    mTextureItem->setPreviewTexture(target, format, textureId);
}

void TextureEditor::setModified(bool modified)
{
    if (mModified != modified) {
        mModified = modified;
        Q_EMIT modificationChanged(modified);
    }
}

void TextureEditor::wheelEvent(QWheelEvent *event)
{
    setFocus();

    if (!event->modifiers()) {
        const auto scenePosition = mapToScene(event->position());

        auto delta = 0;
        auto steps = event->angleDelta().y() / 120.0;
        while (steps) {
            const auto scale = std::min(steps, 1.0);
            steps -= scale;

            if (mZoom < 10 + (scale > 0 ? 0 : 1))
                delta += (scale > 0 ? 1 : -1);
            else if (mZoom < 100 + (scale > 0 ? 0 : 50))
                delta += static_cast<int>(scale * 5);
            else
                delta += static_cast<int>(scale * 50);
        }
        setZoom(mZoom + delta);

        // scroll to restore mouse cursor's scene position
        const auto dpr = devicePixelRatioF();
        const auto offset = (event->position() - mapFromScene(scenePosition)) * dpr * 2;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - offset.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - offset.y());
    }
    else {
        event->setModifiers(event->modifiers() & ~(Qt::ShiftModifier | Qt::ControlModifier));
        QAbstractScrollArea::wheelEvent(event);
    }
    updateMousePosition(event->position().toPoint());
}

void TextureEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    setZoom(100);
    QAbstractScrollArea::mouseDoubleClickEvent(event);
}

void TextureEditor::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        const auto dpr = devicePixelRatioF();
        const auto pos = getMousePosition(event) * dpr * 2;
        mPan = true;
        mPanStart = pos;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    QAbstractScrollArea::mousePressEvent(event);
    updateMousePosition(getMousePosition(event));
    Singletons::inputState().setMouseButtonPressed(event->button());
}

void TextureEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (mPan) {
        const auto dpr = devicePixelRatioF();
        const auto pos = getMousePosition(event) * dpr * 2;
        horizontalScrollBar()->setValue(
            horizontalScrollBar()->value() - pos.x() + mPanStart.x());
        verticalScrollBar()->setValue(
            verticalScrollBar()->value() - pos.y() + mPanStart.y());
        mPanStart = pos;
        return;
    }
    QAbstractScrollArea::mouseMoveEvent(event);
    updateMousePosition(getMousePosition(event));
}

QPointF TextureEditor::getScrollOffset() const
{
    const auto dpr = devicePixelRatioF();
    const auto scale = getZoomScale();
    const auto width = viewport()->width() * dpr;
    const auto height = viewport()->height() * dpr;
    const auto bounds = scale * QSizeF(mBounds.size());
    const auto scrollX = horizontalScrollBar()->value();
    const auto scrollY = verticalScrollBar()->value();
    const auto scrollOffsetX = horizontalScrollBar()->minimum();
    const auto scrollOffsetY = verticalScrollBar()->minimum();
    const auto offset =
        QPointF(std::max(width - bounds.width(), 0.0),
                std::max(height - bounds.height(), 0.0)) +
        QPointF(std::min(scrollOffsetX + 2 * margin(), 0),
                std::min(scrollOffsetY + 2 * margin(), 0)) +
        QPointF(-scrollX, -scrollY);
    return offset / 2;
}

QPointF TextureEditor::mapToScene(const QPointF &position) const
{
    const auto dpr = devicePixelRatioF();
    const auto offset = getScrollOffset();
    const auto scale = getZoomScale();
    return (position * dpr - offset) / scale;
}

QPointF TextureEditor::mapFromScene(const QPointF &position) const
{
    const auto dpr = devicePixelRatioF();
    const auto offset = getScrollOffset();
    const auto scale = getZoomScale();
    return (position * scale + offset) / dpr;
}

void TextureEditor::updateMousePosition(const QPoint &position)
{
    auto pos = mapToScene(position);
    if (!mTextureItem->flipVertically())
        pos.setY(mTextureItem->boundingRect().height() - pos.y());
    pos = QPointF(qRound(pos.x() - 0.5), qRound(pos.y() - 0.5));

    mTextureInfoBar.setMousePosition(pos);
    Singletons::inputState().setMousePosition(pos.toPoint());
    Singletons::inputState().setEditorSize({ mTexture.width(), mTexture.height() });
    const auto outsideItem = (pos.x() < 0 || pos.y() < 0 || 
        pos.x() >= mTexture.width() || pos.y() >= mTexture.height());

    pos = position - QPoint(1, 1);
    pos.setY(viewport()->height() - pos.y());
    mTextureItem->setMousePosition(pos);
    mTextureItem->setPickerEnabled(
        mTextureInfoBar.isPickerEnabled() && !outsideItem);
}

void TextureEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        mPan = false;
        setCursor(Qt::ArrowCursor);
        return;
    }
    Singletons::inputState().setMouseButtonReleased(event->button());
    QAbstractScrollArea::mouseReleaseEvent(event);
}

void TextureEditor::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        Q_EMIT mTextureInfoBar.cancelled();

    Singletons::inputState().setKeyPressed(event->key(), event->isAutoRepeat());

    QAbstractScrollArea::keyPressEvent(event);
}

void TextureEditor::keyReleaseEvent(QKeyEvent *event) 
{
    if (!event->isAutoRepeat())
        Singletons::inputState().setKeyReleased(event->key());

    QAbstractScrollArea::keyReleaseEvent(event);
}

void TextureEditor::setBounds(QRect bounds)
{
    if (bounds != mBounds) {
        mBounds = bounds;
        if (mZoomToFit)
            zoomToFit();
        updateScrollBars();
    }
}

int TextureEditor::margin() const
{
    return (mZoomToFit ? 0 : 20);
}

void TextureEditor::zoomToFit()
{
    const auto dpr = devicePixelRatioF();
    const auto width = viewport()->width() * dpr;
    const auto height = viewport()->height() * dpr;
    const auto scale = std::min(width / mBounds.width(),
                                height / mBounds.height());
    mZoom = std::max(static_cast<int>(scale * 100), 1);
    Q_EMIT zoomChanged(mZoom);
}

void TextureEditor::setZoomToFit(bool fit)
{
    if (mZoomToFit != fit) {
        mZoomToFit = fit;
        Q_EMIT zoomToFitChanged(fit);
        if (fit) {
            zoomToFit();
            updateScrollBars();
        }
    }
}

void TextureEditor::setZoom(int zoom)
{
    zoom = std::max(zoom, 1);
    if (mZoom != zoom) {
        mZoom = zoom;
        Q_EMIT zoomChanged(zoom);
        setZoomToFit(false);
        updateScrollBars();
    }
}

double TextureEditor::getZoomScale() const
{
    return mZoom / 100.0;
}

void TextureEditor::updateScrollBars() 
{
    const auto dpr = devicePixelRatioF();
    const auto scale = getZoomScale();
    const auto width = viewport()->width() * dpr;
    const auto height = viewport()->height() * dpr;
    auto bounds = mBounds;
    bounds.setWidth(bounds.width() * scale);
    bounds.setHeight(bounds.height() * scale);
    const auto m = margin();
    bounds.adjust(-m, -m, m, m);

    horizontalScrollBar()->setPageStep(width);
    verticalScrollBar()->setPageStep(height);
    horizontalScrollBar()->setSingleStep(32);
    verticalScrollBar()->setSingleStep(32);
    const auto sx = std::max(bounds.width() - width, 0.0);
    const auto sy = std::max(bounds.height() - height, 0.0);
    horizontalScrollBar()->setRange(-sx, sx);
    verticalScrollBar()->setRange(-sy, sy);

    mGLWidget->update();
}

void TextureEditor::paintGL()
{
    const auto dpr = devicePixelRatioF();
    const auto scale = getZoomScale();
    const auto width = viewport()->width() * dpr;
    const auto height = viewport()->height() * dpr;
    const auto bounds = scale * QSizeF(mBounds.size());
    const auto scrollX = horizontalScrollBar()->value();
    const auto scrollY = verticalScrollBar()->value();
    const auto scrollOffsetX = horizontalScrollBar()->minimum();
    const auto scrollOffsetY = verticalScrollBar()->minimum();
    const auto offset =
        QPointF(std::max(width - bounds.width(), 0.0),
                std::max(height - bounds.height(), 0.0)) +
        QPointF(std::min(scrollOffsetX + 2 * margin(), 0),
                std::min(scrollOffsetY + 2 * margin(), 0)) +
        QPointF(-scrollX, scrollY);
    mTextureBackground->paintGL(bounds, offset / 2);

    const auto sx = bounds.width() / width;
    const auto sy = -bounds.height() / height;
    const auto x  = -scrollX / width;
    const auto y  = scrollY / height;
    mTextureItem->paintGL(QTransform(sx, 0, 0,
                                      0, sy, 0,
                                      x,  y, 1));
}
