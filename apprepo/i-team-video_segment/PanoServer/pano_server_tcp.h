/*
 *  pano_server_tcp.h
 *  PanoServer
 *
 *  Created by Matthias Grundmann on 3/27/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#ifndef PANO_SERVER_TCP_H__
#define PANO_SERVER_TCP_H__

#include <vector>
using std::vector;

#include <QString>
#include <QObject>

class QTcpServer;
class QTcpSocket;
class QTcpConnection;
class QTextEdit;
class CompositingWidget;

struct TcpConnection {
  TcpConnection(QTcpSocket* sock) : socket(sock) {}
  bool operator==(const TcpConnection& rhs) { return socket == rhs.socket; }
  QTcpSocket* socket;
  QByteArray pkg;
  int target_sz;
  int msg_id;
};

class PanoServerTCP : public QObject {
  Q_OBJECT
public:
  PanoServerTCP(int port, QTextEdit* output);
  ~PanoServerTCP();
  
  void Restart();
  void Write(const QByteArray& msg, QTcpSocket* receiver =0, QTcpSocket* exlusion =0);
  void setView(CompositingWidget* w) {view_ = w;}
  
  void SendLockToClients(int img_id);
  void SendReleaseToClients(int img_id);
  
public slots:
  void NewConnection();
  void ConnectionClosed();
  void IncomingData();
  void SendAllImages();
  
protected:
  void log(const QString& s);
protected:
  int port_;
  QTcpServer* server_;
  vector<TcpConnection> connections_;
  QTcpSocket* latest_conn_;
  
  QTextEdit* output_;
  CompositingWidget* view_;
};

#endif