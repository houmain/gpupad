#include "ImageEditor.h"
#include "ImageFile.h"
#include <cmath>
#include <QAction>
#include <QScrollBar>
#include <QWheelEvent>
#include <QGraphicsPixmapItem>

ImageEditor::ImageEditor(ImageFilePtr file, QWidget *parent)
    : QGraphicsView(parent)
    , mFile(file)
{
    setTransformationAnchor(AnchorUnderMouse);

    connect(file.data(), &ImageFile::dataChanged, this, &ImageEditor::refresh);

    refresh();
}

void ImageEditor::refresh()
{
    setScene(new QGraphicsScene());
    auto pixmap = QPixmap::fromImage(mFile->image());
    auto item = new QGraphicsPixmapItem(pixmap);
    scene()->addItem(item);

    auto pen = QPen();
    pen.setWidth(1);
    pen.setCosmetic(true);
    pen.setColor(QColor::fromRgbF(0.5, 0.5, 0.5));
    mOutside = new QGraphicsPathItem();
    mOutside->setPen(pen);
    mOutside->setZValue(1);
    mOutside->setBrush(QBrush(QColor::fromRgbF(0.6, 0.6, 0.6, 0.7)));
    scene()->addItem(mOutside);

    auto bounds = pixmap.rect();

    const auto max = 65536;
    auto outside = QPainterPath();
    outside.addRect(-max, -max, 2 * max, 2 * max);
    auto inside = QPainterPath();
    inside.addRect(bounds);
    outside = outside.subtracted(inside);
    mOutside->setPath(outside);

    const auto margin = 5;
    bounds.adjust(-margin, -margin, margin, margin);
    setSceneRect(bounds);

    setZoom(mZoom);
}

QList<QMetaObject::Connection> ImageEditor::connectEditActions(
    const EditActions &actions)
{
    auto c = QList<QMetaObject::Connection>();

    actions.windowFileName->setText(mFile->fileName());
    actions.windowFileName->setEnabled(mFile->isModified());
    c += connect(mFile.data(), &ImageFile::fileNameChanged,
        actions.windowFileName, &QAction::setText);
    c += connect(mFile.data(), &ImageFile::modificationChanged,
        actions.windowFileName, &QAction::setEnabled);

    return c;
}

void ImageEditor::wheelEvent(QWheelEvent *event)
{
    if (!event->modifiers()) {
        const auto min = 0;
        const auto max = 3;
        auto delta = (event->delta() > 0 ? 1 : -1);
        setZoom(std::max(min, std::min(mZoom + delta, max)));
        return;
    }
    QGraphicsView::wheelEvent(event);
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

void ImageEditor::setZoom(int zoom) {
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
