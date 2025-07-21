#include "searchfunctionality.h"
#include "../../styles/StyleManager.h"
#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QTextDocument>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>

SearchFunctionality::SearchFunctionality(QObject *parent)
    : QObject(parent)
{
}

void SearchFunctionality::toggleSearchBar(const SearchContext &context, QWidget *container)
{
    if (!context.searchWidget || !context.searchInput || !context.textEdit || !container) return;
    
    if (context.searchWidget->isVisible()) {
        context.searchWidget->hide();
        context.searchInput->clear();
        QTextCursor cursor = context.textEdit->textCursor();
        cursor.clearSelection();
        context.textEdit->setTextCursor(cursor);
        context.textEdit->setExtraSelections(QList<QTextEdit::ExtraSelection>());
        context.textEdit->setFocus();
        if (context.searchButton) {
            context.searchButton->setChecked(false);
        }
        emit searchBarToggled(false);
    } else {
        int x = container->width() - context.searchWidget->width() - 20;
        int y = container->height() - context.searchWidget->height() - 20;
        context.searchWidget->move(x, y);
        context.searchWidget->show();
        context.searchWidget->raise();
        context.searchInput->setFocus();
        context.searchInput->selectAll();
        if (context.searchButton) {
            context.searchButton->setChecked(true);
        }
        emit searchBarToggled(true);
    }
}

void SearchFunctionality::performSearch(QLineEdit *searchInput, QTextEdit *textEdit, QString &lastSearchText)
{
    if (!searchInput || !textEdit) return;
    
    QString searchText = searchInput->text();
    
    if (searchText.isEmpty()) {
        QTextCursor cursor = textEdit->textCursor();
        cursor.clearSelection();
        textEdit->setTextCursor(cursor);
        textEdit->setExtraSelections(QList<QTextEdit::ExtraSelection>());
        return;
    }
    
    if (searchText != lastSearchText) {
        QTextCursor cursor = textEdit->textCursor();
        cursor.movePosition(QTextCursor::Start);
        textEdit->setTextCursor(cursor);
        lastSearchText = searchText;
    }
    
    bool found = textEdit->find(searchText);
    
    if (!found) {
        QTextCursor cursor = textEdit->textCursor();
        cursor.movePosition(QTextCursor::Start);
        textEdit->setTextCursor(cursor);
        found = textEdit->find(searchText);
    }
    
    if (found) {
        QTextCursor currentMatchCursor = textEdit->textCursor();
        highlightAllMatches(textEdit, searchText, currentMatchCursor);
    }
}

void SearchFunctionality::findNext(const SearchContext &context)
{
    if (!context.searchWidget || !context.searchInput || !context.textEdit || 
        !context.searchWidget->isVisible() || context.searchInput->text().isEmpty()) {
        return;
    }
    
    QString searchText = context.searchInput->text();
    bool found = context.textEdit->find(searchText);
    
    if (!found) {
        QTextCursor cursor = context.textEdit->textCursor();
        cursor.movePosition(QTextCursor::Start);
        context.textEdit->setTextCursor(cursor);
        found = context.textEdit->find(searchText);
    }
    
    if (found) {
        QTextCursor currentMatchCursor = context.textEdit->textCursor();
        highlightAllMatches(context.textEdit, searchText, currentMatchCursor);
    }
}

void SearchFunctionality::findPrevious(const SearchContext &context)
{
    if (!context.searchWidget || !context.searchInput || !context.textEdit || 
        !context.searchWidget->isVisible() || context.searchInput->text().isEmpty()) {
        return;
    }
    
    QString searchText = context.searchInput->text();
    bool found = context.textEdit->find(searchText, QTextDocument::FindBackward);
    
    if (!found) {
        QTextCursor cursor = context.textEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        context.textEdit->setTextCursor(cursor);
        found = context.textEdit->find(searchText, QTextDocument::FindBackward);
    }
    
    if (found) {
        QTextCursor currentMatchCursor = context.textEdit->textCursor();
        highlightAllMatches(context.textEdit, searchText, currentMatchCursor);
    }
}

void SearchFunctionality::highlightAllMatches(QTextEdit *textEdit, const QString &searchText, const QTextCursor &currentMatch)
{
    QList<QTextEdit::ExtraSelection> extraSelections;
    QTextDocument *document = textEdit->document();
    QTextCursor highlightCursor(document);
    
    // Setup the format for other occurrences (orange background)
    QTextEdit::ExtraSelection extraSelection;
    QTextCharFormat format;
    format.setBackground(QColor(StyleManager::Colors::PRIMARY_ORANGE));
    format.setForeground(QColor(StyleManager::Colors::BLACK));
    
    // Find all occurrences and highlight them (except the current one)
    while (!highlightCursor.isNull() && !highlightCursor.atEnd()) {
        highlightCursor = document->find(searchText, highlightCursor);
        if (!highlightCursor.isNull()) {
            // Only add if it's not the current selection
            if (!currentMatch.isNull() && 
                (highlightCursor.position() != currentMatch.position() || 
                 highlightCursor.anchor() != currentMatch.anchor())) {
                extraSelection.cursor = highlightCursor;
                extraSelection.format = format;
                extraSelections.append(extraSelection);
            }
        }
    }
    
    // Apply all selections
    textEdit->setExtraSelections(extraSelections);
}

QWidget* SearchFunctionality::createSearchWidget(QWidget *parent, QLineEdit *&searchInput, QPushButton *&closeButton)
{
    QWidget *searchWidget = new QWidget(parent);
    searchWidget->setObjectName("searchWidget");
    searchWidget->setMaximumHeight(35);
    searchWidget->setMinimumWidth(300);
    searchWidget->setMaximumWidth(400);
    searchWidget->hide();
    
    searchWidget->setStyleSheet(QString(
        "#searchWidget {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 4px;"
        "}"
        ).arg(StyleManager::Colors::blackAlpha(220))
         .arg(StyleManager::Colors::primaryOrangeAlpha(100)));
    
    QHBoxLayout *searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(8, 4, 8, 4);
    searchLayout->setSpacing(8);
    
    
    searchInput = new QLineEdit(searchWidget);
    searchInput->setPlaceholderText("Search...");
    searchInput->setStyleSheet(QString(
        "QLineEdit {"
        "  background-color: transparent;"
        "  border: none;"
        "  color: %1;"
        "  font-size: 12px;"
        "  padding: 2px 8px;"
        "}"
        "QLineEdit:focus {"
        "  outline: none;"
        "}")
        .arg(StyleManager::Colors::WHITE));
    
    closeButton = new QPushButton("✕", searchWidget);
    closeButton->setMaximumSize(20, 20);
    closeButton->setFlat(true);
    closeButton->setStyleSheet(QString(
        "QPushButton {"
        "  background-color: transparent;"
        "  border: none;"
        "  color: %1;"
        "  font-size: 14px;"
        "  padding: 0px;"
        "}"
        "QPushButton:hover {"
        "  color: %2;"
        "}")
        .arg(StyleManager::Colors::primaryOrangeAlpha(150))
        .arg(StyleManager::Colors::WHITE));
    
    searchLayout->addWidget(searchInput, 1);
    searchLayout->addWidget(closeButton);
    
    searchInput->installEventFilter(parent);
    
    return searchWidget;
}