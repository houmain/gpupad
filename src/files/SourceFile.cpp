#include "SourceFile.h"
#include <QTextDocument>
#include <QPlainTextDocumentLayout>
#include <QTextCodec>

SourceFile::SourceFile(QObject *parent)
    : QObject(parent)
    , mDocument(new QTextDocument(this))
{
    mDocument->setDocumentLayout(new QPlainTextDocumentLayout(mDocument));
}

SourceFile::~SourceFile()
{
    if (document().isModified())
        emit document().contentsChanged();
}

void SourceFile::setFileName(const QString &fileName)
{
    mFileName = fileName;
    emit fileNameChanged(fileName);
}

bool SourceFile::load()
{
    QFile file(fileName());
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        auto bytes = file.readAll();

        QTextCodec::ConverterState state;
        auto codec = QTextCodec::codecForUtfText(bytes,
            QTextCodec::codecForName("UTF-8"));
        auto text = codec->toUnicode(bytes.constData(), bytes.size(), &state);
        if (state.invalidChars == 0) {
            mDocument->setPlainText(text);
            mDocument->setModified(false);
            return true;
        }
    }
    return false;
}

bool SourceFile::save()
{
    QFile file(fileName());
    if (file.open(QFile::WriteOnly | QFile::Text)) {
        file.write(toPlainText().toUtf8());
        mDocument->setModified(false);
        return true;
    }
    return false;
}

QString SourceFile::toPlainText() const
{
    return mDocument->toPlainText();
}
