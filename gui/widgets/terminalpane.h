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
    void increaseFontSize();
    void decreaseFontSize();

private:
    void setupUi();
    QWidget* createFontControlBar();
    void createTerminalWidget(QTermWidget* &terminal, bool isTopTerminal);
    void styleTerminal(QTermWidget* terminal);
    void updateTerminalFonts();

    QTermWidget* m_topTerminal;
    QTermWidget* m_bottomTerminal;
    QTermWidget* m_activeTerminal;
    QSplitter* m_terminalSplitter;
    QVBoxLayout *m_mainLayout;
    QString m_workingDirectory;
    int m_currentFontSize;
};

#endif // TERMINALPANE_H