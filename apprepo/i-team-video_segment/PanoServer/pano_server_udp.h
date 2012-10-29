/*
 *  pano_server.h
 *  VideoPatch
 *
 *  Created by Matthias Grundmann on 3/12/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef PANO_SERVER_UDP_H__
#define PANO_SERVER_UDP_H__

#include <vector>
using std::vector;

#include <QString>
#include <QObject>
#include <QHostAddress>
#include <QTime>

class QUdpSocket;
class QTextEdit;
class CompositingWidget;

struct HostInfo {
  HostInfo(const QHostAddress& host) : addr(host), last_update_pkg(-1),
                                       last_connect(QTime::currentTime()) {}
  QHostAddress addr;
  int client_id;
  int last_update_pkg;
  QTime last_connect;
  bool operator==(const HostInfo& rhs) { return addr == rhs.addr; };
};

class PanoServerUDP : public QObject {
  Q_OBJECT
public:
  PanoServerUDP(int port, QTextEdit* output);
  ~PanoServerUDP();
  
  void Restart();
  void Write(const QByteArray& msg, QHostAddress* receiver =0, QHostAddress* exlusion =0);
             
  void setView(CompositingWidget* w) {view_ = w;}
  
public slots:
  void IncomingData();
  
protected:
  void log(const QString& s);
protected:
  int port_;
  QUdpSocket* socket_;
  vector<HostInfo> client_addresses_;
  
  QTextEdit* output_;
  CompositingWidget* view_;
  
  int client_counter_;
};

#endif