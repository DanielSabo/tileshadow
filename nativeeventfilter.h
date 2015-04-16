#ifndef NATIVEEVENTFILTER_H
#define NATIVEEVENTFILTER_H

#ifdef USE_NATIVE_EVENT_FILTER

#include <QAbstractNativeEventFilter>

class QGuiApplication;
class NativeEventFilter : public QAbstractNativeEventFilter
{
public:
    NativeEventFilter();
    bool nativeEventFilter(const QByteArray & eventType, void * message, long * result) override;
    static void install(QGuiApplication *app);
};

#endif /* USE_NATIVE_EVENT_FILTER */

#endif // NATIVEEVENTFILTER_H
