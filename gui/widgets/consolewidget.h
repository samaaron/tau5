#ifndef CONSOLEWIDGET_H
#define CONSOLEWIDGET_H

#include <QWidget>
#include <memory>

class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QTextEdit;
class QPropertyAnimation;
class QTimer;
class QCheckBox;
class QScrollBar;

class ConsoleWidget : public QWidget
{
  Q_OBJECT
  Q_PROPERTY(int slidePosition READ slidePosition WRITE setSlidePosition)

public:
  explicit ConsoleWidget(QWidget *parent = nullptr);
  ~ConsoleWidget();

  void appendOutput(const QString &text, bool isError = false);
  void toggle();
  bool isVisible() const { return m_isVisible; }

  int slidePosition() const { return pos().y(); }
  void setSlidePosition(int pos) { move(x(), pos); }

signals:
  void visibilityChanged(bool visible);

private slots:
  void handleAutoScrollToggled(bool checked);
  void animationFinished();

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;

private:
  void setupUi();
  void slide(bool show);

private:
  QTextEdit *m_outputDisplay;
  QCheckBox *m_autoScrollToggle;
  QVBoxLayout *m_mainLayout;
  QHBoxLayout *m_buttonLayout;

  std::unique_ptr<QPropertyAnimation> m_slideAnimation;
  bool m_isVisible;
  bool m_autoScroll;
  int m_maxLines;

  // Resize functionality
  bool m_isResizing;
  int m_resizeStartY;
  int m_resizeStartHeight;
  static constexpr int RESIZE_HANDLE_HEIGHT = 6;
};

#endif // CONSOLEWIDGET_H