#ifndef VNETCONNECTION_H
#define VNETCONNECTION_H

#include <QObject>
#include <QHostAddress>
#include <QTcpSocket>
#include <QTimer>

#include "protocolTypes.h"

class VNetConnection : public QObject
{
    Q_OBJECT
public:
    explicit VNetConnection(QObject *parent = nullptr);
    ~VNetConnection();
    template<typename T> void sendMessage( T* message )
    {
        onSendMessage( reinterpret_cast< ProtocolMessage_t*>( message ) );
    }

public slots:

    //User Slots
    void onConnectToHostRequestedSlot( QHostAddress serverAddress, quint16 port);
    void onDisconnectFromhostRequestedSlot();
    void onErrorSlot( QAbstractSocket::SocketError error );
    void onSendMessage( ProtocolMessage_t *message );

    //Socket slots
    void onConnectedSlot();
    void onDisconnectedSlot();
    void onReadReadySlot();

    //Timer slots
    void onThroughputTimerExpiredSlot();

    //Raw Socket Mode
    void onSetRawSocketMode();
    void onRawOutgoingBytesSlot( QByteArray bytes );

signals:
    //Network control signals
    void connectedToHostSignal();
    void disconnectedFromHostSignal();
    void outgoingByteCountSignal( quint32 bytes );
    void incomingByteCountSignal( quint32 bytes );

    //Protocol messages
    void newMessageReceived( ProtocolMessage_t *message );

    //Raw Socket Mode
    void rawIncomingBytesSignal( QByteArray byte );

private:
    QTcpSocket m_Socket;
    bool m_RawSocketModeActive;

    //Incoming message book keeping
    ProtocolMessage_t *m_IncomingMessageBuffer;
    quint64 m_TotalBytesRead;
    quint64 m_TotalBytesLeftToRead;

    //Throughput bookkeeping
    quint32 m_OutgoingByteCount;
    quint32 m_IncomgingByteCount;
    QTimer m_ThroughputTimer;


};

#endif // VNETCONNECTION_H
