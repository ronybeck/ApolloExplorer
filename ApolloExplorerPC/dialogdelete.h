#ifndef DIALOGDELETE_H
#define DIALOGDELETE_H

#include <QDialog>

namespace Ui {
class DialogDelete;
}

class DialogDelete : public QDialog
{
    Q_OBJECT

public:
    explicit DialogDelete(QWidget *parent = nullptr);
    ~DialogDelete();

public slots:
    void onCurrentFileBeingDeletedSlot( QString filepath );
    void onDeletionCompleteSlot();
    void onCancelButtonReleasedSlot();

signals:
    void cancelDeletionSignal();

private:
    Ui::DialogDelete *ui;
};

#endif // DIALOGDELETE_H
