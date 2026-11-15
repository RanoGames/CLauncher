#include "ConsoleWindow.h"
#include <QIcon>
#include <QTextCursor>
#include <QFont>
#include <QScrollBar>
#include <QGuiApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDir>
#include <QTextCharFormat>
#include <QColor>

ConsoleWindow::ConsoleWindow(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void ConsoleWindow::setupUi()
{
    setWindowTitle(tr("Minecraft Debug Console / Консоль отладки"));
    resize(820, 520);
    setWindowIcon(QIcon(":/mclovin.ico"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setMaximumBlockCount(10000);

    QFont monoFont("Consolas");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(10);
    m_textEdit->setFont(monoFont);

    m_textEdit->setStyleSheet(
        "QPlainTextEdit {"
        "  background-color: #12141c;"
        "  color: #cbd5e1;"
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

    // Copy to clipboard button
    m_copyButton = new QPushButton(tr("📋 Копировать"), this);
    m_copyButton->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(59, 130, 246, 0.18);"
        "  border: 1px solid rgba(59, 130, 246, 0.4);"
        "  border-radius: 8px;"
        "  color: #93c5fd;"
        "  font-weight: 600;"
        "  padding: 6px 14px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(59, 130, 246, 0.35);"
        "  color: #ffffff;"
        "}"
    );
    connect(m_copyButton, &QPushButton::clicked, this, &ConsoleWindow::copyLog);
    bottomLayout->addWidget(m_copyButton);

    // Export to file button
    m_exportButton = new QPushButton(tr("💾 Экспорт"), this);
    m_exportButton->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(16, 185, 129, 0.18);"
        "  border: 1px solid rgba(16, 185, 129, 0.4);"
        "  border-radius: 8px;"
        "  color: #6ee7b7;"
        "  font-weight: 600;"
        "  padding: 6px 14px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(16, 185, 129, 0.35);"
        "  color: #ffffff;"
        "}"
    );
    connect(m_exportButton, &QPushButton::clicked, this, &ConsoleWindow::exportLog);
    bottomLayout->addWidget(m_exportButton);

    // Clear button
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

    // Hide button
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
    if (text.isEmpty()) return;

    const QStringList lines = text.split(QLatin1Char('\n'));
    QTextCursor cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];
        if (i == lines.size() - 1 && line.isEmpty() && !text.endsWith(QLatin1Char('\n'))) {
            break;
        }

        QTextCharFormat format;
        // Default text color
        format.setForeground(QColor(QStringLiteral("#cbd5e1")));

        if (line.contains(QStringLiteral("[ERROR]"), Qt::CaseInsensitive) ||
            line.contains(QStringLiteral("[FATAL]"), Qt::CaseInsensitive) ||
            line.contains(QStringLiteral(" Exception:"), Qt::CaseInsensitive) ||
            line.contains(QStringLiteral("Error:"), Qt::CaseInsensitive) ||
            line.startsWith(QStringLiteral("Caused by:"))) {
            format.setForeground(QColor(QStringLiteral("#f87171"))); // light red
        } else if (line.contains(QStringLiteral("[WARN]"), Qt::CaseInsensitive) ||
                   line.contains(QStringLiteral("[WARNING]"), Qt::CaseInsensitive)) {
            format.setForeground(QColor(QStringLiteral("#fbbf24"))); // yellow / amber
        } else if (line.contains(QStringLiteral("[INFO]"), Qt::CaseInsensitive)) {
            format.setForeground(QColor(QStringLiteral("#38bdf8"))); // light blue
        } else if (line.contains(QStringLiteral("[DEBUG]"), Qt::CaseInsensitive) ||
                   line.contains(QStringLiteral("[TRACE]"), Qt::CaseInsensitive)) {
            format.setForeground(QColor(QStringLiteral("#94a3b8"))); // muted slate
        }

        cursor.insertText(line, format);
        if (i < lines.size() - 1 || text.endsWith(QLatin1Char('\n'))) {
            cursor.insertText(QStringLiteral("\n"));
        }
    }

    if (m_autoScroll) {
        m_textEdit->verticalScrollBar()->setValue(m_textEdit->verticalScrollBar()->maximum());
    }
}

void ConsoleWindow::clearLog()
{
    m_textEdit->clear();
}

void ConsoleWindow::copyLog()
{
    if (m_textEdit) {
        QGuiApplication::clipboard()->setText(m_textEdit->toPlainText());
    }
}

void ConsoleWindow::exportLog()
{
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Экспорт лога игры"),
        QDir::homePath() + QStringLiteral("/minecraft_log.txt"),
        tr("Текстовые файлы (*.txt);;Лог файлы (*.log);;Все файлы (*.*)"));

    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Ошибка сохранения"),
                             tr("Не удалось открыть файл для записи:\n%1").arg(filePath));
        return;
    }

    QTextStream out(&file);
    out << m_textEdit->toPlainText();
    file.close();

    QMessageBox::information(this, tr("Успех"),
                             tr("Лог успешно сохранен в файл:\n%1").arg(filePath));
}

void ConsoleWindow::onAutoScrollToggled(bool checked)
{
    m_autoScroll = checked;
    if (m_autoScroll) {
        m_textEdit->verticalScrollBar()->setValue(m_textEdit->verticalScrollBar()->maximum());
    }
}
