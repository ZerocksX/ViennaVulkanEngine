#include "QTGameClient.h"
#include <QtWidgets/QApplication>
#include <QThread>
#include <QDebug>
#include "decode.h"
#include "socket.h"
#include <chrono>
#include <condition_variable>


class VideoDecoderThread : public QThread {
private:
    QLabel* imageLabel;
    std::mutex pktM;
    std::mutex startM;
    std::unique_lock<std::mutex> pktLock;
    std::unique_lock<std::mutex> startLock;
public:
    std::map<int, MyPackage> packetBuffer;
    std::condition_variable condNewPkt;
    std::condition_variable condStart;
    VideoDecoderThread(QLabel* iL) : imageLabel(iL) {
        pktLock = std::unique_lock<std::mutex>(pktM);
        startLock = std::unique_lock<std::mutex>(startM);
    }
    void run() override {
        Video::Decoder d(imageLabel);
        int nextId = 0;
        condStart.wait(startLock);
        while (true) {
            QThread::msleep(25);
            while (!packetBuffer.empty())
            {
                auto pkt = packetBuffer.begin();
                if (pkt->first >= nextId) { 

                    //auto now = std::chrono::system_clock::now();
                    d.decode_buffer((uint8_t*)pkt->second.data, pkt->second.len);
                    //char buff[100];
                    //sprintf(buff, "Decoding in milliseconds: %d\n", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - now).count());
                    //qDebug() << buff;
                    nextId = pkt->first + 1;
                }
                packetBuffer.erase(pkt->first);
            }

        }
    }
};


class SocketReaderThread : public QThread {
private:
    Socket::UDPReciever server;
    VideoDecoderThread* decoderThread;
    bool first;
public:
    SocketReaderThread(VideoDecoderThread* d) : decoderThread(d) {
        first = true;
    }
    void run() override {
        int counter = 0;
        while (true)
        {
            MyPackage* pkt = this->server.recieve();
            if (first) {
                first = false;
                decoderThread->condStart.notify_one();
            }
            decoderThread->packetBuffer.insert(std::make_pair(pkt->id*10000 + pkt->current, *pkt));
        }

    }
};

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QTGameClient w;
    w.show();
    QImage qImage;
    qImage.load(
        QString("C:\\Users\\pavao\\Documents\\erasmus\\Courses\\CloudGaming\\VVE\\GamePlayer\\test18.jpg")
    );
    w.imageLabel->setMaximumWidth(1280);
    w.imageLabel->setMaximumHeight(720);
    VideoDecoderThread* qDecoder = new VideoDecoderThread(w.imageLabel);
    SocketReaderThread* qReader = new SocketReaderThread(qDecoder);
    qDecoder->start();
    qReader->start();
    return a.exec();
}
