#include "main_window.h"

#include <QtGui>

MainWindow::MainWindow(const std::string& stream_name)
    : QMainWindow(NULL), stream_name_(stream_name) {
  // Create simple widget used for display.
  main_widget_ = new QLabel(this);
  setWindowTitle(QString(stream_name.c_str()));
}

void MainWindow::SetSize(int sx, int sy) {
  main_widget_->resize(sx, sy);
  resize(sx, sy);
}

void MainWindow::DrawImage(const QImage& image) {
  main_widget_->setPixmap(QPixmap::fromImage(image));
  main_widget_->update();
}
