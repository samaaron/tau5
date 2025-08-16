#include "customtitlebar.h"
#include "styles/StyleManager.h"
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QStyle>
#include <QIcon>
#include <QWindow>

CustomTitleBar::CustomTitleBar(QWidget *parent)
    : QWidget(parent)
    , m_titleLabel(nullptr)
    , m_minimizeButton(nullptr)
    , m_maximizeButton(nullptr)
    , m_closeButton(nullptr)
    , m_layout(nullptr)
{
    setupUi();
    applyStyles();
}

CustomTitleBar::~CustomTitleBar()
{
}

void CustomTitleBar::setupUi()
{
    setFixedHeight(32);
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(8, 0, 4, 0);
    m_layout->setSpacing(2);
    m_titleLabel = new QLabel("Tau5", this);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_layout->addWidget(m_titleLabel);
    m_layout->addStretch();
    m_minimizeButton = new QPushButton(this);
    m_maximizeButton = new QPushButton(this);
    m_closeButton = new QPushButton(this);
    m_minimizeButton->setFixedSize(BUTTON_WIDTH, BUTTON_HEIGHT);
    m_maximizeButton->setFixedSize(BUTTON_WIDTH, BUTTON_HEIGHT);
    m_closeButton->setFixedSize(BUTTON_WIDTH, BUTTON_HEIGHT);
    m_minimizeButton->setIcon(QIcon(":/images/window-controls/minimize.svg"));
    m_maximizeButton->setIcon(QIcon(":/images/window-controls/maximize.svg"));
    m_closeButton->setIcon(QIcon(":/images/window-controls/close.svg"));
    const QSize iconSize(12, 12);
    m_minimizeButton->setIconSize(iconSize);
    m_maximizeButton->setIconSize(iconSize);
    m_closeButton->setIconSize(iconSize);
    m_layout->addWidget(m_minimizeButton);
    m_layout->addWidget(m_maximizeButton);
    m_layout->addWidget(m_closeButton);
    connect(m_minimizeButton, &QPushButton::clicked, this, &CustomTitleBar::minimizeClicked);
    connect(m_maximizeButton, &QPushButton::clicked, this, &CustomTitleBar::maximizeClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &CustomTitleBar::closeClicked);
    m_minimizeButton->setObjectName("titleBarMinimize");
    m_maximizeButton->setObjectName("titleBarMaximize");
    m_closeButton->setObjectName("titleBarClose");
}

void CustomTitleBar::applyStyles()
{
    QString backgroundColor = StyleManager::Colors::BACKGROUND_TITLEBAR_DARK;
    QString textColor = StyleManager::Colors::TEXT_PRIMARY;
    QString hoverColor = StyleManager::Colors::BACKGROUND_SURFACE_LIGHT;
    QString closeHoverColor = StyleManager::Colors::BUTTON_CLOSE_HOVER;
    QString titleBarStyle = QString(
        "CustomTitleBar {"
        "  background-color: %1;"
        "  border: none;"
        "}"
    ).arg(backgroundColor);
    QString titleStyle = QString(
        "QLabel {"
        "  color: %1;"
        "  font-family: %2;"
        "  font-size: 13px;"
        "  padding-left: 4px;"
        "  background: transparent;"
        "}"
    ).arg(textColor)
     .arg(StyleManager::Typography::DEFAULT_FONT_FAMILY);
    QString buttonStyle = QString(
        "QPushButton {"
        "  background-color: transparent;"
        "  color: %1;"
        "  border: none;"
        "  font-size: 16px;"
        "  font-family: %2;"
        "}"
        "QPushButton:hover {"
        "  background-color: %3;"
        "}"
        "QPushButton:pressed {"
        "  background-color: %4;"
        "}"
    ).arg(textColor)
     .arg(StyleManager::Typography::DEFAULT_FONT_FAMILY)
     .arg(hoverColor)
     .arg(StyleManager::Colors::BACKGROUND_SURFACE_LIGHT);
    QString closeButtonStyle = buttonStyle + QString(
        "QPushButton#titleBarClose:hover {"
        "  background-color: %1;"
        "  color: white;"
        "}"
    ).arg(closeHoverColor);
    setStyleSheet(titleBarStyle);
    m_titleLabel->setStyleSheet(titleStyle);
    m_minimizeButton->setStyleSheet(buttonStyle);
    m_maximizeButton->setStyleSheet(buttonStyle);
    m_closeButton->setStyleSheet(closeButtonStyle);
}

void CustomTitleBar::setTitle(const QString &title)
{
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
}

QString CustomTitleBar::title() const
{
    return m_titleLabel ? m_titleLabel->text() : QString();
}

void CustomTitleBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.fillRect(rect(), QColor(StyleManager::Colors::BACKGROUND_TITLEBAR_DARK));
}

void CustomTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit maximizeClicked();
    }
    QWidget::mouseDoubleClickEvent(event);
}

void CustomTitleBar::updateMaximizeButton()
{
    if (!m_maximizeButton) return;
    QWidget *topLevel = window();
    if (topLevel && topLevel->isMaximized()) {
        m_maximizeButton->setIcon(QIcon(":/images/window-controls/restore.svg"));
    } else {
        m_maximizeButton->setIcon(QIcon(":/images/window-controls/maximize.svg"));
    }
}