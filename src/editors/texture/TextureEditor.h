#pragma once

#include "TextureData.h"
#include "editors/IEditor.h"
#include "render/ShareHandle.h"
#include <QOpenGLTexture>
#include <QAbstractScrollArea>

class TextureEditorItem;
class TextureEditorBackground;
class TextureEditorToolBar;
class TextureInfoBar;
class GpuWindow;

class TextureEditor final : public QAbstractScrollArea, public IEditor
{
    Q_OBJECT
public:
    struct RawFormat
    {
        Texture::Target target;
        Texture::Format format;
        int width, height, depth, layers, samples;
    };

    TextureEditor(QString fileName, TextureEditorToolBar *editorToolbar,
        TextureInfoBar *textureInfoBar, QWidget *parent = nullptr);
    ~TextureEditor() override;

    QList<QMetaObject::Connection> connectEditActions(
        const EditActions &actions) override;
    QString fileName() const override { return mFileName; }
    void setFileName(QString fileName) override;
    void setRawFormat(RawFormat rawFormat);
    bool load() override;
    bool save() override;
    void copy();
    int tabifyGroup() const override;
    void setModified() override;
    bool isModified() const { return mModified; }
    bool isRaw() const { return mIsRaw; }
    void replace(TextureData texture, bool emitFileChanged = true);
    void setFlipVertically(bool flipVertically);
    void copySharedTexture(ShareHandle textureHandle, int samples = 1);
    void recreateGpuWindow();
    const TextureData &texture() const { return mTexture; }

Q_SIGNALS:
    void modificationChanged(bool modified);
    void fileNameChanged(const QString &fileName);
    void zoomChanged(int zoom);
    void zoomToFitChanged(bool fit);

protected:
    bool eventFilter(QObject *watched, QEvent *event);
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    bool createGpuWindow();
    void destroyGpuWindow();
    void setupGpuWindow();
    void paintGpu();
    void updateMousePosition(const QPoint &position);
    void setBounds(QRect bounds);
    void setZoomToFit(bool fit);
    void setZoom(int zoom);
    double getZoomScale() const;
    void setModified(bool modified);
    void updateEditorToolBar();
    void updateScrollBars();
    int margin() const;
    void zoomToFit();
    QPointF getScrollOffset() const;
    QPointF mapToScene(const QPointF &position) const;
    QPointF mapFromScene(const QPointF &position) const;

    GpuWindow *mGpuWindow{};
    QWidget *mGpuWindowContainer{};
    TextureEditorToolBar &mEditorToolBar;
    TextureInfoBar &mTextureInfoBar;
    QString mFileName;
    RawFormat mRawFormat{};
    bool mIsRaw{};
    bool mModified{};
    TextureData mTexture;
    int mTextureSamples{ 1 };
    bool mPan{};
    QRect mBounds{};
    bool mZoomToFit{};
    int mZoom{ 100 };
    QPoint mPanStart{};
    TextureEditorItem *mTextureItem{};
    TextureEditorBackground *mBackground{};
};
