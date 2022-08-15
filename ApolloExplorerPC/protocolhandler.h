#ifndef PROTOCOLHANDLER_H
#define PROTOCOLHANDLER_H

#include <QObject>
#include <QHostAddress>
#include <QTcpSocket>

#include "protocolTypes.h"
#include "aeconnection.h"
#include "directorylisting.h"
#include "diskvolume.h"

class ProtocolHandler : public QObject
{
    Q_OBJECT

public:
    typedef enum
    {
        CP_DISCONNECTED,
        CP_CONNECTING,
        CP_RECONNECTING,
        CP_CONNECTED
    } ConnectionPhase_t;

    typedef enum
    {
        AS_Unknown,
        AS_SUCCESS,
        AS_FAILED,
    } AcknowledgeState;


public:
    explicit ProtocolHandler(QObject *parent = nullptr);
    template<typename T> void sendMessage( T* message )
    {
        onSendMessageSlot( reinterpret_cast< ProtocolMessage_t*>( message ) );
    }

    /**
     * @brief deleteFile Delete a remote file
     * @param remoteFilePath The path of the remote file
     * @param error An error message in case the file could not be deleted
     * @return Returns true on success and false in all other cases.
     */
    bool deleteFile( QString remoteFilePath, QString &error );

public slots:
    //User Slots
    void onConnectToHostRequestedSlot( QHostAddress serverAddress, quint16 port);
    void onDisconnectFromHostRequestedSlot();
    void onRunCMDSlot( QString command, QString workingDirectroy );
    void onGetDirectorySlot( QString remoteDirectory );
    void onMKDirSlot( QString remoteDirectory );
    void onRenameFileSlot( QString oldPathName, QString newPathName );
    void onSendMessageSlot( ProtocolMessage_t *message );
    void onSendAndReleaseMessageSlot( ProtocolMessage_t *message );
    void onDeleteFileSlot( QString remotePath );
    void onDeleteRecursiveSlot( QString remotePath );
    void onCancelDeleteDirectorySlot();

    //Connection slots
    void onConnectedSlot();
    void onDisconnectedSlot();
    void onMessageReceivedSlot( ProtocolMessage_t *newMessage );

    //Raw Socket Mode
    void onRawOutgoingBytesSlot( QByteArray bytes );
    void onRawIncomingBytesSlot( QByteArray bytes );

signals:

    //Network connection signals
    void connectedToHostSignal();
    void disconnectedFromHostSignal();
    void outgoingByteCountSignal( quint32 bytes );
    void incomingByteCountSignal( quint32 bytes );
    void serverClosedConnectionSignal( QString reason );

    //Protocol messages
    void serverVersionSignal( quint8 major, quint8 minor, quint8 revision );
    void newDirectoryListingSignal( QSharedPointer<DirectoryListing> newListing );
    void acknowledgeWithCodeSignal( quint8 responseCode );
    void acknowledgeSignal();
    void failedWithReasonSignal( QString reason );
    void failedSignal();
    void startOfFileSendSignal( quint64 fileSize, quint32 numberOfChunks, QString filename );
    void fileChunkSignal( quint32 chunkNumber, quint32 bytes, QByteArray chunk );
    void volumeListSignal( QList<QSharedPointer<DiskVolume>> volumes );
    void fileChunkReceivedSignal( quint32 chunkNumber );
    void fileReceivedSignal( quint32 bytesWrittenToDisk );
    void fileDeletedSignal( QString path );
    void fileDeleteFailedSignal( QString path, DeleteFailureReason reasonCode );
    void recursiveDeletionCompletedSignal();


    //Requests to the vconnection object
    void connectToHostSignal( QHostAddress server, quint16 port );
    void disconnectFromHostSignal();
    void sendProtocolMessageSignal( ProtocolMessage_t *message );


    //Raw Socket Mode
    void rawIncomingBytesSignal( QByteArray bytes );
    void rawOutgoingBytesSignal( QByteArray bytes );

private:
    AEConnection m_AEConnection;
    QHostAddress m_ServerAddress;
    quint16 m_ServerPort;

    //protocol book keeping
    ConnectionPhase_t m_ConnectionPhase;
};

#endif // PROTOCOLHANDLER_H
