#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <QMessageBox>
#include <QDir>
#include "SourceType.h"

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
    static bool isTextureFileName(const QString &fileName);
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
        SavingNon2DTexture  = 1 << 8,
        Directory           = 1 << 9,
        AllExtensionFilters = ShaderExtensions | TextureExtensions |
            BinaryExtensions | ScriptExtensions | SessionExtensions
    };
    Q_DECLARE_FLAGS(Options, OptionBit)

    explicit FileDialog(QMainWindow *window);
    ~FileDialog();

    QDir directory() const { return mDirectory; }
    void setDirectory(QDir directory);
    QString fileName() const;
    QStringList fileNames() const { return mFileNames; }
    bool asBinaryFile() const { return mAsBinaryFile; }
    bool exec(Options options, QString fileName = "", 
        SourceType sourceType = SourceType::PlainText);

Q_SIGNALS:
    void directoryChanged(QDir directory);

private:
    QMainWindow *mWindow;
    QStringList mFileNames;
    QDir mDirectory;
    bool mAsBinaryFile{ };
};

bool isNativeCanonicalFilePath(const QString &fileName);
QString toNativeCanonicalFilePath(const QString &fileName);
void showInFileManager(const QString &path);
int showNotSavedDialog(QWidget *parent, const QString &fileName);
bool showSavingFailedMessage(QWidget *parent, const QString &fileName);
void showCopyingSessionFailedMessage(QWidget *parent);
QDir getInstallDirectory(const QString &dirName);
QDir getUserDirectory(const QString &dirName);
QFileInfoList enumerateApplicationPaths(const QString &dirName,
    QDir::Filters filters);

#endif // FILEDIALOG_H
