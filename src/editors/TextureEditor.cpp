#include "TextureEditor.h"
#include "FileDialog.h"
#include "Singletons.h"
#include "FileCache.h"
#include "SynchronizeLogic.h"
#include "Settings.h"
#include "render/GLContext.h"
#include "session/Item.h"
#include "TextureItem.h"
#include <QAction>
#include <QOpenGLWidget>
#include <QWheelEvent>
#include <QScrollBar>

extern bool gZeroCopyPreview;

Ui::TextureEditorToolBar* TextureEditor::createEditorToolBar(QWidget *container)
{
    auto toolBarWidgets = new Ui::TextureEditorToolBar();
    toolBarWidgets->setupUi(container);
    return toolBarWidgets;
}

TextureEditor::TextureEditor(QString fileName, 
      const Ui::TextureEditorToolBar *editorToolBar, 
      QWidget *parent)
    : QGraphicsView(parent)
    , mEditorToolBar(*editorToolBar)
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

    connect(&Singletons::settings(), &Settings::darkThemeChanged,
        this, &TextureEditor::updateBackground);
    updateBackground();
}

TextureEditor::~TextureEditor() 
{
    if (mTextureItem) {
        auto texture = mTextureItem->resetTexture();
        auto glWidget = qobject_cast<QOpenGLWidget*>(viewport());
        if (auto context = glWidget->context())
            if (auto surface = context->surface())
                if (context->makeCurrent(surface)) {
                    auto& gl = *context->functions();
                    gl.glDeleteTextures(1, &texture);
                    mTextureItem->releaseGL();
                    context->doneCurrent();
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

    updateEditorToolBar();

    c += connect(mEditorToolBar.level, 
        qOverload<double>(&QDoubleSpinBox::valueChanged),
        [&](double value) { mTextureItem->setLevel(value); });
    c += connect(mEditorToolBar.layer, 
        qOverload<double>(&QDoubleSpinBox::valueChanged),
        [&](double value) { mTextureItem->setLayer(value); });
    c += connect(mEditorToolBar.sample, 
        qOverload<int>(&QSpinBox::valueChanged),
        [&](int value) { mTextureItem->setSample(value); });
    c += connect(mEditorToolBar.face, 
        qOverload<int>(&QComboBox::currentIndexChanged),
        [&](int index) { mTextureItem->setFace(index); });
    c += connect(mEditorToolBar.filter, 
        &QCheckBox::stateChanged,
        [&](int state) { mTextureItem->setMagnifyLinear(state != 0); });

    return c;
}

void TextureEditor::updateEditorToolBar() 
{
    const auto maxLevel = std::max(mTexture.levels() - 1, 0);
    mEditorToolBar.level->setMaximum(maxLevel);
    mEditorToolBar.labelLevel->setVisible(maxLevel);
    mEditorToolBar.level->setVisible(maxLevel);
    mEditorToolBar.level->setValue(mTextureItem->level());

    const auto maxLayer = std::max(mTexture.layers() - 1, 0);
    mEditorToolBar.layer->setMaximum(maxLayer);
    mEditorToolBar.labelLayer->setVisible(maxLayer);
    mEditorToolBar.layer->setVisible(maxLayer);
    mEditorToolBar.layer->setValue(mTextureItem->layer());

    const auto maxSample = std::max(mTexture.samples() - 1, 0);
    mEditorToolBar.sample->setMinimum(-1);
    mEditorToolBar.sample->setMaximum(maxSample);
    mEditorToolBar.labelSample->setVisible(maxSample);
    mEditorToolBar.sample->setVisible(maxSample);
    mEditorToolBar.sample->setValue(mTextureItem->sample());

    const auto maxFace = std::max(mTexture.faces() - 1, 0);
    mEditorToolBar.labelFace->setVisible(maxFace);
    mEditorToolBar.face->setVisible(maxFace);
    mEditorToolBar.face->setCurrentIndex(mTextureItem->face());

    mEditorToolBar.filter->setVisible(!mTexture.isMultisample());
    mEditorToolBar.filter->setChecked(mTextureItem->magnifyLinear());
}

void TextureEditor::setFileName(QString fileName)
{
    mFileName = fileName;
    emit fileNameChanged(mFileName);
}

bool TextureEditor::load(const QString &fileName, TextureData *texture)
{
    if (!texture || FileDialog::isEmptyOrUntitled(fileName))
        return false;

    auto file = TextureData();
    if (!file.load(fileName)) {
        if (FileDialog::isVideoFileName(fileName)) {
            texture->create(QOpenGLTexture::Target2D,
                QOpenGLTexture::RGBA8_UNorm, 256, 256, 1, 1, 1);
            Singletons::fileCache().asyncOpenVideoPlayer(fileName);
            return true;
        }
        return false;
    }

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

    if (focusWidget() == this)
      updateEditorToolBar();
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
    setTransform(getZoomTransform());

    updateBackground();
}

QTransform TextureEditor::getZoomTransform() const
{
    const auto scale = (mZoom < 0 ?
        1.0 / (1 << (-mZoom)) :
        1 << (mZoom));
    return QTransform().scale(scale, scale);
}

void TextureEditor::updateBackground()
{
    const auto color1 = QPalette().window().color().darker(115);
    const auto color2 = QPalette().window().color().darker(110);

    QPixmap pm(2, 2);
    QPainter pmp(&pm);
    pmp.fillRect(0, 0, 1, 1, color1);
    pmp.fillRect(1, 1, 1, 1, color1);
    pmp.fillRect(0, 1, 1, 1, color2);
    pmp.fillRect(1, 0, 1, 1, color2);
    pmp.end();

    auto brush = QBrush(pm);
    brush.setTransform(getZoomTransform().inverted().translate(0, 1).scale(16, 16));
    setBackgroundBrush(brush);
}
