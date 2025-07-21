#include "tabswitcher.h"
#include <QPushButton>
#include <QStackedWidget>

void TabSwitcher::switchTab(int index, const QList<QPushButton*> &tabButtons, QStackedWidget *stack)
{
    for (int i = 0; i < tabButtons.size(); ++i) {
        tabButtons[i]->setChecked(i == index);
    }
    if (stack) {
        stack->setCurrentIndex(index);
    }
}

void TabSwitcher::switchConsoleTab(int index, const QList<QPushButton*> &tabButtons, 
                                           QStackedWidget *stack, const QList<QPushButton*> &allButtons)
{
    // Update tab button states
    switchTab(index, tabButtons, stack);
    
    // Hide all control buttons first
    for (auto btn : allButtons) {
        if (btn) btn->setVisible(false);
    }
    
    // Show appropriate buttons based on tab index
    // The button layout is: autoScroll, search, zoomOut, zoomIn for BEAM Log
    //                      guiAutoScroll, guiSearch, guiZoomOut, guiZoomIn for GUI Log
    //                      elixirConsoleZoomOut, elixirConsoleZoomIn for Elixir Console
    switch (index) {
        case 0: // BEAM Log
            if (allButtons.size() > 3) {
                allButtons[0]->setVisible(true); // autoScrollButton
                allButtons[1]->setVisible(true); // beamLogSearchButton
                allButtons[2]->setVisible(true); // consoleZoomOutButton
                allButtons[3]->setVisible(true); // consoleZoomInButton
            }
            break;
        case 1: // GUI Log
            if (allButtons.size() > 7) {
                allButtons[4]->setVisible(true); // guiLogAutoScrollButton
                allButtons[5]->setVisible(true); // guiLogSearchButton
                allButtons[6]->setVisible(true); // guiLogZoomOutButton
                allButtons[7]->setVisible(true); // guiLogZoomInButton
            }
            break;
        case 2: // Elixir Console
            if (allButtons.size() > 9) {
                allButtons[8]->setVisible(true); // elixirConsoleZoomOutButton
                allButtons[9]->setVisible(true); // elixirConsoleZoomInButton
            }
            break;
    }
}

void TabSwitcher::switchDevToolsTab(int index, QPushButton *devToolsButton, QPushButton *dashboardButton,
                                            QStackedWidget *stack, const QList<QPushButton*> &zoomButtons)
{
    if (devToolsButton) devToolsButton->setChecked(index == 0);
    if (dashboardButton) dashboardButton->setChecked(index == 1);
    if (stack) stack->setCurrentIndex(index);
    
    // Update zoom button visibility
    if (zoomButtons.size() >= 4) {
        zoomButtons[0]->setVisible(index == 0); // devtools zoom out
        zoomButtons[1]->setVisible(index == 0); // devtools zoom in
        zoomButtons[2]->setVisible(index == 1); // dashboard zoom out
        zoomButtons[3]->setVisible(index == 1); // dashboard zoom in
    }
}