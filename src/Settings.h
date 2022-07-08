#ifndef SETTINGS_H
#define SETTINGS_H

#include <QSettings>
#include <QFont>

class Settings final : public QSettings
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
    void setShowWhiteSpace(bool enabled);
    bool showWhiteSpace() const { return mShowWhiteSpace; }
    void setDarkTheme(bool enabled);
    bool darkTheme() const { return mDarkTheme; }
    void setShaderPreamble(const QString &preamble);
    QString shaderPreamble() const { return mShaderPreamble; }
    void setShaderIncludePaths(const QString &includePaths);
    QString shaderIncludePaths() const { return mShaderIncludePaths; }

Q_SIGNALS:
    void tabSizeChanged(int tabSize);
    void fontChanged(const QFont &font);
    void lineWrapChanged(bool wrap);
    void indentWithSpacesChanged(bool enabled);
    void showWhiteSpaceChanged(bool enabled);
    void darkThemeChanging(bool enabled);
    void darkThemeChanged(bool enabled);
    void shaderPreambleChanged(const QString &preamble);
    void shaderIncludePathsChanged(const QString &includePaths);

private:
    int mTabSize{ 2 };
    QFont mFont;
    bool mLineWrap{ };
    bool mIndentWithSpaces{ true };
    bool mShowWhiteSpace{ };
    bool mDarkTheme{ };
    QString mShaderPreamble;
    QString mShaderIncludePaths;
};

#endif // SETTINGS_H
