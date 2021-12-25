#ifndef TEXTUREEDITORTOOLBAR_H
#define TEXTUREEDITORTOOLBAR_H

#include <QWidget>

namespace Ui { class TextureEditorToolBar; }

class TextureEditorToolBar final : public QWidget
{
    Q_OBJECT
public:
    explicit TextureEditorToolBar(QWidget *parent);
    ~TextureEditorToolBar() override;

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
    void levelChanged(float level);
    void layerChanged(float layer);
    void faceChanged(int index);
    void sampleChanged(int index);
    void filterChanged(bool filter);
    void flipVerticallyChanged(bool flip);

private:
    void filterStateChanged(int state);

    Ui::TextureEditorToolBar *mUi;
};

#endif // TEXTUREEDITORTOOLBAR_H
