/*
 *  pano_server.cpp
 *  VideoPatch
 *
 *  Created by Matthias Grundmann on 3/12/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 */

#include "pano_server_udp.h"
#include "compositing_widget.h"
#include <algorithm>

#include <QtNetwork>
#include <QtGui>

PanoServerUDP::PanoServerUDP(int port, QTextEdit* output) : port_(port), socket_(0), output_(output) {

}

PanoServerUDP::~PanoServerUDP() {
  delete socket_;
}

void PanoServerUDP::Restart() {
  client_addresses_.clear();
  client_counter_ = 0;
  
  // Close running server.
  delete socket_;
  socket_ = new QUdpSocket();
  
  if (socket_->bind(port_)) {
    log(QString("New Server - listing on UDP port ") + QString::number(port_));
  } else {
    log("ERROR: Server failed to start!");
  }
  
  connect(socket_, SIGNAL(readyRead()), this, SLOT(IncomingData()));
}

void PanoServerUDP::Write(const QByteArray& msg, QHostAddress* receiver, 
                       QHostAddress* exclusion) {
  if (receiver) {
    socket_->writeDatagram(msg.data(), msg.size(), *receiver, port_);
  } else {
    for (vector<HostInfo>::iterator i = client_addresses_.begin();
         i != client_addresses_.end();
         ++i) {
      if (exclusion && *exclusion == i->addr)
        continue;
    
      socket_->writeDatagram(msg.data(), msg.size(), i->addr, port_);
      socket_->flush();
    }
  }
}

void PanoServerUDP::IncomingData() {
  
  while (socket_->hasPendingDatagrams()) {
    QByteArray datagram;
    datagram.resize(socket_->pendingDatagramSize());
    QHostAddress client_adr;
    socket_->readDatagram(datagram.data(), datagram.size(), &client_adr);
    
    QDataStream data_stream(datagram);
    data_stream.setByteOrder(QDataStream::LittleEndian);
    int msg_id;
    int length;
    
    // Multiple packages possible.
    while (!data_stream.atEnd()) {
      data_stream >> msg_id;
      switch (msg_id) {
        case 0x13030000:
        {
          // Register client at server.
          vector<HostInfo>::iterator i = std::find(client_addresses_.begin(),
                                                   client_addresses_.end(),
                                                   client_adr);
          if (i == client_addresses_.end()) {
            log(QString("Connection request from ") + client_adr.toString() + 
                + ". Sending ACK");
            
            QByteArray new_pkg;
            QDataStream str(&new_pkg, QIODevice::WriteOnly);
            str.setByteOrder(QDataStream::LittleEndian);
            str << msg_id << client_counter_;
            ++client_counter_;
            
            for (int i = 0; i < 10; ++i)
              Write(new_pkg, &client_adr);
            
            client_addresses_.push_back(HostInfo(client_adr));
          } else if (i->last_connect.msecsTo(QTime::currentTime()) > 1000) {
            i->last_connect = QTime::currentTime();
            log(client_adr.toString() + " already connected. Resending ACK.");
            
            QByteArray new_pkg;
            QDataStream str(&new_pkg, QIODevice::WriteOnly);
            str.setByteOrder(QDataStream::LittleEndian);
            str << msg_id << i->client_id;
            
            for (int i = 0; i < 10; ++i)
              Write(new_pkg, &client_adr);
          }
          
          break;
        }
        case 0x13030001: 
        {
          data_stream >> length;
          if (length != 2 * sizeof(int) + 4 * sizeof(float)) {
            log("Position update package corrupted!");
            return;
          }
          
          vector<HostInfo>::iterator i = std::find(client_addresses_.begin(),
                                                   client_addresses_.end(),
                                                   client_adr);
          
          if (i == client_addresses_.end()) {
            log(client_adr.toString() + " not registered at server!");
            break;
          }
          
          int update_count;
          int img_id;
          float scale;
          float rotation;
          float trans_x;
          float trans_y;
          
          data_stream >> update_count >> img_id >> scale >> rotation >> trans_x >> trans_y;
          
          if (update_count > i->last_update_pkg) {
            i->last_update_pkg = update_count;
            view_->updateImgPosition(img_id, scale, rotation, trans_x, trans_y);
            QByteArray new_pkg;
            QDataStream str(&new_pkg, QIODevice::WriteOnly);
            str.setByteOrder(QDataStream::LittleEndian);
            
            length += sizeof(int);
            
            str << msg_id << length << i->client_id << update_count << img_id << scale 
                << rotation << trans_x << trans_y;
            Write (new_pkg, 0, &client_adr); 
            
          }
          break;
        }
        default:
          log("Unknown package incoming!");
          break;
      }
    }
  }
  
  /*QTcpSocket* conn = (QTcpSocket*) sender();
  if (conn->bytesAvailable() > 0) {
    QByteArray data = conn->readAll();
    
    // Send update to all other clients.
    Write(data, conn);
    
    // Process update.
    
    QDataStream data_stream(data);
    data_stream.setByteOrder(QDataStream::LittleEndian);
    int msg_id;
    int length;
    
    // Multiple packages possible.
    while (!data_stream.atEnd()) {
      data_stream >> msg_id;
   //   log(QString("Msg id: ") + QString::number(msg_id, 16));
      switch (msg_id) {
        case 0x13030001: 
        {
       //   log("Position update for ... TODO");
          data_stream >> length;
          int img_id;
          float scale;
          float rotation;
          float trans_x;
          float trans_y;
          
          data_stream >> img_id >> scale >> rotation >> trans_x >> trans_y;
          
          break;
        }
        default:
          log("Unknown package incoming!");
          break;
      }
    }
  } else {
    log("Error on incoming data. Packet is empty!");
  }
   */
}

void PanoServerUDP::log(const QString& s) {
  output_->setPlainText(output_->toPlainText() + s + "\n");
  QScrollBar* sb = output_->verticalScrollBar();
  sb->setValue(sb->maximum());
}
