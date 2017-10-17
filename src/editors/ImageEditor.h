#ifndef IMAGEEDITOR_H
#define IMAGEEDITOR_H

#include "IEditor.h"
#include <QGraphicsView>

class ImageEditor : public QGraphicsView, public IEditor
{
    Q_OBJECT
public:
    static bool load(const QString &fileName, QImage *image);

    explicit ImageEditor(QString fileName, QWidget *parent = nullptr);

    QList<QMetaObject::Connection>
        connectEditActions(const EditActions &actions) override;
    QString fileName() const override { return mFileName; }
    void setFileName(QString fileName) override;
    bool load() override;
    bool save() override;
    int tabifyGroup() override { return 1; }
    bool isModified() const { return mModified; }
    void replace(QImage image, bool emitDataChanged = true);
    const QImage &image() const { return mImage; }

signals:
    void dataChanged();
    void modificationChanged(bool modified);
    void fileNameChanged(const QString &fileName);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void refresh();
    void setBounds(QRect bounds);
    void setZoom(int zoom);
    void updateTransform(double scale);
    void setModified(bool modified);

    QString mFileName;
    bool mModified{ };
    QImage mImage;
    QGraphicsPixmapItem *mPixmapItem{ };
    QGraphicsPathItem *mOutside{ };
    bool mPan{ };
    QRect mBounds{ };
    int mZoom{ };
    int mPanStartX{ };
    int mPanStartY{ };
};

#endif // IMAGEEDITOR_H
