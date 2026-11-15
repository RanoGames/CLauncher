#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QProgressBar>

#include "../mc/ModBrowser.h"

class ModBrowserDialog : public QDialog {
    Q_OBJECT

public:
    explicit ModBrowserDialog(QWidget *parent = nullptr,
                              const QString &mcVersion = QString(),
                              const QString &gameDir = QString());
    ~ModBrowserDialog() override = default;

private slots:
    void onSearchClicked();
    void onSearchStarted();
    void onSearchFinished(const QList<ModSearchResult> &results, int totalHits);
    void onSearchError(const QString &errorMessage);

    void onInstallRequested(const QString &projectId, const QString &title, QPushButton *button);
    void onDownloadStarted(const QString &fileName);
    void onDownloadProgress(const QString &fileName, qint64 bytesReceived, qint64 bytesTotal, int percent);
    void onDownloadFinished(const QString &fileName, const QString &savedPath);
    void onDownloadError(const QString &fileName, const QString &errorMessage);
    void onStatusMessage(const QString &message);

private:
    void setupUi();
    QString cleanMinecraftVersion(const QString &ver) const;

    QString m_mcVersion;
    QString m_gameDir;
    ModBrowser m_browser;

    QLineEdit *m_searchInput{nullptr};
    QComboBox *m_loaderCombo{nullptr};
    QLineEdit *m_versionEdit{nullptr};
    QPushButton *m_searchButton{nullptr};

    QListWidget *m_listWidget{nullptr};
    QLabel *m_statusLabel{nullptr};
    QProgressBar *m_progressBar{nullptr};
    QPushButton *m_closeButton{nullptr};

    QMap<QString, QPushButton*> m_installingButtons; // projectId -> button
};
