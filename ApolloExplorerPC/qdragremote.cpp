#include "qdragremote.h"
#include "AEUtils.h"

QDragRemote::QDragRemote( QObject *dragSource ) :
    QDrag{ dragSource }
{

}

void QDragRemote::deleteLater()
{
    DBGLOG << "Ignoring delete";
}
