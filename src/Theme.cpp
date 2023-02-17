
#include "Theme.h"
#include "FileDialog.h"
#include <QFile>
#include <QTextStream>
#include <QApplication>
#include <QRegularExpression>
#include <QStyle>
#include <QMap>

namespace 
{
    QMap<QString, Theme> gThemes;

    // base00 - Default Background
    // base01 - Lighter Background (Used for status bars, line number and folding marks)
    // base02 - Selection Background
    // base03 - Comments, Invisibles, Line Highlighting
    // base04 - Dark Foreground (Used for status bars)
    // base05 - Default Foreground, Caret, Delimiters, Operators
    // base06 - Light Foreground (Not often used)
    // base07 - Light Background (Not often used)
    // base08 - Variables, XML Tags, Markup Link Text, Markup Lists, Diff Deleted
    // base09 - Integers, Boolean, Constants, XML Attributes, Markup Link Url
    // base0A - Classes, Markup Bold, Search Text Background
    // base0B - Strings, Inherited Class, Markup Code, Diff Inserted
    // base0C - Support, Regular Expressions, Escape Characters, Markup Quotes
    // base0D - Functions, Methods, Attribute IDs, Headings
    // base0E - Keywords, Storage, Selector, Markup Italic, Diff Changed
    // base0F - Deprecated, Opening/Closing Embedded Language Tags, e.g. <?php ?>

    struct Base16Theme 
    {
        QString name;
        QString author;
        QList<QColor> colors;
    };

    Base16Theme loadBase16Theme(const QString &fileName)
    {
        auto file = QFile(fileName);
        if (!file.open(QFile::ReadOnly | QFile::Text))
            return { };
        const auto lines = QTextStream(&file).readAll().split("\n");
        if (lines.size() < 18)
            return { };
        auto theme = Base16Theme{ };
        static const auto extractString = QRegularExpression("\"([^\"]+)\"");
        theme.name = extractString.match(lines[0]).captured(1);
        theme.author = extractString.match(lines[1]).captured(1);
        const auto colors = lines.mid(2, 16);
        for (const auto &color : colors)
            theme.colors.append(QColor("#" + color.mid(9, 6)));
        return theme;
    }

    bool isDarkColor(const QColor &color) 
    {
        return (color.value() < 128);
    }

    bool isDarkPalette(const QPalette &palette)
    {
        return isDarkColor(palette.color(QPalette::Window));
    }

    QColor middle(const QColor &a, const QColor &b)
    {
        return QColor(
            (a.red() + b.red()) / 2,
            (a.green() + b.green()) / 2,
            (a.blue() + b.blue()) / 2);
    }

    QPalette getThemePalette(const Base16Theme &theme)
    {
        struct S { QPalette::ColorRole role; int baseIndex; };
        const auto roles = std::initializer_list<S>{
            { QPalette::WindowText,      0x05 },
            { QPalette::Button,          0x00 },
            { QPalette::Light,           0x00 }, // shadow of disabled menu entries
            { QPalette::Midlight,        0x00 }, // unused
            { QPalette::Dark,            0x00 }, // unused
            { QPalette::Mid,             0x00 }, // unused
            { QPalette::Text,            0x05 },
            { QPalette::BrightText,      0x0A },
            { QPalette::ButtonText,      0x05 },
            { QPalette::Base,            0x00 },
            { QPalette::Window,          0x00 },
            { QPalette::Shadow,          0x00 }, // unused
            { QPalette::Highlight,       0x02 },
            { QPalette::HighlightedText, 0x05 },
            { QPalette::Link,            0x0A }, // unused
            { QPalette::LinkVisited,     0x0A }, // unused
            { QPalette::AlternateBase,   0x00 },
            { QPalette::ToolTipBase,     0x01 },
            { QPalette::ToolTipText,     0x04 },
            { QPalette::PlaceholderText, 0x05 },
        };
        const auto dark = isDarkColor(theme.colors[0x00]);
        auto palette = QPalette();
        for (const auto [role, base] : roles) {
            auto color = theme.colors[base];
            switch (role) {
                case QPalette::AlternateBase:
                    color = color.lighter(dark ? 95 : 105);
                    break;
                default:
                    break;
            }
            palette.setColor(QPalette::Active, role, color);
            palette.setColor(QPalette::Inactive, role, color);
            palette.setColor(QPalette::Disabled, role,
                middle(color, theme.colors[0x00]));
        }
        return palette;
    }

    QMap<ThemeColor, QColor> getThemeColors(const Base16Theme &theme) 
    {
        const auto dark = isDarkColor(theme.colors[0]);
        auto colors = QMap<ThemeColor, QColor>();
        colors[ThemeColor::Function] = theme.colors[0x0D];
        colors[ThemeColor::Keyword] = theme.colors[0x0E];
        colors[ThemeColor::BuiltinFunction] = theme.colors[0x08];
        colors[ThemeColor::BuiltinConstant] = theme.colors[0x0A];
        colors[ThemeColor::Number] = theme.colors[0x09];
        colors[ThemeColor::Quotation] = theme.colors[0x0B];
        colors[ThemeColor::Preprocessor] = theme.colors[0x0C];
        colors[ThemeColor::Comment] = theme.colors[0x03];
        colors[ThemeColor::WhiteSpace] = theme.colors[0x05].lighter(dark ? 110 : 90);
        return colors;
    }

    QMap<ThemeColor, QColor> getDefaultThemeColors(bool darkTheme) 
    {
        auto colors = QMap<ThemeColor, QColor>();
        if (darkTheme) {
            colors[ThemeColor::Function] = QColor(0x7AAFFF);
            colors[ThemeColor::Keyword] = QColor(0x7AAFFF);
            colors[ThemeColor::BuiltinFunction] = QColor(0x7AAFFF);
            colors[ThemeColor::BuiltinConstant] = QColor(0xDD8D8D);
            colors[ThemeColor::Number] = QColor(0xB09D30);
            colors[ThemeColor::Quotation] = QColor(0xB09D30);
            colors[ThemeColor::Preprocessor] = QColor(0xC87FFF);
            colors[ThemeColor::Comment] = QColor(0x56C056);
            colors[ThemeColor::WhiteSpace] = QColor(0x666666);
        }
        else {
            colors[ThemeColor::Function] = QColor(0x000066);
            colors[ThemeColor::Keyword] = QColor(0x003C98);
            colors[ThemeColor::BuiltinFunction] = QColor(0x000066);
            colors[ThemeColor::BuiltinConstant] = QColor(0x981111);
            colors[ThemeColor::Number] = QColor(0x981111);
            colors[ThemeColor::Quotation] = QColor(0x981111);
            colors[ThemeColor::Preprocessor] = QColor(0x800080);
            colors[ThemeColor::Comment] = QColor(0x008700);
            colors[ThemeColor::WhiteSpace] = QColor(0xCCCCCC);
        }
        return colors;
    }
} // namespace

Theme::Theme(const QString &fileName)
    :  mFileName(fileName)
{
    if (mFileName.isEmpty()) {
        mPalette = qApp->style()->standardPalette();
        mIsDarkTheme = isDarkPalette(mPalette);
        mColors = getDefaultThemeColors(mIsDarkTheme);

        if (mIsDarkTheme) {
            mPalette.setColor(QPalette::ToolTipBase, QColor(0x333333));
            mPalette.setColor(QPalette::ToolTipText, QColor(0xCCCCCC));
        }
    }
    else {
        const auto theme = loadBase16Theme(fileName);
        if (!theme.name.isNull()) {
            mName = theme.name;
            mAuthor = theme.author;
            mPalette = getThemePalette(theme);
            mColors = getThemeColors(theme);
            mIsDarkTheme = isDarkPalette(mPalette);
        }
    }
}

QColor Theme::getColor(ThemeColor color) const
{
    return mColors[color];
}

const Theme &Theme::getTheme(const QString &fileName)
{
    auto it = gThemes.find(fileName);
    if (it == gThemes.end()) {
        auto theme = Theme(fileName);
        if (theme.mColors.isEmpty()) 
            return getTheme("");
        it = gThemes.insert(fileName, std::move(theme));
    }
    return it.value();
}

QStringList Theme::getThemeFileNames()
{
    getTheme("");
    const auto entries = enumerateApplicationPaths("themes", QDir::Dirs);
    for (const auto &entry : entries) {
        auto dir = QDir(entry.absoluteFilePath());
        dir.setNameFilters({ "*.yaml" });
        const auto files = dir.entryInfoList();
        for (const auto &file : files) {
            const auto fileName = file.absoluteFilePath();
            if (gThemes.contains(fileName))
                continue;
            auto theme = Theme(fileName);
            if (theme.mColors.isEmpty())
                continue;
            gThemes.insert(fileName, std::move(theme));
        }
    }
    return gThemes.keys();
}
