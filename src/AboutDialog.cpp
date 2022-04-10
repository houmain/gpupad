#include "AboutDialog.h"
#include <QApplication>
#include <QTabWidget>
#include <QTextBrowser>
#include <QDesktopServices>
#include <QLabel>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QPushButton>
#include <QTextStream>
#include <QFile>
#include <QRegularExpression>

namespace 
{
    QString loadTextFile(const QString &fileName)
    {
        const auto paths = std::initializer_list<QString>{
            QCoreApplication::applicationDirPath(),
#if !defined(NDEBUG)
            QCoreApplication::applicationDirPath() + "/../..",
#endif
#if defined(__linux__)
            qEnvironmentVariable("APPDIR") + "/usr/share/gpupad",
#endif
        };
        for (const auto &path : paths) {
            auto file = QFile(path + "/" + fileName);
            if (file.open(QFile::ReadOnly | QFile::Text))
                return QTextStream(&file).readAll();
        }
        return { };
    }

    QString tweakChangeLog(QString text)
    {
        static const auto header = QRegularExpression(R"((^.*)(## \[Version))", 
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::InvertedGreedinessOption);
        static const auto h2link = QRegularExpression(R"(^##\s*\[([^\]]+)\]\s*\-(.+))",
            QRegularExpression::MultilineOption);
        return text.replace(header, "\\2")
                   .replace(h2link, "-------\n"
                                    "<h2>\\1</h2>\n\n"
                                    "<b>Release date: </b>\\2\n\n"
                                    "<b>All commits: </b>[\\1]")
                   .mid(7);
    }
} // namespace

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    const auto title = tr("About %1").arg(QApplication::applicationName());
    const auto header = QStringLiteral(
       "<h3>%1 %2</h3>"
       "Copyright &copy; %3-%4<br>"
       "Albert Kalchmair<br>"
       "%5<br><br>"
       "<a href='%6'>%6</a><br>")
       .arg(QApplication::applicationName(),
            QApplication::applicationVersion(),
            "2016", 
            QStringLiteral(__DATE__).mid(7),
            tr("All Rights Reserved."),
         "https://github.com/houmain/gpupad");

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setMinimumSize(400, 400);
    resize(600, 500);
    setWindowTitle(title);
    setSizeGripEnabled(true);

    auto icon = new QLabel(this);
    icon->setPixmap(parent->windowIcon().pixmap(128, 128));
    icon->setMargin(15);
    icon->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    
    auto tabs = new QTabWidget(this);
    const auto addTab = [&](QString title, QString text, QString defaultText = "") {
        if (text.isEmpty())
            text = defaultText;
        if (text.isEmpty())
            return;
        auto textbox = new QTextBrowser(this);
        textbox->setFrameShape(QTextEdit::NoFrame);
        textbox->setMarkdown(text);
        textbox->setOpenLinks(false);
        textbox->document()->setDocumentMargin(10);
        connect(textbox, &QTextBrowser::anchorClicked, 
            &QDesktopServices::openUrl);
        tabs->addTab(textbox, title);
    };
    addTab(tr("Change Log"), tweakChangeLog(loadTextFile("CHANGELOG.md")));
    addTab(tr("License"), loadTextFile("LICENSE"),
        tr("This program comes with absolutely no warranty.\n"
           "See the GNU General Public License, version 3 for details."));
    addTab(tr("Third-Party Libraries"), loadTextFile("THIRD-PARTY.md"));

    auto buttons = new QDialogButtonBox(this);
    buttons->setStandardButtons(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    buttons->button(QDialogButtonBox::Close)->setDefault(true);
    auto aboutQt = buttons->addButton(tr("About Qt"), QDialogButtonBox::ActionRole);
    connect(aboutQt, &QPushButton::clicked, this, QApplication::aboutQt);

    auto layout = new QGridLayout(this);
    layout->addWidget(icon, 0, 0);
    layout->addWidget(new QLabel(header), 0, 1);
    layout->addWidget(tabs, 1, 0, 1, 2);
    layout->addWidget(buttons, 2, 0, 1, 2);
}
