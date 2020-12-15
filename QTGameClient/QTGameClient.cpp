#include "QTGameClient.h"

QTGameClient::QTGameClient(QWidget *parent)
    : QMainWindow(parent), udpSender("127.0.0.1",5006)
{
    ui.setupUi(this);
    this->imageLabel = ui.imageLabel;
}

void QTGameClient::keyPressEvent(QKeyEvent* event)
{
    KeyPackage keyPkg;
    std::chrono::nanoseconds nsSent = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch());
    keyPkg.nsSent = nsSent;
    switch (event->key())
    {
    case Qt::Key_W:
        keyPkg.key = 'w';
        qDebug() << "w\n";
        break;
    case Qt::Key_S:
        keyPkg.key = 's';
        qDebug() << "s\n";
        break;
    case Qt::Key_A:
        keyPkg.key = 'a';
        qDebug() << "a\n";
        break;
    case Qt::Key_D:
        keyPkg.key = 'd';
        qDebug() << "d\n";
        break;
    case Qt::Key_Up:
        keyPkg.key = 'W';
        qDebug() << "W\n";
        break;
    case Qt::Key_Down:
        keyPkg.key = 'D';
        qDebug() << "D\n";
        break;
    case Qt::Key_Left:
        keyPkg.key = 'A';
        qDebug() << "A\n";
        break;
    case Qt::Key_Right:
        keyPkg.key = 'D';
        qDebug() << "D\n";
        break;
    case Qt::Key_Escape:
        keyPkg.key = 'q';
        qDebug() << "q\n";
        break;
    default:
        qDebug() << "unknown\n";
        return;
    }
    this->udpSender.sendUDP("127.0.0.1", 5006, (char*)&(keyPkg), sizeof(KeyPackage), 0);
}
