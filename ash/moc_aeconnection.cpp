/****************************************************************************
** Meta object code from reading C++ file 'aeconnection.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../acp/aeconnection.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'aeconnection.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN12AEConnectionE_t {};
} // unnamed namespace

template <> constexpr inline auto AEConnection::qt_create_metaobjectdata<qt_meta_tag_ZN12AEConnectionE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "AEConnection",
        "connectedToHostSignal",
        "",
        "disconnectedFromHostSignal",
        "outgoingByteCountSignal",
        "bytes",
        "incomingByteCountSignal",
        "newMessageReceived",
        "ProtocolMessage_t*",
        "message",
        "rawIncomingBytesSignal",
        "byte",
        "onConnectToHostRequestedSlot",
        "QHostAddress",
        "serverAddress",
        "port",
        "onDisconnectFromhostRequestedSlot",
        "onErrorSlot",
        "QAbstractSocket::SocketError",
        "error",
        "onSendMessage",
        "onConnectedSlot",
        "onDisconnectedSlot",
        "onReadReadySlot",
        "onThroughputTimerExpiredSlot",
        "onSetRawSocketMode",
        "onRawOutgoingBytesSlot"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'connectedToHostSignal'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'disconnectedFromHostSignal'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'outgoingByteCountSignal'
        QtMocHelpers::SignalData<void(quint32)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 5 },
        }}),
        // Signal 'incomingByteCountSignal'
        QtMocHelpers::SignalData<void(quint32)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 5 },
        }}),
        // Signal 'newMessageReceived'
        QtMocHelpers::SignalData<void(ProtocolMessage_t *)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 8, 9 },
        }}),
        // Signal 'rawIncomingBytesSignal'
        QtMocHelpers::SignalData<void(QByteArray)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QByteArray, 11 },
        }}),
        // Slot 'onConnectToHostRequestedSlot'
        QtMocHelpers::SlotData<void(QHostAddress, quint16)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 13, 14 }, { QMetaType::UShort, 15 },
        }}),
        // Slot 'onDisconnectFromhostRequestedSlot'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onErrorSlot'
        QtMocHelpers::SlotData<void(QAbstractSocket::SocketError)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 18, 19 },
        }}),
        // Slot 'onSendMessage'
        QtMocHelpers::SlotData<void(ProtocolMessage_t *)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 8, 9 },
        }}),
        // Slot 'onConnectedSlot'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onDisconnectedSlot'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onReadReadySlot'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onThroughputTimerExpiredSlot'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onSetRawSocketMode'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onRawOutgoingBytesSlot'
        QtMocHelpers::SlotData<void(QByteArray)>(26, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QByteArray, 5 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<AEConnection, qt_meta_tag_ZN12AEConnectionE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject AEConnection::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12AEConnectionE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12AEConnectionE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN12AEConnectionE_t>.metaTypes,
    nullptr
} };

void AEConnection::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AEConnection *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->connectedToHostSignal(); break;
        case 1: _t->disconnectedFromHostSignal(); break;
        case 2: _t->outgoingByteCountSignal((*reinterpret_cast<std::add_pointer_t<quint32>>(_a[1]))); break;
        case 3: _t->incomingByteCountSignal((*reinterpret_cast<std::add_pointer_t<quint32>>(_a[1]))); break;
        case 4: _t->newMessageReceived((*reinterpret_cast<std::add_pointer_t<ProtocolMessage_t*>>(_a[1]))); break;
        case 5: _t->rawIncomingBytesSignal((*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[1]))); break;
        case 6: _t->onConnectToHostRequestedSlot((*reinterpret_cast<std::add_pointer_t<QHostAddress>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<quint16>>(_a[2]))); break;
        case 7: _t->onDisconnectFromhostRequestedSlot(); break;
        case 8: _t->onErrorSlot((*reinterpret_cast<std::add_pointer_t<QAbstractSocket::SocketError>>(_a[1]))); break;
        case 9: _t->onSendMessage((*reinterpret_cast<std::add_pointer_t<ProtocolMessage_t*>>(_a[1]))); break;
        case 10: _t->onConnectedSlot(); break;
        case 11: _t->onDisconnectedSlot(); break;
        case 12: _t->onReadReadySlot(); break;
        case 13: _t->onThroughputTimerExpiredSlot(); break;
        case 14: _t->onSetRawSocketMode(); break;
        case 15: _t->onRawOutgoingBytesSlot((*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 8:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAbstractSocket::SocketError >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AEConnection::*)()>(_a, &AEConnection::connectedToHostSignal, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (AEConnection::*)()>(_a, &AEConnection::disconnectedFromHostSignal, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (AEConnection::*)(quint32 )>(_a, &AEConnection::outgoingByteCountSignal, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (AEConnection::*)(quint32 )>(_a, &AEConnection::incomingByteCountSignal, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (AEConnection::*)(ProtocolMessage_t * )>(_a, &AEConnection::newMessageReceived, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (AEConnection::*)(QByteArray )>(_a, &AEConnection::rawIncomingBytesSignal, 5))
            return;
    }
}

const QMetaObject *AEConnection::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AEConnection::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12AEConnectionE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int AEConnection::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 16)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 16;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 16)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 16;
    }
    return _id;
}

// SIGNAL 0
void AEConnection::connectedToHostSignal()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void AEConnection::disconnectedFromHostSignal()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void AEConnection::outgoingByteCountSignal(quint32 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void AEConnection::incomingByteCountSignal(quint32 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void AEConnection::newMessageReceived(ProtocolMessage_t * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void AEConnection::rawIncomingBytesSignal(QByteArray _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}
QT_WARNING_POP
