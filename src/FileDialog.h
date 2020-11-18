#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <QObject>
#include <QDir>

class QMainWindow;

class FileDialog final : public QObject
{
    Q_OBJECT
public:
    static QString generateNextUntitledFileName(QString base);
    static bool isEmptyOrUntitled(const QString &fileName);
    static bool isUntitled(const QString &fileName);
    static QString getFileTitle(const QString &fileName);
    static QString getWindowTitle(const QString &fileName);
    static QString getFullWindowTitle(const QString &fileName);
    static bool isSessionFileName(const QString &fileName);
    static bool isVideoFileName(const QString &fileName);

    enum OptionBit
    {
        Saving              = 1 << 0,
        Importing           = 1 << 1,
        Multiselect         = 1 << 2,
        ShaderExtensions    = 1 << 3,
        TextureExtensions   = 1 << 4,
        BinaryExtensions    = 1 << 5,
        SessionExtensions   = 1 << 6,
        ScriptExtensions    = 1 << 7,
        SupportedExtensions = 1 << 8,
        SavingNon2DTexture  = 1 << 9,
        AllExtensionFilters = ShaderExtensions | TextureExtensions |
            BinaryExtensions | ScriptExtensions | SessionExtensions | SupportedExtensions
    };
    Q_DECLARE_FLAGS(Options, OptionBit)

    explicit FileDialog(QMainWindow *window);
    ~FileDialog();

    QDir directory() const { return mDirectory; }
    void setDirectory(QDir directory);
    QString fileName() const;
    QStringList fileNames() const { return mFileNames; }
    bool asBinaryFile() const { return mAsBinaryFile; }
    bool exec(Options options, QString fileName = "");

private:
    QMainWindow *mWindow;
    QStringList mFileNames;
    QDir mDirectory;
    bool mAsBinaryFile{ };
};

#endif // FILEDIALOG_H
