#ifndef SHELLSESSION_H
#define SHELLSESSION_H

#include <QObject>
#include <QHostAddress>
#include <QList>
#include <QStringList>
#include "protocolhandler.h"

class ShellSession : public QObject
{
    Q_OBJECT

public:
    explicit ShellSession( QString ipAddressString, QObject *parent = nullptr );

    bool connectToHost();
    void disconnectFromHost();
    int execute( const QString &command );
    QStringList requestCompletion( const QString &partialToken );
    QString currentDir() const { return m_CurrentDir; }

private slots:
    void onShellOutputSlot( quint32 bytesContained, QByteArray data );
    void onShellDoneSlot( int returnCode, QString currentDir );
    void onShellCompleteRspSlot( QStringList completions );

private:
    QString m_Host;
    ProtocolHandler m_ProtocolHandler;
    bool m_CommandDone;
    int m_ReturnCode;
    QString m_CurrentDir;
    QList<QByteArray> m_PendingOutput;
    bool m_CompletionDone;
    QStringList m_Completions;
};

#endif // SHELLSESSION_H
