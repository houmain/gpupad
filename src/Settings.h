#ifndef SETTINGS_H
#define SETTINGS_H

#include <QSettings>
#include <QFont>

class Settings : public QSettings
{
    Q_OBJECT
public:
    explicit Settings(QObject *parent = nullptr);
    ~Settings();

    void setTabSize(int tabSize);
    int tabSize() const { return mTabSize; }
    void selectFont();
    void setFont(const QFont &font);
    const QFont &font() const { return mFont; }
    void setLineWrap(bool enabled);
    bool lineWrap() const { return mLineWrap; }
    void setIndentWithSpaces(bool enabled);
    bool indentWithSpaces() const { return mIndentWithSpaces; }
    void setAutoIndentation(bool enabled);
    bool autoIndentation() const { return mAutoIndentation; }
    void setSyntaxHighlighting(bool enabled);
    bool syntaxHighlighting() const { return mSyntaxHighlighting; }
    void setSourceValidation(bool enabled);
    bool sourceValidation() const { return mSourceValidation; }

signals:
    void tabSizeChanged(int tabSize);
    void fontChanged(const QFont &font);
    void lineWrapChanged(bool wrap);
    void indentWithSpacesChanged(bool enabled);
    void autoIndentationChanged(bool enabled);
    void syntaxHighlightingChanged(bool enabled);
    void sourceValidationChanged(bool enabled);

private:
    int mTabSize{ 4 };
    QFont mFont;
    bool mLineWrap{ };
    bool mIndentWithSpaces{ };
    bool mAutoIndentation{ true };
    bool mSyntaxHighlighting{ true };
    bool mSourceValidation{ };
};

#endif // SETTINGS_H
