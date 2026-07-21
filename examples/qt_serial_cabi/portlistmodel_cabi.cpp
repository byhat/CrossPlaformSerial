#include "portlistmodel_cabi.h"

#include <cps/cps.h>

PortListModelCabi::PortListModelCabi(QObject* parent) : QAbstractListModel(parent) {}

int PortListModelCabi::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ports_.size();
}

QVariant PortListModelCabi::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= ports_.size())
        return {};
    const Entry& e = ports_[index.row()];
    switch (role) {
        case Qt::DisplayRole:
        case PortNameRole:     return e.name;
        case DescriptionRole:  return e.description;
        case ManufacturerRole: return e.manufacturer;
        case VidRole:          return e.vid;
        case PidRole:          return e.pid;
    }
    return {};
}

QHash<int, QByteArray> PortListModelCabi::roleNames() const {
    return {
        {Qt::DisplayRole,  "display"},
        {PortNameRole,     "portName"},
        {DescriptionRole,  "description"},
        {ManufacturerRole, "manufacturer"},
        {VidRole,          "vid"},
        {PidRole,          "pid"}
    };
}

static QString safeUtf8(const char* s) {
    return QString::fromUtf8(s ? s : "");
}

void PortListModelCabi::refresh() {
    int count = 0;
    cps_PortInfo* arr = cps_available_ports(&count);

    beginResetModel();
    ports_.clear();
    for (int i = 0; i < count; ++i) {
        const cps_PortInfo& p = arr[i];
        Entry e;
        e.name         = safeUtf8(p.port_name);
        e.description  = safeUtf8(p.description);
        e.manufacturer = safeUtf8(p.manufacturer);
        e.vid          = p.vendor_id;
        e.pid          = p.product_id;
        ports_.append(std::move(e));
    }
    endResetModel();

    if (arr)
        cps_free_ports(arr); // frees the C-allocated array and its strings
}

QString PortListModelCabi::portNameAt(int index) const {
    if (index < 0 || index >= ports_.size())
        return {};
    return ports_[index].name;
}
