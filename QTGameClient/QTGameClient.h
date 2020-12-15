#pragma once

#include <QtWidgets/QMainWindow>
#include "socket.h"
#include "ui_QTGameClient.h"
#include <QKeyEvent>
#include <QDebug>

class QTGameClient : public QMainWindow
{
    Q_OBJECT

public:
    QTGameClient(QWidget *parent = Q_NULLPTR);
    QLabel* imageLabel;
protected:
    void keyPressEvent(QKeyEvent*);
private:
    Ui::QTGameClientClass ui;
    Socket::UDPSender udpSender;
};
