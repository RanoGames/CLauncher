#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

class ConsoleWindow : public QWidget {
    Q_OBJECT

public:
    explicit ConsoleWindow(QWidget *parent = nullptr);
    ~ConsoleWindow() override = default;

public slots:
    void appendLog(const QString &text);
    void clearLog();

private slots:
    void onAutoScrollToggled(bool checked);

private:
    void setupUi();

    QPlainTextEdit *m_textEdit{nullptr};
    QPushButton *m_clearButton{nullptr};
    QPushButton *m_hideButton{nullptr};
    QCheckBox *m_autoScrollCheck{nullptr};
    bool m_autoScroll{true};
};
