#ifndef PORTLISTMODEL_H
#define PORTLISTMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QString>

/**
 * QAbstractListModel exposing the serial ports found by cps::SerialPortInfo.
 *
 * Roles: portName, description, manufacturer, vid, pid (plus Qt::DisplayRole ==
 * portName). refresh() re-queries the platform and resets the model. Drives the
 * QML ComboBox (valueRole "portName" -> currentValue is the port to open).
 */
class PortListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        PortNameRole = Qt::UserRole,
        DescriptionRole,
        ManufacturerRole,
        VidRole,
        PidRole
    };
    Q_ENUM(Roles)

    explicit PortListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString portNameAt(int index) const;

private:
    struct Entry {
        QString name;
        QString description;
        QString manufacturer;
        quint16 vid = 0;
        quint16 pid = 0;
    };
    QList<Entry> ports_;
};

#endif // PORTLISTMODEL_H
