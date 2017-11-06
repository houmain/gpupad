#include "Settings.h"
#include <QFontDialog>
#include <QFontDatabase>

Settings::Settings(QObject *parent) : QSettings(parent)
{
    mFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    beginGroup("General");
    setTabSize(value("tabSize", "4").toInt());
    setLineWrap(value("lineWrap", "false").toBool());
    setIndentWithSpaces(value("indentWithSpaces", "false").toBool());
    setAutoIndentation(value("autoIndentation", "true").toBool());
    setSyntaxHighlighting(value("syntaxHighlighting", "true").toBool());

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
    setValue("autoIndentation", autoIndentation());
    setValue("syntaxHighlighting", syntaxHighlighting());
    setValue("font", font().toString());
    endGroup();
}

void Settings::setTabSize(int tabSize)
{
    mTabSize = tabSize;
    emit tabSizeChanged(tabSize);
}

void Settings::setFont(const QFont &font)
{
    mFont = font;
    emit fontChanged(font);
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
    emit lineWrapChanged(enabled);
}

void Settings::setIndentWithSpaces(bool enabled)
{
    mIndentWithSpaces = enabled;
    emit indentWithSpacesChanged(enabled);
}

void Settings::setAutoIndentation(bool enabled)
{
    mAutoIndentation = enabled;
    emit autoIndentationChanged(enabled);
}

void Settings::setSyntaxHighlighting(bool enabled)
{
    mSyntaxHighlighting = enabled;
    emit syntaxHighlightingChanged(enabled);
}
