#ifndef TAU5DEVBRIDGE_H
#define TAU5DEVBRIDGE_H

#include <QObject>

class Tau5DevBridge : public QObject
{
    Q_OBJECT

public:
    explicit Tau5DevBridge(QObject *parent = nullptr);

public slots:
    void hardRefresh();

signals:
    void hardRefreshRequested();
};

#endif // TAU5DEVBRIDGE_H