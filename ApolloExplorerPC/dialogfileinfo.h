#ifndef DIALOGFILEINFO_H
#define DIALOGFILEINFO_H

#include <QDialog>
#include <QSharedPointer>
#include "directorylisting.h"

namespace Ui {
class DialogFileInfo;
}

class DialogFileInfo : public QDialog
{
    Q_OBJECT

public:
    explicit DialogFileInfo(QSharedPointer<DirectoryListing> directoryListing, QWidget *parent = nullptr);
    ~DialogFileInfo();

private:
    Ui::DialogFileInfo *ui;
    QSharedPointer<DirectoryListing> m_DirectoryListing;
};

#endif // DIALOGFILEINFO_H
