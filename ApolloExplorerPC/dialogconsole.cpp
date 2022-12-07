#include "dialogconsole.h"
#include "ui_dialogconsole.h"

#include <QDebug>

DialogConsole::DialogConsole( QHostAddress address, quint16 port, QString command, QString workdingDirectotry, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogConsole),
    m_ProtocolHandler( ),
    m_Command( command ),
    m_WorkingDirectroy( workdingDirectotry )
{
    ui->setupUi(this);
    ui->plainTextEdit->installEventFilter( this );

    connect( &m_ProtocolHandler, &ProtocolHandler::connectedToHostSignal, this, &DialogConsole::onConnectedSlot );
    connect( &m_ProtocolHandler, &ProtocolHandler::rawIncomingBytesSignal, this, &DialogConsole::rawIncomingBytesSlot );
    connect( this, &DialogConsole::rawOutgoingBytesSignal, &m_ProtocolHandler, &ProtocolHandler::onRawOutgoingBytesSlot );
    connect( &m_TestTimer, &QTimer::timeout, this, &DialogConsole::onTestTimerExpiredSlot );

    //Start the test timer
    m_TestTimer.setInterval( 2000 );
    m_TestTimer.setSingleShot( true );
    m_TestTimer.start();

    m_ProtocolHandler.onConnectToHostRequestedSlot( address, port );
}

DialogConsole::~DialogConsole()
{
    delete ui;
}

bool DialogConsole::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return)
        {
            qDebug() << "Key pressed: " << keyEvent->text().toUtf8()[0];
            //emit rawOutgoingBytesSignal( keyEvent->text().toUtf8() );

            //Get the last line typed.
            QStringList lines = ui->plainTextEdit->toPlainText().split( "\n" );
            if( lines.length() )
            {
                QString lastLine = lines.last();
                qDebug() << "Last line is: " << lastLine;
                m_ProtocolHandler.onRunCMDSlot( lastLine, "" );
            }

            return QDialog::eventFilter(object, event);
        }
    }
    return QDialog::eventFilter(object, event);
}

void DialogConsole::onConnectedSlot()
{
    //m_ProtocolHandler.onRunCMDSlot( m_Command, m_WorkingDirectroy );
}

void DialogConsole::rawIncomingBytesSlot( QByteArray bytes )
{
    ui->plainTextEdit->appendPlainText( bytes );
    qDebug() << "Console: " << bytes.toHex();
}

void DialogConsole::onTestTimerExpiredSlot()
{
    //m_ProtocolHandler.onRawOutgoingBytesSlot( "dir\n" );
    //m_ProtocolHandler.onRawIncomingBytesSlot(  QString( (char)(-1) ).toUtf8() );
}
