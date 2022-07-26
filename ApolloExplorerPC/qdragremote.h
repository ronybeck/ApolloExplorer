#ifndef QDRAGREMOTE_H
#define QDRAGREMOTE_H

#include <QDrag>
#include <QObject>
#include <QEvent>

class QDragRemote : public QDrag
{
    Q_OBJECT
public:
    QDragRemote( QObject *dragSource );

public slots:
    bool event( QEvent *e ) override;
};

#endif // QDRAGREMOTE_H
