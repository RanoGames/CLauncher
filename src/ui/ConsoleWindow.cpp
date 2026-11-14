#include "ConsoleWindow.h"
#include <QIcon>
#include <QTextCursor>
#include <QFont>
#include <QScrollBar>

ConsoleWindow::ConsoleWindow(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void ConsoleWindow::setupUi()
{
    setWindowTitle(tr("Minecraft Debug Console / Консоль отладки"));
    resize(800, 500);
    setWindowIcon(QIcon(":/mclovin.ico"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setMaximumBlockCount(5000);

    QFont monoFont("Consolas");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(10);
    m_textEdit->setFont(monoFont);

    m_textEdit->setStyleSheet(
        "QPlainTextEdit {"
        "  background-color: #161821;"
        "  color: #d4d4d4;"
        "  border: 1px solid rgba(255, 255, 255, 0.1);"
        "  border-radius: 8px;"
        "  padding: 8px;"
        "}"
    );
    mainLayout->addWidget(m_textEdit);

    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(10);

    m_autoScrollCheck = new QCheckBox(tr("Автопрокрутка"), this);
    m_autoScrollCheck->setChecked(true);
    connect(m_autoScrollCheck, &QCheckBox::toggled, this, &ConsoleWindow::onAutoScrollToggled);
    bottomLayout->addWidget(m_autoScrollCheck);

    bottomLayout->addStretch();

    m_clearButton = new QPushButton(tr("🗑 Очистить"), this);
    m_clearButton->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(239, 68, 68, 0.18);"
        "  border: 1px solid rgba(239, 68, 68, 0.4);"
        "  border-radius: 8px;"
        "  color: #fca5a5;"
        "  font-weight: 600;"
        "  padding: 6px 14px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(239, 68, 68, 0.35);"
        "  color: #ffffff;"
        "}"
    );
    connect(m_clearButton, &QPushButton::clicked, this, &ConsoleWindow::clearLog);
    bottomLayout->addWidget(m_clearButton);

    m_hideButton = new QPushButton(tr("Скрыть"), this);
    m_hideButton->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(255, 255, 255, 0.08);"
        "  border: 1px solid rgba(255, 255, 255, 0.18);"
        "  border-radius: 8px;"
        "  color: #ffffff;"
        "  font-weight: 600;"
        "  padding: 6px 16px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(255, 255, 255, 0.16);"
        "}"
    );
    connect(m_hideButton, &QPushButton::clicked, this, &ConsoleWindow::hide);
    bottomLayout->addWidget(m_hideButton);

    mainLayout->addLayout(bottomLayout);
}

void ConsoleWindow::appendLog(const QString &text)
{
    m_textEdit->moveCursor(QTextCursor::End);
    m_textEdit->insertPlainText(text);
    if (!text.endsWith('\n')) {
        m_textEdit->insertPlainText("\n");
    }
    if (m_autoScroll) {
        m_textEdit->verticalScrollBar()->setValue(m_textEdit->verticalScrollBar()->maximum());
    }
}

void ConsoleWindow::clearLog()
{
    m_textEdit->clear();
}

void ConsoleWindow::onAutoScrollToggled(bool checked)
{
    m_autoScroll = checked;
    if (m_autoScroll) {
        m_textEdit->verticalScrollBar()->setValue(m_textEdit->verticalScrollBar()->maximum());
    }
}
