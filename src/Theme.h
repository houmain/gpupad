#pragma once

#include <QMap>
#include <QObject>
#include <QPalette>

enum class ThemeColor {
    Function,
    Keyword,
    BuiltinFunction,
    BuiltinConstant,
    Number,
    Quotation,
    Preprocessor,
    Comment,
    WhiteSpace,
};

class Theme
{
public:
    static const Theme &getTheme(const QString &fileName);
    static QStringList getThemeFileNames();

    const QString &fileName() const { return mFileName; }
    const QString &name() const { return mName; }
    const QString &author() const { return mAuthor; }
    bool isDarkTheme() const { return mIsDarkTheme; }
    const QPalette &palette() const { return mPalette; }
    QColor getColor(ThemeColor color) const;

private:
    explicit Theme(const QString &fileName);

    QString mFileName;
    QString mName;
    QString mAuthor;
    bool mIsDarkTheme{};
    QPalette mPalette;
    QMap<ThemeColor, QColor> mColors;
};

Q_DECLARE_METATYPE(const Theme *);
