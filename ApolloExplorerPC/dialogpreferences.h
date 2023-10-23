#ifndef DIALOGPREFERENCES_H
#define DIALOGPREFERENCES_H

#include <QDialog>
#include <QSharedPointer>
#include <QSettings>
#include <QTableWidget>

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
    void onDefaultSortCombobBoxChangedSlot( const QString selection );
    void onIgnoreCaseCheckBoxClickedSlot( const bool checked );

    //Icon Size
    void onIconSizeSliderChangedSlot( quint32 value );
    void onDownloadAmigaIconCheckboxSlot( bool selected );

private:
    Ui::DialogPreferences *ui;


private:
    QSharedPointer<QSettings> m_Settings;
    QTableWidgetItem *m_TableItem1;
    QTableWidgetItem *m_TableItem2;
};

#endif // DIALOGPREFERENCES_H
