#include "mouseeventfilter.h"
#import <AppKit/AppKit.h>


MouseEventFilter::MouseEventFilter() :
    m_LeftMouseButtonDown( false )
{

}

bool MouseEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
    Q_UNUSED( result )
    //if ( QString( eventType ) == "mac_generic_NSEvent")
    {
        NSEvent *event = static_cast<NSEvent *>(message);
        if ([event type] == NSEventTypeLeftMouseDown )
        {
            m_LeftMouseButtonDown = true;
        }
        if ([event type] == NSEventTypeLeftMouseUp )
        {
            m_LeftMouseButtonDown = false;
        }
    }
    return false;
}

bool MouseEventFilter::isLeftMouseButtonDown()
{
    return m_LeftMouseButtonDown;
}
