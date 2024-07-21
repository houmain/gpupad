#pragma once

#include <QWidget>

namespace Ui {
    class TextureEditorToolBar;
}

class TextureEditorToolBar final : public QWidget
{
    Q_OBJECT
public:
    explicit TextureEditorToolBar(QWidget *parent);
    ~TextureEditorToolBar() override;

    void setZoom(int zoom);
    void setZoomToFit(bool fit);
    void setMaxLevel(int maxLevel);
    void setLevel(float level);
    void setMaxLayer(int maxLayer, int maxDepth);
    void setLayer(float layer);
    void setMaxSample(int maxSample);
    void setSample(int sample);
    void setMaxFace(int maxFace);
    void setFace(int face);
    void setCanFilter(bool canFilter);
    void setFilter(bool filter);
    void setCanFlipVertically(bool canFlip);
    void setFlipVertically(bool flip);

Q_SIGNALS:
    void zoomChanged(int zoom);
    void zoomToFitChanged(bool fit);
    void levelChanged(float level);
    void layerChanged(float layer);
    void faceChanged(int index);
    void sampleChanged(int index);
    void filterChanged(bool filter);
    void flipVerticallyChanged(bool flip);

private:
    void zoomIndexChanged(int index);
    void filterStateChanged(int state);

    Ui::TextureEditorToolBar *mUi;
};
