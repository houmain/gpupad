#include "Settings.h"
#include <QFontDialog>
#include <QFontDatabase>

Settings::Settings(QObject *parent) : QSettings(parent)
{
    mFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    beginGroup("General");
    setTabSize(value("tabSize", "2").toInt());
    setLineWrap(value("lineWrap", "false").toBool());
    setIndentWithSpaces(value("indentWithSpaces", "true").toBool());
    setShowWhiteSpace(value("showWhiteSpace", "false").toBool());
    setDarkTheme(value("darkTheme", "false").toBool());
    setShaderPreamble(value("shaderPreamble", "").toString());
    setShaderIncludePaths(value("shaderIncludePaths", "").toString());

    auto fontSettings = value("font").toString();
    if (!fontSettings.isEmpty()) {
        auto font = QFont();
        if (font.fromString(fontSettings))
            setFont(font);
    }
}

Settings::~Settings()
{
    setValue("tabSize", tabSize());
    setValue("lineWrap", lineWrap());
    setValue("indentWithSpaces", indentWithSpaces());
    setValue("showWhiteSpace", showWhiteSpace());
    setValue("darkTheme", darkTheme());
    setValue("font", font().toString());
    setValue("shaderPreamble", shaderPreamble());
    setValue("shaderIncludePaths", shaderIncludePaths());
    endGroup();
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

void Settings::setDarkTheme(bool enabled)
{
    mDarkTheme = enabled;
    Q_EMIT darkThemeChanging(enabled);
    Q_EMIT darkThemeChanged(enabled);
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
