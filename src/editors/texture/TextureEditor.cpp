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
    , mMargin(20)
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

    // automatically flip in editor when opening an image file
    if (mTexture.isNull() && 
        !FileDialog::isEmptyOrUntitled(mFileName) && 
        texture.dimensions() == 2 && !texture.isCubemap())
        setFlipVertically(true);

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
        const auto min = -3;
        const auto max = 8;
        auto delta = (event->angleDelta().y() > 0 ? 1 : -1);
        setZoom(std::max(min, std::min(mZoom + delta, max)));
        return;
    }

    event->setModifiers(event->modifiers() & ~(Qt::ShiftModifier | Qt::ControlModifier));
    QAbstractScrollArea::wheelEvent(event);
}

void TextureEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    setZoom(0);
    QAbstractScrollArea::mouseDoubleClickEvent(event);
}

void TextureEditor::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        mPan = true;
        const auto pos = getMousePosition(event);
        mPanStartX = pos.x();
        mPanStartY = pos.y();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    updateMousePosition(event);
    Singletons::inputState().setMouseButtonPressed(event->button());

    QAbstractScrollArea::mousePressEvent(event);
}

void TextureEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (mPan) {
        const auto pos = getMousePosition(event);
        horizontalScrollBar()->setValue(
            horizontalScrollBar()->value() - (pos.x() - mPanStartX));
        verticalScrollBar()->setValue(
            verticalScrollBar()->value() - (pos.y() - mPanStartY));
        mPanStartX = pos.x();
        mPanStartY = pos.y();
        return;
    }

    updateMousePosition(event);

    QAbstractScrollArea::mouseMoveEvent(event);
}

void TextureEditor::updateMousePosition(QMouseEvent *event)
{
    // TODO:
    auto pos = QPointF(getMousePosition(event) - QPoint(1, 1));
    //auto pos = mTextureItem->mapFromScene(
    //    mapToScene(event->pos() - QPoint(1, 1))) - mTextureItem->boundingRect().topLeft();

    if (!mTextureItem->flipVertically())
        pos.setY(mTextureItem->boundingRect().height() - pos.y());

    pos = QPointF(qRound(pos.x() - 0.5), qRound(pos.y() - 0.5));
    mTextureInfoBar.setMousePosition(pos);
    Singletons::inputState().setMousePosition(pos.toPoint());
    Singletons::inputState().setEditorSize({ mTexture.width(), mTexture.height() });
    const auto outsideItem = (pos.x() < 0 || pos.y() < 0 || 
        pos.x() >= mTexture.width() || pos.y() >= mTexture.height());

    pos = event->pos()- QPoint(1, 1);
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
    if (bounds == mBounds)
        return;
    mBounds = bounds;
    updateScrollBars();
}

void TextureEditor::setZoom(int zoom)
{
    if (mZoom == zoom)
        return;
    mZoom = zoom;
    updateScrollBars();
}

double TextureEditor::getZoomScale() const
{
    return (mZoom < 0 ?
        1.0 / (1 << (-mZoom)) :
        1 << (mZoom));
}

void TextureEditor::updateScrollBars() 
{
    const auto dpr = devicePixelRatio();
    const auto size = viewport()->size() * dpr;
    const auto scale = getZoomScale();
    auto bounds = mBounds;
    bounds.setWidth(bounds.width() * scale);
    bounds.setHeight(bounds.height() * scale);
    bounds.adjust(-mMargin, -mMargin, mMargin, mMargin);

    horizontalScrollBar()->setPageStep(size.width());
    verticalScrollBar()->setPageStep(size.height());
    const auto sx = std::max(bounds.width() - size.width(), 0);
    const auto sy = std::max(bounds.height() - size.height(), 0);
    horizontalScrollBar()->setRange(-sx, sx);
    verticalScrollBar()->setRange(-sy, sy);

    mGLWidget->update();
}

void TextureEditor::paintGL()
{
    const auto dpr = devicePixelRatio();
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
        QPointF(std::min(scrollOffsetX + 2 * mMargin, 0), 
                std::min(scrollOffsetY + 2 * mMargin, 0)) +
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