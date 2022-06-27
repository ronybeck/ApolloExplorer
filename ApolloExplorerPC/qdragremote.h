#ifndef QDRAGREMOTE_H
#define QDRAGREMOTE_H

#include <QDrag>
#include <QObject>

class QDragRemote : public QDrag
{
    Q_OBJECT
public:
    QDragRemote( QObject *dragSource );

public slots:
    void deleteLater();
};

#endif // QDRAGREMOTE_H
