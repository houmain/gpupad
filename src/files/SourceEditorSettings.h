#ifndef SOURCEEDITORSETTINGS_H
#define SOURCEEDITORSETTINGS_H

#include <QObject>
#include <QFont>

class SourceEditorSettings : public QObject
{
    Q_OBJECT
public:
    explicit SourceEditorSettings(QObject *parent = 0);

    void setTabSize(int tabSize);
    int tabSize() const { return mTabSize; }
    void setFont(const QFont &font);
    const QFont &font() const { return mFont; }
    void setLineWrap(bool enabled);
    bool lineWrap() const { return mLineWrap; }
    void setIndentWithSpaces(bool enabled);
    bool indentWithSpaces() const { return mIndentWithSpaces; }
    void setAutoIndentation(bool enabled);
    bool autoIndentation() const { return mAutoIndentation; }

signals:
    void tabSizeChanged(int tabSize);
    void fontChanged(const QFont &font);
    void lineWrapChanged(bool wrap);
    void indentWithSpacesChanged(bool enabled);
    void autoIndentationChanged(bool enabled);

private:
    int mTabSize{ 4 };
    QFont mFont;
    bool mLineWrap{ };
    bool mIndentWithSpaces{ };
    bool mAutoIndentation{ true };
};

#endif // SOURCEEDITORSETTINGS_H
