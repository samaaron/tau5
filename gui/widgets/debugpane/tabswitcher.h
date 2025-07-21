#ifndef DEBUGPANE_TABSWITCHER_H
#define DEBUGPANE_TABSWITCHER_H

#include <QList>

class QPushButton;
class QStackedWidget;

class TabSwitcher
{
public:
    static void switchTab(int index, const QList<QPushButton*> &tabButtons, QStackedWidget *stack);
    static void switchConsoleTab(int index, const QList<QPushButton*> &tabButtons, 
                                QStackedWidget *stack, const QList<QPushButton*> &allButtons);
    static void switchDevToolsTab(int index, QPushButton *devToolsButton, QPushButton *dashboardButton,
                                 QStackedWidget *stack, const QList<QPushButton*> &zoomButtons);
};

#endif // DEBUGPANE_TABSWITCHER_H