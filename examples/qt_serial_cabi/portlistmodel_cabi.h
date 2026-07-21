#ifndef PORTLISTMODELCABI_H
#define PORTLISTMODELCABI_H

#include <QAbstractListModel>
#include <QList>
#include <QString>

/**
 * QAbstractListModel populated purely from the C ABI (cps_available_ports /
 * cps_free_ports). Roles: portName, description, manufacturer, vid, pid (plus
 * Qt::DisplayRole == portName). Drives the QML ComboBox (valueRole "portName").
 */
class PortListModelCabi : public QAbstractListModel {
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

    explicit PortListModelCabi(QObject* parent = nullptr);

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

#endif // PORTLISTMODELCABI_H
