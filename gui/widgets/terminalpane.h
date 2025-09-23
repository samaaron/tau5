#ifndef TERMINALPANE_H
#define TERMINALPANE_H

#include <QWidget>
#include <QSplitter>
#include <memory>

class QTermWidget;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;

class TerminalPane : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalPane(QWidget *parent = nullptr);
    ~TerminalPane();

    void setWorkingDirectory(const QString &dir);
    void setVisible(bool visible) override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

signals:
    void closeRequested();

private slots:
    void handleTerminalFinished();
    void handleClearTerminal();
    void handleCopySelection();
    void handlePasteClipboard();

private:
    void setupUi();
    void createTerminalWidget(QTermWidget* &terminal, bool isTopTerminal);
    void styleTerminal(QTermWidget* terminal);

    QTermWidget* m_topTerminal;
    QTermWidget* m_bottomTerminal;
    QTermWidget* m_activeTerminal;
    QSplitter* m_terminalSplitter;
    QPushButton *m_closeButton;
    QPushButton *m_clearButton;
    QPushButton *m_copyButton;
    QPushButton *m_pasteButton;
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_toolbarLayout;
    QString m_workingDirectory;
};

#endif // TERMINALPANE_H