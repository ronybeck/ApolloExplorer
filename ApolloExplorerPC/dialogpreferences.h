#ifndef DIALOGPREFERENCES_H
#define DIALOGPREFERENCES_H

#include <QDialog>
#include <QSharedPointer>
#include <QSettings>

namespace Ui {
class DialogPreferences;
}

class DialogPreferences : public QDialog
{
    Q_OBJECT

public:
    explicit DialogPreferences( QSharedPointer<QSettings> settings, QWidget *parent = nullptr);
    ~DialogPreferences();

    public slots:
    void onDoubleClickComboBoxChangedSlot( const QString selection );
    void onDelayBetweenDeletesChangedSlot( int newValue );
    void onHelloTimeoutChangedSlot( int newValue );
    void onDNDSizeChangedSlot( int newValue );
    void onDNDOperationComboboxChangedSlot( const QString selection );

private:
    Ui::DialogPreferences *ui;


private:
    QSharedPointer<QSettings> m_Settings;
};

#endif // DIALOGPREFERENCES_H
