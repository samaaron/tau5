#ifndef SEARCHFUNCTIONALITY_H
#define SEARCHFUNCTIONALITY_H

#include <QObject>
#include <QString>
#include <QTextEdit>
#include <QLineEdit>

class QWidget;
class QTextCursor;
class QPushButton;

class SearchFunctionality : public QObject
{
    Q_OBJECT

public:
    struct SearchContext {
        QWidget *searchWidget;
        QLineEdit *searchInput;
        QTextEdit *textEdit;
        QString *lastSearchText;
        QPushButton *searchButton;
    };

    SearchFunctionality(QObject *parent = nullptr);

    void toggleSearchBar(const SearchContext &context, QWidget *container);
    void performSearch(QLineEdit *searchInput, QTextEdit *textEdit, QString &lastSearchText);
    void findNext(const SearchContext &context);
    void findPrevious(const SearchContext &context);
    
    static void highlightAllMatches(QTextEdit *textEdit, const QString &searchText, const QTextCursor &currentMatch);
    static QWidget* createSearchWidget(QWidget *parent, QLineEdit *&searchInput, QPushButton *&closeButton);
    
signals:
    void searchBarToggled(bool visible);
};

#endif // SEARCHFUNCTIONALITY_H