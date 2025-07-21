#ifndef BUTTONUTILITIES_H
#define BUTTONUTILITIES_H

#include <QPushButton>
#include <QString>
#include <QIcon>

class QWidget;
class QLineEdit;

class ButtonUtilities
{
public:
    static QPushButton* createTabButton(const QString &text, QWidget *parent);
    static QPushButton* createZoomButton(const QIcon &icon, const QString &tooltip, QWidget *parent);
    static QPushButton* createToolButton(const QIcon &icon, const QString &tooltip, QWidget *parent, 
                                         bool checkable = false, bool checked = false);
    
    static QString getTabButtonStyle();
    static QString getZoomButtonStyle();
    static QString getToolButtonStyle();
    static QString getHeaderButtonStyle();
    
    static QWidget* createTabToolbar(QWidget *parent);
};

#endif // BUTTONUTILITIES_H