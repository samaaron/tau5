#include "debugwidget.h"
#include "../styles/StyleManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QToolBar>
#include <QFontDatabase>

DebugWidget::DebugWidget(QWidget *parent)
    : QWidget(parent)
    , m_toolbar(nullptr)
    , m_toolbarLayout(nullptr)
    , m_contentWidget(nullptr)
    , m_contentLayout(nullptr)
{
}

DebugWidget::~DebugWidget()
{
  // Toolbar is managed by DebugPane
  m_toolbar = nullptr;
}

void DebugWidget::setupUI()
{
  m_mainLayout = new QVBoxLayout(this);
  m_mainLayout->setContentsMargins(0, 0, 0, 0);
  m_mainLayout->setSpacing(0);
  
  setupToolbar();
  setupContent();
  m_mainLayout->addWidget(m_contentWidget);
}

void DebugWidget::setupToolbar()
{
  m_toolbar = new QWidget();
  m_toolbar->setMaximumHeight(28);
  m_toolbar->setStyleSheet(QString(
      "QWidget {"
      "  background-color: %1;"
      "  border-bottom: 1px solid %2;"
      "}")
      .arg(StyleManager::Colors::DARK_BACKGROUND)
      .arg(StyleManager::Colors::primaryOrangeAlpha(100)));
  
  m_toolbarLayout = new QHBoxLayout(m_toolbar);
  m_toolbarLayout->setContentsMargins(5, 2, 5, 2);
  m_toolbarLayout->setSpacing(5);
}

void DebugWidget::setupContent()
{
  m_contentWidget = new QWidget(this);
  m_contentLayout = new QVBoxLayout(m_contentWidget);
  m_contentLayout->setContentsMargins(0, 0, 0, 0);
  m_contentLayout->setSpacing(0);
}

void DebugWidget::setToolbarVisible(bool visible)
{
  if (m_toolbar) {
    m_toolbar->setVisible(visible);
  }
}

QPushButton* DebugWidget::createToolButton(const QString &text, const QString &tooltip, bool checkable)
{
  if (!m_toolbar) return nullptr;
  
  QPushButton *button = new QPushButton(text, m_toolbar);
  button->setToolTip(tooltip);
  button->setCheckable(checkable);
  button->setFixedSize(20, 20);
  
  static bool codiconLoaded = false;
  if (!codiconLoaded) {
    QFontDatabase::addApplicationFont(":/fonts/codicon.ttf");
    codiconLoaded = true;
  }
  
  QString fontFamily = (text == "+" || text == "-") ? "Segoe UI, Arial" : "codicon";
  QString fontSize = (text == "+" || text == "-") ? "16px" : "14px";
  
  button->setStyleSheet(QString(
      "QPushButton {"
      "  font-family: '%1';"
      "  font-size: %2;"
      "  font-weight: bold;"
      "  color: %3;"
      "  background: transparent;"
      "  border: none;"
      "  padding: 2px;"
      "}"
      "QPushButton:hover {"
      "  background-color: %4;"
      "  border-radius: 3px;"
      "}"
      "QPushButton:checked {"
      "  color: %5;"
      "  background-color: %4;"
      "  border-radius: 3px;"
      "}")
      .arg(fontFamily)
      .arg(fontSize)
      .arg(StyleManager::Colors::TIMESTAMP_GRAY)
      .arg(StyleManager::Colors::blackAlpha(50))
      .arg(StyleManager::Colors::PRIMARY_ORANGE));
      
  return button;
}