#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <QObject>
#include <QDir>

class QMainWindow;

class FileDialog : public QObject
{
    Q_OBJECT
public:
    static void resetNextUntitledFileIndex();
    static QString generateNextUntitledFileName(QString base,
        bool startWithZero = false);
    static bool isEmptyOrUntitled(const QString &fileName);
    static bool isUntitled(const QString &fileName);
    static QString getFileTitle(const QString &fileName);
    static QString getWindowTitle(const QString &fileName);
    static QString getFullWindowTitle(const QString &fileName);
    static bool isSessionFileName(const QString &fileName);
    static bool isBinaryFileName(const QString &fileName);

    enum OptionBit
    {
        Loading             = 1 << 0,
        Saving              = 1 << 1,
        Importing           = 1 << 2,
        ShaderExtensions    = 1 << 3,
        ImageExtensions     = 1 << 4,
        BinaryExtensions    = 1 << 5,
        SessionExtensions   = 1 << 6,
        ScriptExtensions    = 1 << 7,
        SupportedExtensions = 1 << 8,
        AllExtensionFilters = ShaderExtensions | ImageExtensions |
            BinaryExtensions | ScriptExtensions | SessionExtensions | SupportedExtensions
    };
    Q_DECLARE_FLAGS(Options, OptionBit)

    explicit FileDialog(QMainWindow *window);
    ~FileDialog();

    QDir directory() const;
    void setDirectory(QDir directory);
    QString fileName() const;
    QStringList fileNames() const;
    bool exec(Options options, QString fileName = "");

private:
    QMainWindow *mWindow;
    QStringList mFileNames;
    QDir mDirectory;
};

#endif // FILEDIALOG_H
