//--
// This file is part of Sonic Pi: http://sonic-pi.net
// Full project source: https://github.com/samaaron/sonic-pi
// License: https://github.com/samaaron/sonic-pi/blob/main/LICENSE.md
//
// Copyright 2013, 2014, 2015, 2016 by Sam Aaron (http://sam.aaron.name).
// All rights reserved.
//
// Permission is granted for use, copying, modification, and
// distribution of modified versions of this work as long as this
// notice is included.
//++

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

  PhxWidget *phxWidget;
  Beam *beam;
  QWidget *mainWidget;
};
