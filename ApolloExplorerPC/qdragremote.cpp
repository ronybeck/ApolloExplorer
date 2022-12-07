#include "qdragremote.h"
#include "AEUtils.h"

QDragRemote::QDragRemote( QObject *dragSource ) :
    QDrag{ dragSource }
{

}

bool QDragRemote::event(QEvent *e)
{
    switch( e->type() )
    {
        case QEvent::DeferredDelete:
            return true;
        default:
            return QObject::event( e );
        break;
    }
}
