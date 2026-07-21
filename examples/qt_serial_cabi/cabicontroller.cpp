#include "cabicontroller.h"

#include <QByteArray>
#include <QDateTime>
#include <QMetaObject>

// cps_Callbacks are declared inside cps.h's `extern "C"` block, so the function
// pointer types carry C language linkage -> the trampolines must be extern "C".
extern "C" {
static void cps_cabi_ready_read(void* user) {
    static_cast<CabiController*>(user)->handleReadyRead();
}
static void cps_cabi_error(void* user, cps_SerialError err) {
    static_cast<CabiController*>(user)->handleError(err);
}
} // extern "C"

CabiController::CabiController(QObject* parent) : QObject(parent) {
    port_ = cps_serial_new(nullptr);
    cps_Callbacks cb{};
    cb.on_ready_read    = &cps_cabi_ready_read;
    cb.on_bytes_written = nullptr;
    cb.on_error         = &cps_cabi_error;
    cps_serial_set_callbacks(port_, &cb, this);
}

CabiController::~CabiController() {
    disconnectPort();
    if (connectThread_.joinable())
        connectThread_.join();
    if (port_)
        cps_serial_free(port_);
}

void CabiController::connectPort(const QString& portName, int baudRate) {
    if (connected_.load())
        disconnectPort();
    if (connectThread_.joinable())
        connectThread_.join();

    const QByteArray name = portName.toUtf8();
    const int baud = baudRate;

    connectThread_ = std::thread([this, name, baud]() {
        std::lock_guard<std::mutex> lk(openMx_);

        cps_serial_set_port_name(port_, name.constData());
        cps_serial_set_baud_rate(port_, baud, CPS_DIR_ALL);
        cps_serial_set_data_bits(port_, CPS_DATA_8);
        cps_serial_set_parity(port_, CPS_PAR_NONE);
        cps_serial_set_stop_bits(port_, CPS_STOP_ONE);
        cps_serial_set_flow_control(port_, CPS_FLOW_NONE);
        cps_serial_clear_error(port_);

        // On Android this blocks while cps requests USB permission (handled by the
        // system on the GUI thread Looper, which stays free because we are here).
        const cps_SerialError e = cps_serial_open(port_, CPS_MODE_READWRITE);

        QMetaObject::invokeMethod(this, [this, name, e]() {
            if (e == CPS_ERR_NONE) {
                connectedPort_ = QString::fromUtf8(name);
                connected_ = true;
                emit connectedChanged(true);
            } else {
                emit errorOccurred(QStringLiteral("Failed to open %1: %2")
                                       .arg(QString::fromUtf8(name),
                                            QString::fromUtf8(cps_error_string(e))));
            }
        }, Qt::QueuedConnection);
    });
}

void CabiController::disconnectPort() {
    if (connectThread_.joinable())
        connectThread_.join();
    std::lock_guard<std::mutex> lk(openMx_);
    if (!connected_.load())
        return;
    cps_serial_close(port_); // joins the cps worker thread -> no more callbacks
    connected_ = false;
    connectedPort_.clear();
    emit connectedChanged(false);
}

qint64 CabiController::send(const QString& text) {
    if (!connected_.load())
        return -1;
    const QByteArray bytes = text.toUtf8();
    return static_cast<qint64>(cps_serial_write(port_, bytes.constData(),
                                                static_cast<long>(bytes.size())));
}

void CabiController::clear() {
    std::lock_guard<std::mutex> lk(openMx_);
    if (connected_.load())
        cps_serial_clear(port_, CPS_DIR_ALL); // flush the port's RX/TX buffers
}

void CabiController::handleReadyRead() {
    // cps worker thread: drain whatever arrived, forward as a string to the GUI.
    unsigned char buf[4096];
    const long n = cps_serial_read(port_, buf, sizeof(buf));
    if (n <= 0)
        return;
    // fromLatin1 maps every byte 1:1 -> no data loss for arbitrary/binary input.
    const QString text = QString::fromLatin1(reinterpret_cast<const char*>(buf),
                                             static_cast<int>(n));
    const QString line = QStringLiteral("[%1] (%2 bytes): %3\n")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")))
        .arg(n)
        .arg(text);
    emit dataReceived(line); // auto-queued to the GUI thread
}

void CabiController::handleError(cps_SerialError err) {
    if (err == CPS_ERR_NONE)
        return;
    const QString msg = QString::fromUtf8(cps_error_string(err));
    // Marshal to the GUI thread: close() joins the worker, so it must NOT run here.
    QMetaObject::invokeMethod(this, [this, msg]() {
        emit errorOccurred(msg);
        std::lock_guard<std::mutex> lk(openMx_);
        if (connected_.load()) {
            cps_serial_close(port_);
            connected_ = false;
            connectedPort_.clear();
            emit connectedChanged(false);
        }
    }, Qt::QueuedConnection);
}
