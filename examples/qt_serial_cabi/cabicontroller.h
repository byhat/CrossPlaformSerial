#ifndef CABICONTROLLER_H
#define CABICONTROLLER_H

#include <QObject>
#include <QString>
#include <atomic>
#include <mutex>
#include <thread>

#include <cps/cps.h>   // the only cps header used here (stable C ABI)

/**
 * Qt controller that talks to cps EXCLUSIVELY through the C ABI in <cps/cps.h>:
 * it holds an opaque cps_SerialPort* (cps_serial_new), drives it with cps_serial_*
 * calls, and receives data via cps_Callbacks (C function pointers + void* user_data).
 *
 * No cps C++ class is instantiated anywhere in this example, so it is ABI-portable
 * (no compiler/STL compatibility requirement vs. how cps itself was built).
 *
 * Threading:
 *  - the C callbacks fire on cps's internal worker thread; handleReadyRead/handleError
 *    read the bytes there and emit signals, which Qt auto-queues to the GUI thread;
 *  - connectPort() runs the (potentially blocking) open on a background std::thread
 *    because, on Android, cps requests USB permission synchronously inside open().
 */
class CabiController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString portName READ portName NOTIFY connectedChanged)

public:
    explicit CabiController(QObject* parent = nullptr);
    ~CabiController() override;

    bool connected() const { return connected_.load(); }
    QString portName() const { return connectedPort_; }

    Q_INVOKABLE void connectPort(const QString& portName, int baudRate = 115200);
    Q_INVOKABLE void disconnectPort();
    Q_INVOKABLE qint64 send(const QString& text);
    Q_INVOKABLE void clear();

    // cps invokes these (from its worker thread) via the extern "C" trampolines
    // in the .cpp. Public so the free-function trampolines can reach them.
    void handleReadyRead();
    void handleError(cps_SerialError err);

signals:
    void connectedChanged(bool connected);
    void dataReceived(const QString& data);
    void errorOccurred(const QString& message);

private:
    cps_SerialPort* port_ = nullptr; // opaque C handle from cps_serial_new()
    std::atomic<bool> connected_{false};
    QString connectedPort_;
    std::thread connectThread_;
    std::mutex openMx_; // serializes open()/close()
};

#endif // CABICONTROLLER_H
