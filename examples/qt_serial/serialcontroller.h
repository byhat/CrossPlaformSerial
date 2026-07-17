#ifndef SERIALCONTROLLER_H
#define SERIALCONTROLLER_H

#include <QObject>
#include <QString>
#include <atomic>
#include <mutex>
#include <thread>

#include <cps/SerialPort.hpp>
#include <cps/SerialTypes.hpp>

/**
 * Bridges the cps C++ serial API to QML.
 *
 * The cps readyRead/error callbacks fire on the library's internal worker thread;
 * the data is read there and forwarded to the GUI thread via queued signals
 * (dataReceived/errorOccurred), so QML is never touched off-thread.
 *
 * connectPort() runs the (potentially blocking) open on a background std::thread:
 * on Android cps requests USB permission synchronously inside open(), and the
 * permission result is delivered on the GUI thread's Looper — so open() must NOT
 * run on the GUI thread, or it would deadlock waiting for a result it itself blocks.
 */
class SerialController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString portName READ portName NOTIFY connectedChanged)

public:
    explicit SerialController(QObject* parent = nullptr);
    ~SerialController() override;

    bool connected() const { return connected_.load(); }
    QString portName() const { return connectedPort_; }

    Q_INVOKABLE void connectPort(const QString& portName, qint32 baudRate = 115200);
    Q_INVOKABLE void disconnectPort();
    Q_INVOKABLE qint64 send(const QString& text);
    Q_INVOKABLE void clear();

signals:
    void connectedChanged(bool connected);
    void dataReceived(const QString& data);
    void errorOccurred(const QString& message);

private:
    void onReadyRead();   // runs on the cps worker thread
    void onCpsError(cps::SerialError e);

    static QString errorToString(cps::SerialError e);

    cps::SerialPort port_;
    std::atomic<bool> connected_{false};
    QString connectedPort_;
    std::thread connectThread_;
    std::mutex openMx_; // serializes open()/close()
};

#endif // SERIALCONTROLLER_H
