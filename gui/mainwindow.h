#pragma once

#include <fstream>
#include <vector>
#include <memory>

#include <QMainWindow>

// On windows, we need to include winsock2 before other instances of winsock
#ifdef WIN32
#include <winsock2.h>
#endif

class Beam;
class PhxWidget;
class QWebEngineView;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(quint16 port, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void showAbout();

private:
  PhxWidget *phxWidget;
  Beam *beam;
  QWidget *mainWidget;
};
