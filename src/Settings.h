#pragma once

#include <QFont>
#include <QSettings>
class Theme;

class Settings final : public QSettings
{
    Q_OBJECT
public:
    explicit Settings(QObject *parent = nullptr);
    ~Settings();
    void applyTheme();

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
    void setWindowTheme(const Theme &theme);
    const Theme &windowTheme() const { return *mWindowTheme; }
    void setEditorTheme(const Theme &theme);
    const Theme &editorTheme() const { return *mEditorTheme; }
    void setHideMenuBar(bool hide);
    bool hideMenuBar() const { return mHideMenuBar; }

Q_SIGNALS:
    void tabSizeChanged(int tabSize);
    void fontChanged(const QFont &font);
    void lineWrapChanged(bool wrap);
    void indentWithSpacesChanged(bool enabled);
    void showWhiteSpaceChanged(bool enabled);
    void windowThemeChanging(const Theme &theme);
    void windowThemeChanged(const Theme &theme);
    void editorThemeChanging(const Theme &theme);
    void editorThemeChanged(const Theme &theme);
    void hideMenuBarChanged(bool hide);

private:
    int mTabSize{ 2 };
    QFont mFont;
    bool mLineWrap{};
    bool mIndentWithSpaces{ true };
    bool mShowWhiteSpace{};
    const Theme *mWindowTheme{};
    const Theme *mEditorTheme{};
    bool mHideMenuBar{};
};
