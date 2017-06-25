#include "Settings.h"
#include <QFontDatabase>

Settings::Settings(QObject *parent) : QObject(parent)
{
    mFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
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
