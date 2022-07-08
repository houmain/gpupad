#ifndef TEXTUREEDITOR_H
#define TEXTUREEDITOR_H

#include "IEditor.h"
#include "TextureData.h"
#include <QGraphicsView>
#include <QOpenGLTexture>

class TextureItem;
class TextureEditorToolBar;
class TextureInfoBar;

class TextureEditor final : public QGraphicsView, public IEditor
{
    Q_OBJECT
public:
    struct RawFormat {
        QOpenGLTexture::Target target;
        QOpenGLTexture::TextureFormat format;
        int width, height, depth, layers, samples;
    };

    TextureEditor(QString fileName, 
        TextureEditorToolBar* editorToolbar, 
        TextureInfoBar* textureInfoBar,
        QWidget *parent = nullptr);
    ~TextureEditor() override;

    QList<QMetaObject::Connection>
        connectEditActions(const EditActions &actions) override;
    QString fileName() const override { return mFileName; }
    void setFileName(QString fileName) override;
    void setRawFormat(RawFormat rawFormat);
    bool load() override;
    bool save() override;
    int tabifyGroup() override { return 1; }
    bool isModified() const { return mModified; }
    void replace(TextureData texture, bool emitFileChanged = true);
    void updatePreviewTexture(QOpenGLTexture::Target target,
        QOpenGLTexture::TextureFormat format, GLuint textureId);
    const TextureData &texture() const { return mTexture; }

Q_SIGNALS:
    void modificationChanged(bool modified);
    void fileNameChanged(const QString &fileName);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    void updateMousePosition(QMouseEvent *event);
    void setBounds(QRect bounds);
    void setZoom(int zoom);
    QTransform getZoomTransform() const;
    void updateBackground();
    void setModified(bool modified);
    void updateEditorToolBar();

    TextureEditorToolBar &mEditorToolBar;
    TextureInfoBar &mTextureInfoBar;
    QString mFileName;
    RawFormat mRawFormat{ };
    bool mIsRaw{ };
    bool mModified{ };
    TextureData mTexture;
    bool mPan{ };
    QRect mBounds{ };
    int mZoom{ };
    int mPanStartX{ };
    int mPanStartY{ };
    QGraphicsPathItem *mBorder{ };
    TextureItem *mTextureItem{ };
    QGraphicsPixmapItem *mPixmapItem{ };
};

#endif // TEXTUREEDITOR_H
