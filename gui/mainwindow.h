#pragma once

#include <QMainWindow>
#include <memory>

// On windows, we need to include winsock2 before other instances of winsock
#ifdef WIN32
#include <winsock2.h>
#endif

class PhxWidget;
class ConsoleWidget;
class Beam;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

  bool connectToServer(quint16 port);
  void setBeamInstance(Beam *beam);
  void toggleConsole();

protected:
  void closeEvent(QCloseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private slots:
  void showAbout() const;
  void handleBeamOutput(const QString &output);
  void handleBeamError(const QString &error);

private:
  void initializePhxWidget(quint16 port);
  void initializeConsole();

private:
  std::unique_ptr<PhxWidget> phxWidget;
  std::unique_ptr<ConsoleWidget> consoleWidget;
  Beam *beamInstance;
};