
#include "main_window.h"
#include <QApplication.h>

int main (int argc, char** argv)
{
	QApplication a( argc, argv );

	MainWindow w;
	w.show();
	
	return a.exec();
}