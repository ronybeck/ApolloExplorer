/****************************************************************************
** Meta object code from reading C++ file 'protocolhandler.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../acp/protocolhandler.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QSharedPointer>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'protocolhandler.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15ProtocolHandlerE_t {};
} // unnamed namespace

template <> constexpr inline auto ProtocolHandler::qt_create_metaobjectdata<qt_meta_tag_ZN15ProtocolHandlerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ProtocolHandler",
        "connectedToHostSignal",
        "",
        "disconnectedFromHostSignal",
        "outgoingByteCountSignal",
        "bytes",
        "incomingByteCountSignal",
        "serverClosedConnectionSignal",
        "reason",
        "serverVersionSignal",
        "major",
        "minor",
        "revision",
        "newDirectoryListingSignal",
        "QSharedPointer<DirectoryListing>",
        "newListing",
        "acknowledgeWithCodeSignal",
        "responseCode",
        "acknowledgeSignal",
        "failedWithReasonSignal",
        "failedSignal",
        "startOfFileSendSignal",
        "fileSize",
        "numberOfChunks",
        "filename",
        "fileChunkSignal",
        "chunkNumber",
        "chunk",
        "volumeListSignal",
        "QList<QSharedPointer<DiskVolume>>",
        "volumes",
        "fileChunkReceivedSignal",
        "fileReceivedSignal",
        "bytesWrittenToDisk",
        "fileDeletedSignal",
        "path",
        "fileDeleteFailedSignal",
        "DeleteFailureReason",
        "reasonCode",
        "recursiveDeletionCompletedSignal",
        "shellOutputSignal",
        "bytesContained",
        "data",
        "shellDoneSignal",
        "returnCode",
        "connectToHostSignal",
        "QHostAddress",
        "server",
        "port",
        "disconnectFromHostSignal",
        "sendProtocolMessageSignal",
        "ProtocolMessage_t*",
        "message",
        "rawIncomingBytesSignal",
        "rawOutgoingBytesSignal",
        "onConnectToHostRequestedSlot",
        "serverAddress",
        "onDisconnectFromHostRequestedSlot",
        "onRunCMDSlot",
        "command",
        "workingDirectroy",
        "onGetDirectorySlot",
        "remoteDirectory",
        "onMKDirSlot",
        "onRenameFileSlot",
        "oldPathName",
        "newPathName",
        "onSendMessageSlot",
        "onSendAndReleaseMessageSlot",
        "onDeleteFileSlot",
        "remotePath",
        "onDeleteRecursiveSlot",
        "onCancelDeleteDirectorySlot",
        "onGetVolumeListSlot",
        "onConnectedSlot",
        "onDisconnectedSlot",
        "onMessageReceivedSlot",
        "newMessage",
        "onRawOutgoingBytesSlot",
        "onRawIncomingBytesSlot"
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
        // Signal 'serverClosedConnectionSignal'
        QtMocHelpers::SignalData<void(QString)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 8 },
        }}),
        // Signal 'serverVersionSignal'
        QtMocHelpers::SignalData<void(quint8, quint8, quint8)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UChar, 10 }, { QMetaType::UChar, 11 }, { QMetaType::UChar, 12 },
        }}),
        // Signal 'newDirectoryListingSignal'
        QtMocHelpers::SignalData<void(QSharedPointer<DirectoryListing>)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 14, 15 },
        }}),
        // Signal 'acknowledgeWithCodeSignal'
        QtMocHelpers::SignalData<void(quint8)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UChar, 17 },
        }}),
        // Signal 'acknowledgeSignal'
        QtMocHelpers::SignalData<void()>(18, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'failedWithReasonSignal'
        QtMocHelpers::SignalData<void(QString)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 8 },
        }}),
        // Signal 'failedSignal'
        QtMocHelpers::SignalData<void()>(20, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'startOfFileSendSignal'
        QtMocHelpers::SignalData<void(quint64, quint32, QString)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::ULongLong, 22 }, { QMetaType::UInt, 23 }, { QMetaType::QString, 24 },
        }}),
        // Signal 'fileChunkSignal'
        QtMocHelpers::SignalData<void(quint32, quint32, QByteArray)>(25, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 26 }, { QMetaType::UInt, 5 }, { QMetaType::QByteArray, 27 },
        }}),
        // Signal 'volumeListSignal'
        QtMocHelpers::SignalData<void(QList<QSharedPointer<DiskVolume>>)>(28, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 29, 30 },
        }}),
        // Signal 'fileChunkReceivedSignal'
        QtMocHelpers::SignalData<void(quint32)>(31, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 26 },
        }}),
        // Signal 'fileReceivedSignal'
        QtMocHelpers::SignalData<void(quint32)>(32, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 33 },
        }}),
        // Signal 'fileDeletedSignal'
        QtMocHelpers::SignalData<void(QString)>(34, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 35 },
        }}),
        // Signal 'fileDeleteFailedSignal'
        QtMocHelpers::SignalData<void(QString, DeleteFailureReason)>(36, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 35 }, { 0x80000000 | 37, 38 },
        }}),
        // Signal 'recursiveDeletionCompletedSignal'
        QtMocHelpers::SignalData<void()>(39, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'shellOutputSignal'
        QtMocHelpers::SignalData<void(quint32, QByteArray)>(40, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::UInt, 41 }, { QMetaType::QByteArray, 42 },
        }}),
        // Signal 'shellDoneSignal'
        QtMocHelpers::SignalData<void(int)>(43, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 44 },
        }}),
        // Signal 'connectToHostSignal'
        QtMocHelpers::SignalData<void(QHostAddress, quint16)>(45, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 46, 47 }, { QMetaType::UShort, 48 },
        }}),
        // Signal 'disconnectFromHostSignal'
        QtMocHelpers::SignalData<void()>(49, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'sendProtocolMessageSignal'
        QtMocHelpers::SignalData<void(ProtocolMessage_t *)>(50, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 51, 52 },
        }}),
        // Signal 'rawIncomingBytesSignal'
        QtMocHelpers::SignalData<void(QByteArray)>(53, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QByteArray, 5 },
        }}),
        // Signal 'rawOutgoingBytesSignal'
        QtMocHelpers::SignalData<void(QByteArray)>(54, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QByteArray, 5 },
        }}),
        // Slot 'onConnectToHostRequestedSlot'
        QtMocHelpers::SlotData<void(QHostAddress, quint16)>(55, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 46, 56 }, { QMetaType::UShort, 48 },
        }}),
        // Slot 'onDisconnectFromHostRequestedSlot'
        QtMocHelpers::SlotData<void()>(57, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onRunCMDSlot'
        QtMocHelpers::SlotData<void(QString, QString)>(58, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 59 }, { QMetaType::QString, 60 },
        }}),
        // Slot 'onGetDirectorySlot'
        QtMocHelpers::SlotData<void(QString)>(61, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 62 },
        }}),
        // Slot 'onMKDirSlot'
        QtMocHelpers::SlotData<void(QString)>(63, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 62 },
        }}),
        // Slot 'onRenameFileSlot'
        QtMocHelpers::SlotData<void(QString, QString)>(64, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 65 }, { QMetaType::QString, 66 },
        }}),
        // Slot 'onSendMessageSlot'
        QtMocHelpers::SlotData<void(ProtocolMessage_t *)>(67, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 51, 52 },
        }}),
        // Slot 'onSendAndReleaseMessageSlot'
        QtMocHelpers::SlotData<void(ProtocolMessage_t *)>(68, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 51, 52 },
        }}),
        // Slot 'onDeleteFileSlot'
        QtMocHelpers::SlotData<void(QString)>(69, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 70 },
        }}),
        // Slot 'onDeleteRecursiveSlot'
        QtMocHelpers::SlotData<void(QString)>(71, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 70 },
        }}),
        // Slot 'onCancelDeleteDirectorySlot'
        QtMocHelpers::SlotData<void()>(72, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onGetVolumeListSlot'
        QtMocHelpers::SlotData<void()>(73, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onConnectedSlot'
        QtMocHelpers::SlotData<void()>(74, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onDisconnectedSlot'
        QtMocHelpers::SlotData<void()>(75, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onMessageReceivedSlot'
        QtMocHelpers::SlotData<void(ProtocolMessage_t *)>(76, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 51, 77 },
        }}),
        // Slot 'onRawOutgoingBytesSlot'
        QtMocHelpers::SlotData<void(QByteArray)>(78, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QByteArray, 5 },
        }}),
        // Slot 'onRawIncomingBytesSlot'
        QtMocHelpers::SlotData<void(QByteArray)>(79, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QByteArray, 5 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ProtocolHandler, qt_meta_tag_ZN15ProtocolHandlerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ProtocolHandler::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15ProtocolHandlerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15ProtocolHandlerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15ProtocolHandlerE_t>.metaTypes,
    nullptr
} };

void ProtocolHandler::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ProtocolHandler *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->connectedToHostSignal(); break;
        case 1: _t->disconnectedFromHostSignal(); break;
        case 2: _t->outgoingByteCountSignal((*reinterpret_cast<std::add_pointer_t<quint32>>(_a[1]))); break;
        case 3: _t->incomingByteCountSignal((*reinterpret_cast<std::add_pointer_t<quint32>>(_a[1]))); break;
        case 4: _t->serverClosedConnectionSignal((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->serverVersionSignal((*reinterpret_cast<std::add_pointer_t<quint8>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<quint8>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<quint8>>(_a[3]))); break;
        case 6: _t->newDirectoryListingSignal((*reinterpret_cast<std::add_pointer_t<QSharedPointer<DirectoryListing>>>(_a[1]))); break;
        case 7: _t->acknowledgeWithCodeSignal((*reinterpret_cast<std::add_pointer_t<quint8>>(_a[1]))); break;
        case 8: _t->acknowledgeSignal(); break;
        case 9: _t->failedWithReasonSignal((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 10: _t->failedSignal(); break;
        case 11: _t->startOfFileSendSignal((*reinterpret_cast<std::add_pointer_t<quint64>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<quint32>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 12: _t->fileChunkSignal((*reinterpret_cast<std::add_pointer_t<quint32>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<quint32>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[3]))); break;
        case 13: _t->volumeListSignal((*reinterpret_cast<std::add_pointer_t<QList<QSharedPointer<DiskVolume>>>>(_a[1]))); break;
        case 14: _t->fileChunkReceivedSignal((*reinterpret_cast<std::add_pointer_t<quint32>>(_a[1]))); break;
        case 15: _t->fileReceivedSignal((*reinterpret_cast<std::add_pointer_t<quint32>>(_a[1]))); break;
        case 16: _t->fileDeletedSignal((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 17: _t->fileDeleteFailedSignal((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<DeleteFailureReason>>(_a[2]))); break;
        case 18: _t->recursiveDeletionCompletedSignal(); break;
        case 19: _t->shellOutputSignal((*reinterpret_cast<std::add_pointer_t<quint32>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[2]))); break;
        case 20: _t->shellDoneSignal((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 21: _t->connectToHostSignal((*reinterpret_cast<std::add_pointer_t<QHostAddress>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<quint16>>(_a[2]))); break;
        case 22: _t->disconnectFromHostSignal(); break;
        case 23: _t->sendProtocolMessageSignal((*reinterpret_cast<std::add_pointer_t<ProtocolMessage_t*>>(_a[1]))); break;
        case 24: _t->rawIncomingBytesSignal((*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[1]))); break;
        case 25: _t->rawOutgoingBytesSignal((*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[1]))); break;
        case 26: _t->onConnectToHostRequestedSlot((*reinterpret_cast<std::add_pointer_t<QHostAddress>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<quint16>>(_a[2]))); break;
        case 27: _t->onDisconnectFromHostRequestedSlot(); break;
        case 28: _t->onRunCMDSlot((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 29: _t->onGetDirectorySlot((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 30: _t->onMKDirSlot((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 31: _t->onRenameFileSlot((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 32: _t->onSendMessageSlot((*reinterpret_cast<std::add_pointer_t<ProtocolMessage_t*>>(_a[1]))); break;
        case 33: _t->onSendAndReleaseMessageSlot((*reinterpret_cast<std::add_pointer_t<ProtocolMessage_t*>>(_a[1]))); break;
        case 34: _t->onDeleteFileSlot((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 35: _t->onDeleteRecursiveSlot((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 36: _t->onCancelDeleteDirectorySlot(); break;
        case 37: _t->onGetVolumeListSlot(); break;
        case 38: _t->onConnectedSlot(); break;
        case 39: _t->onDisconnectedSlot(); break;
        case 40: _t->onMessageReceivedSlot((*reinterpret_cast<std::add_pointer_t<ProtocolMessage_t*>>(_a[1]))); break;
        case 41: _t->onRawOutgoingBytesSlot((*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[1]))); break;
        case 42: _t->onRawIncomingBytesSlot((*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 6:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QSharedPointer<DirectoryListing> >(); break;
            }
            break;
        case 13:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<QSharedPointer<DiskVolume>> >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)()>(_a, &ProtocolHandler::connectedToHostSignal, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)()>(_a, &ProtocolHandler::disconnectedFromHostSignal, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(quint32 )>(_a, &ProtocolHandler::outgoingByteCountSignal, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(quint32 )>(_a, &ProtocolHandler::incomingByteCountSignal, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(QString )>(_a, &ProtocolHandler::serverClosedConnectionSignal, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(quint8 , quint8 , quint8 )>(_a, &ProtocolHandler::serverVersionSignal, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(QSharedPointer<DirectoryListing> )>(_a, &ProtocolHandler::newDirectoryListingSignal, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(quint8 )>(_a, &ProtocolHandler::acknowledgeWithCodeSignal, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)()>(_a, &ProtocolHandler::acknowledgeSignal, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(QString )>(_a, &ProtocolHandler::failedWithReasonSignal, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)()>(_a, &ProtocolHandler::failedSignal, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(quint64 , quint32 , QString )>(_a, &ProtocolHandler::startOfFileSendSignal, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(quint32 , quint32 , QByteArray )>(_a, &ProtocolHandler::fileChunkSignal, 12))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(QList<QSharedPointer<DiskVolume>> )>(_a, &ProtocolHandler::volumeListSignal, 13))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(quint32 )>(_a, &ProtocolHandler::fileChunkReceivedSignal, 14))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(quint32 )>(_a, &ProtocolHandler::fileReceivedSignal, 15))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(QString )>(_a, &ProtocolHandler::fileDeletedSignal, 16))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(QString , DeleteFailureReason )>(_a, &ProtocolHandler::fileDeleteFailedSignal, 17))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)()>(_a, &ProtocolHandler::recursiveDeletionCompletedSignal, 18))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(quint32 , QByteArray )>(_a, &ProtocolHandler::shellOutputSignal, 19))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(int )>(_a, &ProtocolHandler::shellDoneSignal, 20))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(QHostAddress , quint16 )>(_a, &ProtocolHandler::connectToHostSignal, 21))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)()>(_a, &ProtocolHandler::disconnectFromHostSignal, 22))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(ProtocolMessage_t * )>(_a, &ProtocolHandler::sendProtocolMessageSignal, 23))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(QByteArray )>(_a, &ProtocolHandler::rawIncomingBytesSignal, 24))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtocolHandler::*)(QByteArray )>(_a, &ProtocolHandler::rawOutgoingBytesSignal, 25))
            return;
    }
}

const QMetaObject *ProtocolHandler::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ProtocolHandler::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15ProtocolHandlerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ProtocolHandler::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 43)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 43;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 43)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 43;
    }
    return _id;
}

// SIGNAL 0
void ProtocolHandler::connectedToHostSignal()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ProtocolHandler::disconnectedFromHostSignal()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ProtocolHandler::outgoingByteCountSignal(quint32 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void ProtocolHandler::incomingByteCountSignal(quint32 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void ProtocolHandler::serverClosedConnectionSignal(QString _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void ProtocolHandler::serverVersionSignal(quint8 _t1, quint8 _t2, quint8 _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1, _t2, _t3);
}

// SIGNAL 6
void ProtocolHandler::newDirectoryListingSignal(QSharedPointer<DirectoryListing> _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void ProtocolHandler::acknowledgeWithCodeSignal(quint8 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}

// SIGNAL 8
void ProtocolHandler::acknowledgeSignal()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void ProtocolHandler::failedWithReasonSignal(QString _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1);
}

// SIGNAL 10
void ProtocolHandler::failedSignal()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}

// SIGNAL 11
void ProtocolHandler::startOfFileSendSignal(quint64 _t1, quint32 _t2, QString _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 11, nullptr, _t1, _t2, _t3);
}

// SIGNAL 12
void ProtocolHandler::fileChunkSignal(quint32 _t1, quint32 _t2, QByteArray _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 12, nullptr, _t1, _t2, _t3);
}

// SIGNAL 13
void ProtocolHandler::volumeListSignal(QList<QSharedPointer<DiskVolume>> _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 13, nullptr, _t1);
}

// SIGNAL 14
void ProtocolHandler::fileChunkReceivedSignal(quint32 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 14, nullptr, _t1);
}

// SIGNAL 15
void ProtocolHandler::fileReceivedSignal(quint32 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 15, nullptr, _t1);
}

// SIGNAL 16
void ProtocolHandler::fileDeletedSignal(QString _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 16, nullptr, _t1);
}

// SIGNAL 17
void ProtocolHandler::fileDeleteFailedSignal(QString _t1, DeleteFailureReason _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 17, nullptr, _t1, _t2);
}

// SIGNAL 18
void ProtocolHandler::recursiveDeletionCompletedSignal()
{
    QMetaObject::activate(this, &staticMetaObject, 18, nullptr);
}

// SIGNAL 19
void ProtocolHandler::shellOutputSignal(quint32 _t1, QByteArray _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 19, nullptr, _t1, _t2);
}

// SIGNAL 20
void ProtocolHandler::shellDoneSignal(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 20, nullptr, _t1);
}

// SIGNAL 21
void ProtocolHandler::connectToHostSignal(QHostAddress _t1, quint16 _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 21, nullptr, _t1, _t2);
}

// SIGNAL 22
void ProtocolHandler::disconnectFromHostSignal()
{
    QMetaObject::activate(this, &staticMetaObject, 22, nullptr);
}

// SIGNAL 23
void ProtocolHandler::sendProtocolMessageSignal(ProtocolMessage_t * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 23, nullptr, _t1);
}

// SIGNAL 24
void ProtocolHandler::rawIncomingBytesSignal(QByteArray _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 24, nullptr, _t1);
}

// SIGNAL 25
void ProtocolHandler::rawOutgoingBytesSignal(QByteArray _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 25, nullptr, _t1);
}
QT_WARNING_POP
