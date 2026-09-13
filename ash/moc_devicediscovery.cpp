/****************************************************************************
** Meta object code from reading C++ file 'devicediscovery.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../acp/devicediscovery.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QSharedPointer>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'devicediscovery.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN15DeviceDiscoveryE_t {};
} // unnamed namespace

template <> constexpr inline auto DeviceDiscovery::qt_create_metaobjectdata<qt_meta_tag_ZN15DeviceDiscoveryE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "DeviceDiscovery",
        "hostAliveSignal",
        "",
        "QSharedPointer<AmigaHost>",
        "hostDiedSignal",
        "onScanTimerTimeoutSlot",
        "onSocketReadReadySlot"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'hostAliveSignal'
        QtMocHelpers::SignalData<void(QSharedPointer<AmigaHost>)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 2 },
        }}),
        // Signal 'hostDiedSignal'
        QtMocHelpers::SignalData<void(QSharedPointer<AmigaHost>)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 2 },
        }}),
        // Slot 'onScanTimerTimeoutSlot'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onSocketReadReadySlot'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<DeviceDiscovery, qt_meta_tag_ZN15DeviceDiscoveryE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject DeviceDiscovery::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15DeviceDiscoveryE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15DeviceDiscoveryE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15DeviceDiscoveryE_t>.metaTypes,
    nullptr
} };

void DeviceDiscovery::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DeviceDiscovery *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->hostAliveSignal((*reinterpret_cast<std::add_pointer_t<QSharedPointer<AmigaHost>>>(_a[1]))); break;
        case 1: _t->hostDiedSignal((*reinterpret_cast<std::add_pointer_t<QSharedPointer<AmigaHost>>>(_a[1]))); break;
        case 2: _t->onScanTimerTimeoutSlot(); break;
        case 3: _t->onSocketReadReadySlot(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QSharedPointer<AmigaHost> >(); break;
            }
            break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QSharedPointer<AmigaHost> >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (DeviceDiscovery::*)(QSharedPointer<AmigaHost> )>(_a, &DeviceDiscovery::hostAliveSignal, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (DeviceDiscovery::*)(QSharedPointer<AmigaHost> )>(_a, &DeviceDiscovery::hostDiedSignal, 1))
            return;
    }
}

const QMetaObject *DeviceDiscovery::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DeviceDiscovery::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15DeviceDiscoveryE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int DeviceDiscovery::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void DeviceDiscovery::hostAliveSignal(QSharedPointer<AmigaHost> _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void DeviceDiscovery::hostDiedSignal(QSharedPointer<AmigaHost> _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}
QT_WARNING_POP
