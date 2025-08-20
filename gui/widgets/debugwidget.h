#ifndef DEBUGWIDGET_H
#define DEBUGWIDGET_H

#include <QWidget>
#include <QString>
#include <QList>
#include <memory>

QT_BEGIN_NAMESPACE
class QToolBar;
class QPushButton;
class QHBoxLayout;
class QVBoxLayout;
QT_END_NAMESPACE

class DebugWidget : public QWidget
{
  Q_OBJECT

public:
  explicit DebugWidget(QWidget *parent = nullptr);
  virtual ~DebugWidget();

  virtual QWidget* getToolbar() { return m_toolbar; }
  
  virtual QWidget* getContent() { return m_contentWidget; }
  
  virtual void onActivated() {}
  
  virtual void onDeactivated() {}
  void setToolbarVisible(bool visible);
  
protected:
  void setupUI();
  
  virtual void setupToolbar();
  
  virtual void setupContent();
  QPushButton* createToolButton(const QString &text, const QString &tooltip, 
                                bool checkable = false);

protected:
  QVBoxLayout* m_mainLayout;
  QWidget* m_toolbar;
  QHBoxLayout* m_toolbarLayout;
  QWidget* m_contentWidget;
  QVBoxLayout* m_contentLayout;
};

#endif // DEBUGWIDGET_H