#include "portlistmodel.h"

#include <cps/SerialPortInfo.hpp>

PortListModel::PortListModel(QObject* parent) : QAbstractListModel(parent) {}

int PortListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ports_.size();
}

QVariant PortListModel::data(const QModelIndex& index, int role) const {
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

QHash<int, QByteArray> PortListModel::roleNames() const {
    return {
        {Qt::DisplayRole,  "display"},
        {PortNameRole,     "portName"},
        {DescriptionRole,  "description"},
        {ManufacturerRole, "manufacturer"},
        {VidRole,          "vid"},
        {PidRole,          "pid"}
    };
}

void PortListModel::refresh() {
    std::vector<cps::SerialPortInfo> list = cps::SerialPortInfo::availablePorts();
    beginResetModel();
    ports_.clear();
    ports_.reserve(static_cast<int>(list.size()));
    for (const cps::SerialPortInfo& p : list) {
        Entry e;
        e.name         = QString::fromStdString(p.portName());
        e.description  = QString::fromStdString(p.description());
        e.manufacturer = QString::fromStdString(p.manufacturer());
        e.vid          = p.vendorIdentifier();
        e.pid          = p.productIdentifier();
        ports_.append(std::move(e));
    }
    endResetModel();
}

QString PortListModel::portNameAt(int index) const {
    if (index < 0 || index >= ports_.size()) return {};
    return ports_[index].name;
}
