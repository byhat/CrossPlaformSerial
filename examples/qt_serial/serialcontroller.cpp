#include "serialcontroller.h"

#include <QByteArray>
#include <QDateTime>
#include <QMetaObject>
#include <string>

SerialController::SerialController(QObject* parent) : QObject(parent) {
    // Callbacks are stored now and invoked later from the cps worker thread.
    port_.setCallbacks({
        [this]() { onReadyRead(); },
        {},
        [this](cps::SerialError e) { onCpsError(e); }
    });
}

SerialController::~SerialController() {
    if (connectThread_.joinable())
        connectThread_.join();
}

void SerialController::connectPort(const QString& portName, qint32 baudRate) {
    if (connected_.load())
        disconnectPort();
    if (connectThread_.joinable())
        connectThread_.join();

    // Capture by value; the thread touches port_ (thread-safe) and reports back via
    // a queued invocation on the GUI thread.
    const std::string name = portName.toStdString();
    const qint32 baud = baudRate;

    connectThread_ = std::thread([this, name, baud]() {
        std::lock_guard<std::mutex> lk(openMx_);

        port_.setPortName(name);
        port_.setBaudRate(baud);
        port_.setDataBits(cps::DataBits::Data8);
        port_.setParity(cps::Parity::NoParity);
        port_.setStopBits(cps::StopBits::OneStop);
        port_.setFlowControl(cps::FlowControl::NoFlowControl);
        port_.clearError();

        // On Android this blocks while cps requests USB permission (handled by the
        // system on the GUI thread Looper, which stays free because we are here).
        const bool ok = port_.open(cps::OpenMode::ReadWrite);

        QMetaObject::invokeMethod(this, [this, name, ok]() {
            if (ok) {
                connectedPort_ = QString::fromStdString(name);
                connected_ = true;
                emit connectedChanged(true);
            } else {
                emit errorOccurred(QStringLiteral("Failed to open %1: %2")
                                       .arg(QString::fromStdString(name),
                                            errorToString(port_.error())));
            }
        }, Qt::QueuedConnection);
    });
}

void SerialController::disconnectPort() {
    if (connectThread_.joinable())
        connectThread_.join();
    std::lock_guard<std::mutex> lk(openMx_);
    if (!connected_.load())
        return;
    port_.close(); // joins the cps worker thread -> no more callbacks after this
    connected_ = false;
    connectedPort_.clear();
    emit connectedChanged(false);
}

qint64 SerialController::send(const QString& text) {
    if (!connected_.load())
        return -1;
    const QByteArray bytes = text.toUtf8();
    return port_.write(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

void SerialController::clear() {
    std::lock_guard<std::mutex> lk(openMx_);
    if (connected_.load())
        port_.clear(); // flush the port's RX/TX buffers
}

void SerialController::onReadyRead() {
    // cps worker thread: drain whatever arrived, forward as a string to the GUI.
    std::vector<std::uint8_t> bytes = port_.readAll();
    if (bytes.empty())
        return;
    // fromLatin1 maps every byte 1:1 -> no data loss for arbitrary/binary input.
    const QString text = QString::fromLatin1(
        reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()));
    const QString line = QStringLiteral("[%1] (%2 bytes): %3\n")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")))
        .arg(bytes.size())
        .arg(text);
    emit dataReceived(line); // auto-queued to the GUI thread
}

void SerialController::onCpsError(cps::SerialError e) {
    if (e == cps::SerialError::NoError)
        return;
    const QString msg = errorToString(e);
    // Marshal to the GUI thread: close() joins the worker, so it must NOT run here.
    QMetaObject::invokeMethod(this, [this, msg]() {
        emit errorOccurred(msg);
        std::lock_guard<std::mutex> lk(openMx_);
        if (connected_.load()) {
            port_.close();
            connected_ = false;
            connectedPort_.clear();
            emit connectedChanged(false);
        }
    }, Qt::QueuedConnection);
}

QString SerialController::errorToString(cps::SerialError e) {
    switch (e) {
        case cps::SerialError::NoError:                   return QStringLiteral("no error");
        case cps::SerialError::DeviceNotFoundError:        return QStringLiteral("device not found");
        case cps::SerialError::PermissionError:            return QStringLiteral("permission denied");
        case cps::SerialError::OpenError:                  return QStringLiteral("open error");
        case cps::SerialError::ResourceError:              return QStringLiteral("device lost / resource error");
        case cps::SerialError::WriteError:                 return QStringLiteral("write error");
        case cps::SerialError::ReadError:                  return QStringLiteral("read error");
        case cps::SerialError::TimeoutError:               return QStringLiteral("timeout");
        case cps::SerialError::NotOpenError:               return QStringLiteral("port not open");
        default:                                           return QStringLiteral("serial error");
    }
}
