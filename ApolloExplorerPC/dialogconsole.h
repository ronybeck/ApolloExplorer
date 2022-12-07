#ifndef DIALOGCONSOLE_H
#define DIALOGCONSOLE_H

#include <QDialog>
#include <QHostAddress>

#include "protocolhandler.h"

namespace Ui {
class DialogConsole;
}

class DialogConsole : public QDialog
{
    Q_OBJECT

public:
    explicit DialogConsole( QHostAddress addres, quint16 port, QString command, QString workdingDirectotry, QWidget *parent = nullptr);
    ~DialogConsole();

    virtual bool eventFilter(QObject *object, QEvent *event) override;

public slots:
    void onConnectedSlot();
    void rawIncomingBytesSlot( QByteArray bytes );
    void onTestTimerExpiredSlot();

signals:
    void rawOutgoingBytesSignal( QByteArray bytes );

private:
    Ui::DialogConsole *ui;
    ProtocolHandler m_ProtocolHandler;
    QString m_Command;
    QString m_WorkingDirectroy;
    QTimer m_TestTimer;
};

#endif // DIALOGCONSOLE_H
