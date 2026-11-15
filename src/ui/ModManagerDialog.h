#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QDragEnterEvent>
#include <QDropEvent>

#include "../mc/ModManager.h"

class ModManagerDialog : public QDialog {
    Q_OBJECT

public:
    explicit ModManagerDialog(QWidget *parent = nullptr, const QString &gameDir = QString());
    ~ModManagerDialog() override = default;

public slots:
    void refreshMods();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onSearchTextChanged(const QString &text);
    void onFilterComboChanged(int index);
    void onAddModClicked();
    void onOpenFolderClicked();
    void onEnableAllClicked();
    void onDisableAllClicked();
    void onModToggled(const QString &filePath, bool checked);
    void onDeleteModClicked(const QString &filePath, const QString &displayName);

private:
    void setupUi();
    void updateStats(int total, int active, int disabled);
    void applyFilter();

    ModManager m_modManager;
    QList<ModInfo> m_cachedMods;

    QLineEdit *m_searchInput{nullptr};
    QComboBox *m_filterCombo{nullptr};
    QPushButton *m_addModButton{nullptr};
    QPushButton *m_openFolderButton{nullptr};
    QPushButton *m_refreshButton{nullptr};

    QLabel *m_statsLabel{nullptr};
    QPushButton *m_enableAllButton{nullptr};
    QPushButton *m_disableAllButton{nullptr};

    QTableWidget *m_table{nullptr};
    QPushButton *m_closeButton{nullptr};
};
