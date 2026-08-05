#pragma once

#include <QDialog>

class AboutDialog : public QDialog
{
public:
    AboutDialog(QWidget *parent);
};

extern const char *copyrightAuthor;
extern const char *copyrightRangeBegin;
extern const char *copyrightRangeEnd;
