#include "ImageEditor.h"
#include "FileDialog.h"
#include <cmath>
#include <QAction>
#include <QScrollBar>
#include <QWheelEvent>
#include <QGraphicsPixmapItem>

ImageEditor::ImageEditor(QString fileName, QWidget *parent)
    : QGraphicsView(parent)
    , mFileName(fileName)
    , mImage(QSize(1, 1), QImage::Format_RGB888)
{
    mImage.fill(Qt::black);
    setTransformationAnchor(AnchorUnderMouse);

    setScene(new QGraphicsScene(this));

    auto pen = QPen();
    pen.setWidth(1);
    pen.setCosmetic(true);
    pen.setColor(QColor::fromRgbF(0.5, 0.5, 0.5));
    mOutside = new QGraphicsPathItem();
    mOutside->setPen(pen);
    mOutside->setZValue(1);
    mOutside->setBrush(QBrush(QColor::fromRgbF(0.6, 0.6, 0.6, 0.7)));
    scene()->addItem(mOutside);

    setZoom(mZoom);
    refresh();
}

void ImageEditor::refresh()
{
    delete mPixmapItem;
    auto pixmap = QPixmap::fromImage(mImage,
        Qt::NoOpaqueDetection | Qt::NoFormatConversion);
    mPixmapItem = new QGraphicsPixmapItem(pixmap);
    scene()->addItem(mPixmapItem);

    setBounds(pixmap.rect());
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

bool ImageEditor::load(const QString &fileName, QImage *image)
{
    if (FileDialog::isEmptyOrUntitled(fileName))
        return false;

    auto file = QImage();
    if (!file.load(fileName))
        return false;

    *image = file;
    return true;
}

bool ImageEditor::load()
{
    if (!load(mFileName, &mImage))
        return false;

    refresh();
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

void ImageEditor::replace(QImage image, bool emitDataChanged)
{
    if (image.constBits() == mImage.constBits())
        return;

    mImage = image;
    refresh();
    setModified(true);

    if (emitDataChanged)
        emit dataChanged();
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
    QGraphicsView::mouseMoveEvent(event);
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
    bounds.adjust(-1, -1, 0, 0);
    const auto max = 65536;
    auto outside = QPainterPath();
    outside.addRect(-max, -max, 2 * max, 2 * max);
    auto inside = QPainterPath();
    inside.addRect(bounds);
    outside = outside.subtracted(inside);
    mOutside->setPath(outside);

    const auto margin = 15;
    bounds.adjust(-margin, -margin, margin, margin);
    setSceneRect(bounds);
}

void ImageEditor::setZoom(int zoom)
{
    mZoom = zoom;
    auto scale = (mZoom < 0 ?
        1.0 / (1 << (-mZoom)) :
        1 << (mZoom));
    updateTransform(scale);
}

void ImageEditor::updateTransform(double scale)
{
    auto transform = QTransform().scale(scale, scale);
    setTransform(transform);

    // update background checkers pattern
    QPixmap pm(2, 2);
    QPainter pmp(&pm);
    pmp.fillRect(0, 0, 1, 1, Qt::lightGray);
    pmp.fillRect(1, 1, 1, 1, Qt::lightGray);
    pmp.fillRect(0, 1, 1, 1, Qt::gray);
    pmp.fillRect(1, 0, 1, 1, Qt::gray);
    pmp.end();

    auto brush = QBrush(pm);
    brush.setTransform(transform.inverted().scale(8, 8));
    setBackgroundBrush(brush);
}
