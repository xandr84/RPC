#ifndef MANAGER_H
#define MANAGER_H

#include <QtWidgets/QMainWindow>
#include "ui_manager.h"

class Manager : public QMainWindow
{
	Q_OBJECT

public:
	Manager(QWidget *parent = 0);
	~Manager();

private:
	Ui::ManagerClass ui;
};

#endif // MANAGER_H
