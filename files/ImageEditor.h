#ifndef IMAGEEDITOR_H
#define IMAGEEDITOR_H

#include "FileTypes.h"
#include <QGraphicsView>

class ImageEditor : public QGraphicsView
{
    Q_OBJECT
public:
    explicit ImageEditor(ImageFilePtr file, QWidget *parent = 0);

    QList<QMetaObject::Connection> connectEditActions(const EditActions &actions);
    const ImageFilePtr &file() const { return mFile; }

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void refresh();
    void setZoom(int zoom);
    void updateTransform(double scale);

    ImageFilePtr mFile;
    QGraphicsPathItem *mOutside{ };
    bool mPan{ };
    int mZoom{ };
    int mPanStartX{ };
    int mPanStartY{ };
};

#endif // IMAGEEDITOR_H
