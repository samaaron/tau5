#pragma once

#include <QMainWindow>
#include <memory>

// On windows, we need to include winsock2 before other instances of winsock
#ifdef WIN32
#include <winsock2.h>
#endif

class PhxWidget;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();  // Destructor must be defined in .cpp file

  // Configuration method
  bool connectToServer(quint16 port);

protected:
  void closeEvent(QCloseEvent *event) override;

private slots:
  void showAbout() const;

private:
  void initializePhxWidget(quint16 port);

private:
  std::unique_ptr<PhxWidget> phxWidget;
};