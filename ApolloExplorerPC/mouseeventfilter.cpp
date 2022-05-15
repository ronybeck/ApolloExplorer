#include "mouseeventfilter.h"

//#define DEBUG 1
#include "AEUtils.h"

//Linux specific stuff
#if __linux__
#include <QX11Info>
extern "C"
{
#include <linux/input.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#undef Bool
}
#endif

MouseEventFilter::MouseEventFilter() :
    m_LeftMouseButtonDown( false )
{

}

bool MouseEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long *)
{
#if __linux__
#define MOUSE_BUTTON_MASK_1 0x0001
#define MOUSE_BUTTON_MASK_2 0x0003
#define MOUSE_BUTTON_MASK_DOWN 0x0400
#define MOUSE_BUTTON_MASK_UP 0x0500


    if (eventType == "xcb_generic_event_t")
    {
        xcb_generic_event_t* event = static_cast<xcb_generic_event_t *>(message);
        uint8_t response_type = event->response_type & ~0x80;
        if( response_type != 35) DBGLOG << "XCB Event type: " << response_type;
        switch( response_type )
        {
            case 84:
            case XCB_BUTTON_PRESS:
            case 85:
            case XCB_BUTTON_RELEASE:
            {
                //For what ever reason, all events come through as "mouse button release"
                xcb_button_press_event_t *bp = (xcb_button_press_event_t *)event;
                if( ( bp->state & 0x0F00 ) == MOUSE_BUTTON_MASK_UP )
                {
                   if( ( bp->state & 0x000f ) == MOUSE_BUTTON_MASK_1 )
                   {
                       DBGLOG << "Mouse button 1 up.";
                       m_LeftMouseButtonDown = false;
                   }
                   if( ( bp->state & 0x000f ) == MOUSE_BUTTON_MASK_2 )
                   {
                       DBGLOG << "Mouse button 2 up.";
                   }
                }

                if( ( bp->state & 0x0f00 ) == MOUSE_BUTTON_MASK_DOWN )
                {
                    if( ( bp->state & 0x000f ) == MOUSE_BUTTON_MASK_1 )
                    {
                        DBGLOG << "Mouse button 1 down.";
                        m_LeftMouseButtonDown = true;
                    }
                    if( ( bp->state & 0x000f ) == MOUSE_BUTTON_MASK_2 )
                    {
                        DBGLOG << "Mouse button 2 down.";
                    }
                }
            }
            default:
            break;
        }
    }
#endif
    return false;
}

bool MouseEventFilter::isLeftMouseButtonDown()
{
    return m_LeftMouseButtonDown;
}

MouseEventFilter *MouseEventFilterSingleton::getInstance()
{
    static MouseEventFilter* instance = new MouseEventFilter();
    return instance;
}