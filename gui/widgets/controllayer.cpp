#include "controllayer.h"
#include "StyleManager.h"
#include <QResizeEvent>

ControlLayer::ControlLayer(QWidget *parent)
    : QWidget(parent)
    , m_consoleVisible(false)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setupControls();
    connectSignals();
}

void ControlLayer::setupControls()
{
    m_sizeDownButton = new QPushButton("-", this);
    m_sizeUpButton = new QPushButton("+", this);
    m_openExternalBrowserButton = new QPushButton(" E ", this);
    m_resetBrowserButton = new QPushButton(" R ", this);
    m_consoleToggleButton = new QPushButton(" ▲ ", this);

    QString buttonStyle = StyleManager::guiButton();

    m_sizeDownButton->setStyleSheet(buttonStyle);
    m_sizeUpButton->setStyleSheet(buttonStyle);
    m_openExternalBrowserButton->setStyleSheet(buttonStyle);
    m_resetBrowserButton->setStyleSheet(buttonStyle);
    m_consoleToggleButton->setStyleSheet(buttonStyle);

    m_sizeDownButton->setFixedWidth(30);
    m_sizeUpButton->setFixedWidth(30);
    m_openExternalBrowserButton->setFixedWidth(30);
    m_resetBrowserButton->setFixedWidth(30);
    m_consoleToggleButton->setFixedWidth(30);

    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setContentsMargins(0, 0, 0, 0);
    m_buttonLayout->addWidget(m_consoleToggleButton);
    m_buttonLayout->addWidget(m_resetBrowserButton);
    m_buttonLayout->addWidget(m_openExternalBrowserButton);
    m_buttonLayout->addWidget(m_sizeDownButton);
    m_buttonLayout->addWidget(m_sizeUpButton);

    setLayout(m_buttonLayout);
    setStyleSheet(
        QString("ControlLayer { "
        "  background-color: %1; "
        "  border-top: 1px solid %2; "
        "  border-bottom: 1px solid %2; "
        "}")
        .arg(StyleManager::Colors::blackAlpha(191))
        .arg(StyleManager::Colors::primaryOrangeAlpha(100))
    );

    positionControls();
}

void ControlLayer::connectSignals()
{
    connect(m_sizeDownButton, &QPushButton::released, this, &ControlLayer::sizeDown);
    connect(m_sizeUpButton, &QPushButton::released, this, &ControlLayer::sizeUp);
    connect(m_openExternalBrowserButton, &QPushButton::released, this, &ControlLayer::openExternalBrowser);
    connect(m_resetBrowserButton, &QPushButton::released, this, &ControlLayer::resetBrowser);
    connect(m_consoleToggleButton, &QPushButton::released, this, &ControlLayer::toggleConsole);
}

void ControlLayer::setConsoleVisible(bool visible)
{
    m_consoleVisible = visible;
    if (m_consoleVisible) {
        m_consoleToggleButton->setText(" ▼ ");
    } else {
        m_consoleToggleButton->setText(" ▲ ");
    }
}

void ControlLayer::positionControls()
{
    QWidget* parentWidget = this->parentWidget();
    if (!parentWidget) return;
    
    QSize containerSize = this->sizeHint();
    int margin = 10;
    int scrollbarBuffer = 30;
    
    int xPos = parentWidget->width() - containerSize.width() - margin - scrollbarBuffer;
    int yPos = parentWidget->height() - containerSize.height() - margin;
    
    setGeometry(xPos, yPos, containerSize.width(), containerSize.height());
    raise();
}

void ControlLayer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    positionControls();
}