#ifndef SOURCEFILE_H
#define SOURCEFILE_H

#include <QObject>

class QTextDocument;

class SourceFile : public QObject
{
    Q_OBJECT
public:
    explicit SourceFile(QObject *parent = 0);
    ~SourceFile();

    void setFileName(const QString &fileName);
    const QString &fileName() const { return mFileName; }
    bool load();
    bool save();

    QTextDocument &document() { return *mDocument; }
    QString toPlainText() const;

signals:
    void fileNameChanged(const QString &fileName);

private:
    QString mFileName;
    QTextDocument *mDocument;
};

#endif // SOURCEFILE_H
