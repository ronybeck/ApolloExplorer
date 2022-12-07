#ifndef MOUSEEVENTFILTER_H
#define MOUSEEVENTFILTER_H

#include <QObject>
#include <QAbstractNativeEventFilter>

class MouseEventFilter : public QAbstractNativeEventFilter
{

public:
    explicit MouseEventFilter();

    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;
    bool isLeftMouseButtonDown();

private:
    bool m_LeftMouseButtonDown;
};

class MouseEventFilterSingleton
{
public:
    static MouseEventFilter *getInstance();
};

#endif // MOUSEEVENTFILTER_H
