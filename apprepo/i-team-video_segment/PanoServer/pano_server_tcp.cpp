/*
 *  pano_server_tcp.cpp
 *  PanoServer
 *
 *  Created by Matthias Grundmann on 3/27/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "pano_server_tcp.h"

#include <algorithm>

#include <QtNetwork>
#include <QtGui>

#include "compositing_widget.h"

PanoServerTCP::PanoServerTCP(int port, QTextEdit* output) : port_(port), server_(0), output_(output) {
  
}

PanoServerTCP::~PanoServerTCP() {
  delete server_;
}

void PanoServerTCP::Restart() {
  // Close running server.
  delete server_;
  server_ = new QTcpServer();
  
  if (server_->listen(QHostAddress::Any, port_)) {
    log(QString("New Server - listing on TCP port ") + QString::number(port_));
  } else {
    log("ERROR: Server failed to start!");
  }
  
  connect(server_, SIGNAL(newConnection()), this, SLOT(NewConnection()));
}

void PanoServerTCP::Write(const QByteArray& msg, QTcpSocket* receiver, QTcpSocket* exclusion) {
  if (receiver) {
    receiver->write(msg);
    receiver->flush();
  } else {
    for (vector<TcpConnection>::iterator i = connections_.begin();
         i != connections_.end();
         ++i) {
      QTcpSocket* socket = i->socket;
      if (socket != exclusion) {
        socket->write(msg);
        socket->flush();
      }
    }
  }
}

void PanoServerTCP::SendAllImages() {
  QTcpSocket* conn = latest_conn_;
  latest_conn_ = 0;
  
  const vector<ImageInfo>& img_infos = view_->getImageInfos();
  for (vector<ImageInfo>::const_iterator ii = img_infos.begin();
       ii != img_infos.end();
       ++ii) {
    QByteArray img_data;
    QBuffer buffer(&img_data);
    buffer.open(QIODevice::WriteOnly);
    ii->image->save(&buffer, "PNG");  
    
    QByteArray to_send;
    QDataStream send_stream(&to_send, QIODevice::WriteOnly);
    send_stream.setByteOrder(QDataStream::LittleEndian);
    
    int preload_sz = sizeof(float) * 4 + sizeof(int);
    int total_sz = preload_sz + img_data.size();
    send_stream << 0x13030002 << total_sz
                << ii->img_id << ii->scale << ii->rotation << ii->trans_x << ii->trans_y;
    
    to_send.append(img_data);
    Write(to_send, conn);    
  }

}

void PanoServerTCP::NewConnection() {
  // Open Socket to client.
  QTcpSocket* conn = server_->nextPendingConnection();
  
  if (!conn) {
    log("ERROR: can't establish connection to host.");
    return;
  }
  
  log (QString("Connected to peer :") + conn->peerAddress().toString());
  connections_.push_back(TcpConnection(conn));
  
  latest_conn_ = conn;
  QTimer::singleShot(500, this, SLOT(SendAllImages()));
  
  connect(conn, SIGNAL(readyRead()), this, SLOT(IncomingData()));
  connect(conn, SIGNAL(disconnected()), this, SLOT(ConnectionClosed()));
}

void PanoServerTCP::ConnectionClosed() {
  // Remove from peer list.
  QTcpSocket* conn = (QTcpSocket*) sender();
  log (QString("WARNING: Connection to peer ") + conn->peerAddress().toString() + " closed.");
  
  vector<TcpConnection>::iterator i = std::find(connections_.begin(),
                                                connections_.end(),
                                                TcpConnection(conn));
  vector<ImageInfo>& img_infos = view_->getImageInfos();
  for (vector<ImageInfo>::iterator ii = img_infos.begin();
       ii != img_infos.end();
       ++ii) {
    if (ii->locked_by == conn) {
      ii->locked_by = 0;
      SendReleaseToClients(ii->img_id);
    }
  }
  
  if (i != connections_.end())
    connections_.erase(i);
  
  conn->deleteLater();
}

void PanoServerTCP::IncomingData() {
  QTcpSocket* conn = (QTcpSocket*) sender();
  vector<TcpConnection>::iterator host = std::find(connections_.begin(),
                                                   connections_.end(),
                                                   TcpConnection(conn));
  
  if (host == connections_.end()) {
    log("Client to registered at TCP server.");
    return;
  }
  
  if (conn->bytesAvailable() <= 0) {
    log("Error on incoming data. Packet is empty!");
    return;
  }
  
  QByteArray data = conn->readAll();
  int data_pos = 0;
  
  // Multiple packages possible.
  while (data_pos < data.length()) {
    // Continue package?
    if (host->pkg.length()) {
      int remaining_data_bytes = data.length() - data_pos;
      int remaining_bytes = host->target_sz - host->pkg.length();
      
      if (remaining_data_bytes <= remaining_bytes) {
        host->pkg.append(data.mid(data_pos, remaining_data_bytes));
        data_pos += remaining_data_bytes;
      } else {
        // Another package is waiting.
        host->pkg.append(data.mid(data_pos, remaining_bytes));
        data_pos += remaining_bytes;
      }
      
    } else {
      // New package.
      QDataStream data_stream(data.mid(data_pos));
      data_stream.setByteOrder(QDataStream::LittleEndian);
      int msg_id;
      int length;
      
      data_stream >> msg_id;
      data_stream >> length;
      
      data_pos += sizeof(int) * 2;
      int remaining_data_bytes = data.length() - data_pos;
      
      switch (msg_id) {
        case 0x13030002: 
        {
          log("New image uploaded");          
          // Most likely more packages are incoming ...
          host->target_sz = length;
          host->msg_id = msg_id;
          
          if (length > remaining_data_bytes) {
            host->pkg.append(data.mid(data_pos, remaining_data_bytes));
            data_pos += remaining_data_bytes;
          } else {
            host->pkg.append(data.mid(data_pos, length));
            data_pos += length;
          }
          break;
        }
        case 0x13030005:
        {
          // Image lock request.
          QDataStream img_id_stream(data.mid(data_pos));
          img_id_stream.setByteOrder(QDataStream::LittleEndian);
          int img_id;
          img_id_stream >> img_id;
          data_pos += sizeof(int);
          
          if (view_->aquireLock(img_id, conn)) {
            // Notify all clients.
            SendLockToClients(img_id);
          } else {
            log("Failed lock request from client. Lock already aquired.");
          }
          
          break;
        }
        case 0x13030006:
        {
          // Image release request.
          QDataStream img_id_stream(data.mid(data_pos));
          img_id_stream.setByteOrder(QDataStream::LittleEndian);
          int img_id;
          img_id_stream >> img_id;
          data_pos += sizeof(int);
          
          if (view_->releaseLock(img_id, conn)) {
            // Notify all clients.
            SendReleaseToClients(img_id);
          } else {
            log("Failed release request. Inconsistency error occured.\n");
          }
          
          break;
        }
        default:
          log("Unknown package incoming!");
          break;
      }
    }
      
    // Read completely?
    if (host->pkg.length() == host->target_sz) {
      switch(host->msg_id) {
        case 0x13030002:
        {
          // Get initial transformation.
          QDataStream pkg_stream(host->pkg);
          pkg_stream.setByteOrder(QDataStream::LittleEndian);
          float scale, rotation, trans_x, trans_y;
          pkg_stream >> scale >> rotation >> trans_x >> trans_y;
          
          QByteArray img_data = host->pkg.mid(sizeof(float) * 4);
          QImage img = QImage::fromData(img_data, "png");
          int img_id = view_->AddImage(img);
          log(QString("Assigned id ") + QString::number(img_id) + " to image. Sending ACK.");
          view_->updateImgPosition(img_id, scale, rotation, trans_x, trans_y);
          
          // Send img_id back.
          QByteArray img_id_package;
          int img_id_sz = sizeof(int);
          QDataStream img_id_stream(&img_id_package, QIODevice::WriteOnly);
          img_id_stream.setByteOrder(QDataStream::LittleEndian);
          img_id_stream << 0x13030003 << img_id_sz << img_id;
          Write(img_id_package, conn, 0);
          
          // Send to remaining cients.
          QByteArray to_send;
          QDataStream send_stream(&to_send, QIODevice::WriteOnly);
          send_stream.setByteOrder(QDataStream::LittleEndian);
          
          int preload_sz = sizeof(float) * 4 + sizeof(int);
          int total_sz = preload_sz + img_data.size();
          send_stream << host->msg_id << total_sz
                      << img_id << scale << rotation << trans_x << trans_y;
          
          to_send.append(img_data);
          
          Write(to_send, 0, conn);
          break;
        }
        default:
          break;
      }
      host->pkg.clear();
    }
  }
}

void PanoServerTCP::SendLockToClients(int img_id) {
  QByteArray pkg;
  int pkg_sz = sizeof(int);
  QDataStream pkg_stream(&pkg, QIODevice::WriteOnly);
  pkg_stream.setByteOrder(QDataStream::LittleEndian);
  pkg_stream << 0x13030005 << pkg_sz << img_id;
  Write(pkg);
}

void PanoServerTCP::SendReleaseToClients(int img_id) {
  QByteArray pkg;
  int pkg_sz = sizeof(int);
  QDataStream pkg_stream(&pkg, QIODevice::WriteOnly);
  pkg_stream.setByteOrder(QDataStream::LittleEndian);
  pkg_stream << 0x13030006 << pkg_sz << img_id;
  Write(pkg);
}

void PanoServerTCP::log(const QString& s) {
  output_->setPlainText(output_->toPlainText() + s + "\n");
  QScrollBar* sb = output_->verticalScrollBar();
  sb->setValue(sb->maximum());
}

