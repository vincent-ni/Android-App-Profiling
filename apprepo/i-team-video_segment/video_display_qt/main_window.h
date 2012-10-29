#ifndef MAIN_WINDOW_H__
#define MAIN_WINDOW_H__

#include <string>
#include <QMainWindow>

class QLabel;
class QImage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const std::string& stream_name);

    void SetSize(int sx, int sy);
    void DrawImage(const QImage& image);
private:
  QLabel* main_widget_;
  std::string stream_name_;
};

#endif // MAIN_WINDOW_H__
