#ifndef DIALOGADDHOST_H
#define DIALOGADDHOST_H

#include "amigahost.h"
#include <QDialog>

namespace Ui {
class DialogAddHost;
}

class DialogAddHost : public QDialog
{
    Q_OBJECT

public:
    explicit DialogAddHost(QWidget *parent = nullptr);
    ~DialogAddHost();

    QSharedPointer<AmigaHost> getAmigaHost();

    private slots:
    void onHardwareSelectedSlot( int index );

private:
    Ui::DialogAddHost *ui;
};

#endif // DIALOGADDHOST_H
