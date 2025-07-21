#include "buttonutilities.h"
#include "../../styles/StyleManager.h"
#include <QWidget>
#include <QHBoxLayout>

QPushButton* ButtonUtilities::createTabButton(const QString &text, QWidget *parent)
{
    QPushButton *button = new QPushButton(text, parent);
    button->setCheckable(true);
    button->setStyleSheet(getTabButtonStyle());
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

QPushButton* ButtonUtilities::createZoomButton(const QIcon &icon, const QString &tooltip, QWidget *parent)
{
    QPushButton *button = new QPushButton(parent);
    button->setIcon(icon);
    button->setStyleSheet(getZoomButtonStyle());
    button->setToolTip(tooltip);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

QPushButton* ButtonUtilities::createToolButton(const QIcon &icon, const QString &tooltip, QWidget *parent, 
                                               bool checkable, bool checked)
{
    QPushButton *button = new QPushButton(parent);
    button->setIcon(icon);
    button->setCheckable(checkable);
    button->setChecked(checked);
    button->setStyleSheet(getToolButtonStyle());
    button->setToolTip(tooltip);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

QString ButtonUtilities::getTabButtonStyle()
{
    return QString(
        "QPushButton { "
        "  background: transparent; "
        "  color: %1; "
        "  border: none; "
        "  padding: 2px 8px; "
        "  font-family: %2; "
        "  font-size: %3; "
        "  font-weight: %4; "
        "} "
        "QPushButton:hover { "
        "  background: %5; "
        "} "
        "QPushButton:checked { "
        "  background: %6; "
        "  color: %7; "
        "}")
        .arg(StyleManager::Colors::primaryOrangeAlpha(180))
        .arg(StyleManager::Typography::MONOSPACE_FONT_FAMILY)
        .arg(StyleManager::Typography::FONT_SIZE_SMALL)
        .arg(StyleManager::Typography::FONT_WEIGHT_BOLD)
        .arg(StyleManager::Colors::primaryOrangeAlpha(25))  // 0.1 * 255 ≈ 25
        .arg(StyleManager::Colors::primaryOrangeAlpha(51))  // 0.2 * 255 ≈ 51
        .arg(StyleManager::Colors::PRIMARY_ORANGE);
}

QString ButtonUtilities::getZoomButtonStyle()
{
    return QString(
        "QPushButton { "
        "  background: transparent; "
        "  border: none; "
        "  padding: 2px; "
        "  min-width: 16px; "
        "  max-width: 16px; "
        "  min-height: 16px; "
        "  max-height: 16px; "
        "} "
        "QPushButton:hover { "
        "  background: %1; "
        "} "
        "QPushButton:pressed { "
        "  background: %2; "
        "}")
        .arg(StyleManager::Colors::primaryOrangeAlpha(25))  // 0.1 * 255 ≈ 25
        .arg(StyleManager::Colors::primaryOrangeAlpha(38)); // 0.15 * 255 ≈ 38
}

QString ButtonUtilities::getToolButtonStyle()
{
    return QString(
        "QPushButton { "
        "  background: transparent; "
        "  border: none; "
        "  padding: 2px; "
        "  min-width: 16px; "
        "  max-width: 16px; "
        "  min-height: 16px; "
        "  max-height: 16px; "
        "} "
        "QPushButton:hover { "
        "  background: %1; "
        "}"
        "QPushButton:checked { "
        "  background: %2; "
        "  border-radius: 2px; "
        "}")
        .arg(StyleManager::Colors::primaryOrangeAlpha(25))
        .arg(StyleManager::Colors::primaryOrangeAlpha(64));
}

QString ButtonUtilities::getHeaderButtonStyle()
{
    return QString(
        "QPushButton { "
        "  background: transparent; "
        "  border: none; "
        "  padding: 2px; "
        "  margin: 0 2px; "
        "  min-width: 24px; "
        "  max-width: 24px; "
        "  min-height: 16px; "
        "  max-height: 16px; "
        "} "
        "QPushButton:hover { "
        "  background: %1; "
        "} "
        "QPushButton:pressed { "
        "  background: %2; "
        "} "
        "QPushButton:checked { "
        "  background: %3; "
        "  border-radius: 3px; "
        "} "
        "QPushButton:focus { "
        "  outline: none; "
        "}")
        .arg(StyleManager::Colors::primaryOrangeAlpha(25))   // hover
        .arg(StyleManager::Colors::primaryOrangeAlpha(51))   // pressed
        .arg(StyleManager::Colors::errorBlueAlpha(51));      // checked
}

QWidget* ButtonUtilities::createTabToolbar(QWidget *parent)
{
    QWidget *toolbar = new QWidget(parent);
    toolbar->setFixedHeight(26);
    toolbar->setStyleSheet(QString(
        "QWidget { "
        "  background-color: %1; "
        "  border-bottom: 1px solid %2; "
        "}")
        .arg(StyleManager::Colors::blackAlpha(230))
        .arg(StyleManager::Colors::primaryOrangeAlpha(50)));
    
    return toolbar;
}