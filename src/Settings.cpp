#include "Settings.h"
#include "Theme.h"
#include <QFontDialog>
#include <QFontDatabase>
#include <QIcon>

Settings::Settings(QObject *parent) : QSettings(parent)
{
    mFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    QIcon::setThemeName("light");

    beginGroup("General");
    setTabSize(value("tabSize", "2").toInt());
    setLineWrap(value("lineWrap", "false").toBool());
    setIndentWithSpaces(value("indentWithSpaces", "true").toBool());
    setShowWhiteSpace(value("showWhiteSpace", "false").toBool());
    setHideMenuBar(value("hideMenuBar", "false").toBool());
    setShaderPreamble(value("shaderPreamble", "").toString());
    setShaderIncludePaths(value("shaderIncludePaths", "").toString());
    setRenderer(value("renderer", "").toString());

    const auto fontSettings = value("font").toString();
    auto font = QFont();
    if (!fontSettings.isEmpty() &&
         font.fromString(fontSettings)) {
        setFont(font);
    }
    else {
        for (const auto family : {
            "Cascadia Mono",
            "Consolas",
            "Courier New",
        }) {
            font = QFont(family);
            if (font.exactMatch()) {
                font.setPointSize(10);
                setFont(font);
                break;
            }
        }
    }
}

Settings::~Settings()
{
    setValue("tabSize", tabSize());
    setValue("lineWrap", lineWrap());
    setValue("indentWithSpaces", indentWithSpaces());
    setValue("showWhiteSpace", showWhiteSpace());
    setValue("windowTheme", windowTheme().fileName());
    setValue("editorTheme", editorTheme().fileName());
    setValue("hideMenuBar", hideMenuBar());
    setValue("font", font().toString());
    setValue("shaderPreamble", shaderPreamble());
    setValue("shaderIncludePaths", shaderIncludePaths());
    setValue("renderer", renderer());
    endGroup();
}

void Settings::applyTheme()
{
    setWindowTheme(Theme::getTheme(value("windowTheme").toString()));
    setEditorTheme(Theme::getTheme(value("editorTheme").toString()));
}

void Settings::setTabSize(int tabSize)
{
    mTabSize = tabSize;
    Q_EMIT tabSizeChanged(tabSize);
}

void Settings::setFont(const QFont &font)
{
    mFont = font;
    Q_EMIT fontChanged(font);
}

void Settings::selectFont()
{
    auto prevFont = mFont;
    QFontDialog dialog{ mFont };
    dialog.setMinimumSize({ 700, 500 });
    dialog.setMinimumSize({ 500, 400 });
    connect(&dialog, &QFontDialog::currentFontChanged,
        this, &Settings::setFont);
    if (dialog.exec() == QDialog::Accepted)
        mFont = dialog.selectedFont();
    else
        mFont = prevFont;
    setFont(mFont);
}

void Settings::setLineWrap(bool enabled)
{
    mLineWrap = enabled;
    Q_EMIT lineWrapChanged(enabled);
}

void Settings::setIndentWithSpaces(bool enabled)
{
    mIndentWithSpaces = enabled;
    Q_EMIT indentWithSpacesChanged(enabled);
}

void Settings::setShowWhiteSpace(bool enabled)
{
    mShowWhiteSpace = enabled;
    Q_EMIT showWhiteSpaceChanged(enabled);
}

void Settings::setWindowTheme(const Theme &theme)
{
    mWindowTheme = &theme;
    QIcon::setThemeName(mWindowTheme->isDarkTheme() ? "dark" : "light");
    Q_EMIT windowThemeChanging(theme);
    Q_EMIT windowThemeChanged(theme);
}

void Settings::setEditorTheme(const Theme &theme)
{
    mEditorTheme = &theme;
    Q_EMIT editorThemeChanging(theme);
    Q_EMIT editorThemeChanged(theme);
}

void Settings::setHideMenuBar(bool hide)
{
    if (mHideMenuBar != hide) {
        mHideMenuBar = hide;
        Q_EMIT hideMenuBarChanged(hide);
    }
}

void Settings::setShaderPreamble(const QString &preamble)
{
    if (mShaderPreamble != preamble) {
        mShaderPreamble = preamble;
        Q_EMIT shaderPreambleChanged(preamble);
    }
}

void Settings::setShaderIncludePaths(const QString &includePaths)
{
    if (mShaderIncludePaths != includePaths) {
        mShaderIncludePaths = includePaths;
        Q_EMIT shaderIncludePathsChanged(includePaths);
    }
}

void Settings::setRenderer(const QString &renderer)
{
    if (mRenderer != renderer) {
      mRenderer = renderer;
      Q_EMIT rendererChanged(renderer);
    }
}
