#include "stdafx.h"
#include "manager.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	Manager w;
	w.show();
	return a.exec();
}
