#include "SourceEditorSettings.h"
#include <QFontDatabase>

SourceEditorSettings::SourceEditorSettings(QObject *parent) : QObject(parent)
{
    mFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

void SourceEditorSettings::setTabSize(int tabSize)
{
    mTabSize = tabSize;
    emit tabSizeChanged(tabSize);
}

void SourceEditorSettings::setFont(const QFont &font)
{
    mFont = font;
    emit fontChanged(font);
}

void SourceEditorSettings::setLineWrap(bool enabled)
{
    mLineWrap = enabled;
    emit lineWrapChanged(enabled);
}

void SourceEditorSettings::setIndentWithSpaces(bool enabled)
{
    mIndentWithSpaces = enabled;
    emit indentWithSpacesChanged(enabled);
}

void SourceEditorSettings::setAutoIndentation(bool enabled)
{
    mAutoIndentation = enabled;
    emit autoIndentationChanged(enabled);
}
