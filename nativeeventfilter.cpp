#include "nativeeventfilter.h"

#ifdef USE_NATIVE_EVENT_FILTER

#include <QWidget>
#include <QDebug>
#include <QVector>
#include <QMap>
#include <QBitArray>
#include <QX11Info>
#include <QDesktopWidget>
#include <QTabletEvent>
#include <QApplication>

#include <iostream>
#include <type_traits>
#include <array>

#include <X11/extensions/XInput2.h>
#include <X11/extensions/XI2proto.h>

namespace {
    Atom AbsX;
    Atom AbsY;
    Atom Pressure;
    Atom TiltX;
    Atom TiltY;
    Atom AbsWheel;

    Display *display;

    struct ValuatorMapping {
        double min;
        double max;
        int axis;
    };

    struct DeviceInfo {
        bool isTablet;
        QVector<ValuatorMapping> mapping;
    };

    QMap<int, DeviceInfo> deviceMap;

    struct xcb_xXIDeviceEvent
    {
        typedef uint32_t Time;
        typedef uint32_t Window;

        uint8_t     type;                   /**< Always GenericEvent */
        uint8_t     extension;              /**< XI extension offset */
        uint16_t    sequenceNumber;
        uint32_t    length;                 /**< Length in 4 byte uints */
        uint16_t    evtype;
        uint16_t    deviceid;
        Time        time;
        uint32_t    detail;                 /**< Keycode or button */
        Window      root;
        Window      event;
        Window      child;
    /* └──────── 32 byte boundary ────────┘ */
        uint32_t    full_sequence;
    /* !!! WARNING !!!
     * The "full_sequence" value above is added by the xcb protocol and is NOT included in the xXIDeviceEvent
     * structure in XI2proto.h. Using the normal xXIDeviceEvent would result in everything past this point
     * being offset by 4 bytes. */
        FP1616      root_x;                 /**< Always screen coords, 16.16 fixed point */
        FP1616      root_y;
        FP1616      event_x;                /**< Always screen coords, 16.16 fixed point */
        FP1616      event_y;
        uint16_t    buttons_len;            /**< Len of button flags in 4 b units */
        uint16_t    valuators_len;          /**< Len of val. flags in 4 b units */
        uint16_t    sourceid;               /**< The source device */
        uint16_t    pad0;
        uint32_t    flags;                  /**< ::XIKeyRepeat */

        struct
        {
            uint32_t    base_mods;              /**< Logically pressed modifiers */
            uint32_t    latched_mods;           /**< Logically latched modifiers */
            uint32_t    locked_mods;            /**< Logically locked modifiers */
            uint32_t    effective_mods;         /**< Effective modifiers */
        } mods;

        struct
        {
            uint8_t     base_group;             /**< Logically "pressed" group */
            uint8_t     latched_group;          /**< Logically latched group */
            uint8_t     locked_group;           /**< Logically locked group */
            uint8_t     effective_group;        /**< Effective group */
        } group;
    };

    static_assert(sizeof(xcb_xXIDeviceEvent) == sizeof(xXIDeviceEvent) + 4,
                  "External xXIDeviceEvent structure does not match expected size.");
    static_assert(offsetof(xcb_xXIDeviceEvent, child) == offsetof(xXIDeviceEvent, child),
                  "External xXIDeviceEvent structure does not match expected layout.");
    static_assert(offsetof(xcb_xXIDeviceEvent, buttons_len) == offsetof(xXIDeviceEvent, buttons_len) + 4,
                  "External xXIDeviceEvent structure does not match expected layout.");
    static_assert(sizeof(xcb_xXIDeviceEvent::time) == sizeof(xXIDeviceEvent::time),
                  "External xXIDeviceEvent member does not match expected size.");
    static_assert(sizeof(xcb_xXIDeviceEvent::root) == sizeof(xXIDeviceEvent::root),
                  "External xXIDeviceEvent member does not match expected size.");

    constexpr int lastMouse(int i = 0, unsigned int mask = Qt::MaxMouseButton) {
            return mask ? lastMouse(i + 1, mask >> 1) : i;
        }
    const int LAST_QT_MOUSE_BUTTON = lastMouse();

    Qt::MouseButton xButtonToQtButton(int button) {
        /* Skip the X11 scroll buttons (4-7) */
        if (button == 1)
            return Qt::LeftButton;
        if (button == 2)
            return Qt::MiddleButton;
        if (button == 3)
            return Qt::RightButton;
        else if (button > 7 && button <= LAST_QT_MOUSE_BUTTON + 4)
            return static_cast<Qt::MouseButton>(1 << (button - 5));
        else
            return Qt::NoButton;
    }

    bool checkDeviceNameWhitelist(const char *name)
    {
        /* Check the device name against the same whitelist as qxcbconnection_xi2.cpp does.
         * We only want generate motion events for devices Qt understands because we rely on
         * it for the proximity & stylus type information. */

        static std::array<const char *, 5> whitelistName = {
            "eraser",
            "cursor",
            "pen",
            "stylus",
            "aiptek"
        };

        QByteArray devName = QByteArray(name).toLower();
        for (const char *testName: whitelistName)
            if (devName.contains(testName))
                return true;
        if (devName.contains("wacom") && !devName.contains("touch"))
            return true;
        return false;
    }

    bool checkQtVersionGreaterOrEqual(int major, int minor, int micro)
    {
        QStringList parts = QString(qVersion()).split(".");
        if (major > parts.value(0).toInt())
            return false;
        if (minor > parts.value(1).toInt())
            return false;
        if (micro > parts.value(2).toInt())
            return false;
        return true;
    }

    int xinputOpcode;
    bool useSourceDevice = false;
    bool generateMouseEvents = false;
}

void NativeEventFilter::install(QGuiApplication *app)
{
    if (app->platformName() != "xcb")
        return;

    if (!checkQtVersionGreaterOrEqual(5, 5, 0))
    {
        useSourceDevice = false;
        generateMouseEvents = false;
    }
    else if (checkQtVersionGreaterOrEqual(5, 7, 0) && !checkQtVersionGreaterOrEqual(5, 9, 0))
    {
        useSourceDevice = true;
        generateMouseEvents = true;
    }
    else
    {
        return;
    }

    display = QX11Info::display();
    int eventBase, errorBase; // Unused values

    if(!XQueryExtension(display, "XInputExtension", &xinputOpcode, &eventBase, &errorBase))
    {
        std::cout << "No XInput extension detected" << std::endl;
        return;
    }

    /*FIXME: In theory we should also check the XInput version, but the XIQueryVersion()
     *       also modifies the "client supported version" and doing that here may conflict
     *       with Qt XInput code. */

    app->installNativeEventFilter(new NativeEventFilter);
    std::cout << "XInput2 event handler installed" << std::endl;
}

NativeEventFilter::NativeEventFilter()
{
    display = QX11Info::display();

    AbsX = XInternAtom(display, "Abs X", false);
    AbsY = XInternAtom(display, "Abs Y", false);
    Pressure = XInternAtom(display, "Abs Pressure", false);
    TiltX = XInternAtom(display, "Abs Tilt X", false);
    TiltY = XInternAtom(display, "Abs Tilt Y", false);
    AbsWheel = XInternAtom(display, "Abs Wheel", false);
}

bool NativeEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long *)
{
    xcb_xXIDeviceEvent *xiEvent = static_cast<xcb_xXIDeviceEvent *>(message);

    if ((xiEvent->type & ~0x80) != GenericEvent)
        return false;

    if (xiEvent->extension != xinputOpcode)
        return false;

    bool process = false;
    QEvent::Type type = QEvent::Type(0); // This is QEvent::None but there is a conflicting X11 define for None
    Qt::MouseButtons buttons = Qt::NoButton;
    Qt::MouseButton clickButton = Qt::NoButton;

    if (XI_ButtonPress == xiEvent->evtype)
    {
        type = QEvent::TabletPress;
        clickButton = xButtonToQtButton(xiEvent->detail);
        process = true;
    }
    else if (XI_ButtonRelease == xiEvent->evtype)
    {
        type = QEvent::TabletRelease;
        clickButton = xButtonToQtButton(xiEvent->detail);
        process = true;
    }
    else if (XI_Motion == xiEvent->evtype)
    {
        type = QEvent::TabletMove;
        process = true;
    }
    else if (XI_HierarchyChanged == xiEvent->evtype)
    {
        deviceMap.clear();
    }

    if (!process)
        return false;

    int deviceID = useSourceDevice ? xiEvent->sourceid : xiEvent->deviceid;

    auto devIter = deviceMap.find(deviceID);
    if (devIter == deviceMap.end())
    {
        DeviceInfo devInfo;
        devInfo.isTablet = false;

        int deviceCount = 1;
        XIDeviceInfo *xDevInfo = XIQueryDevice(display, deviceID, &deviceCount);

        if (xDevInfo)
        {
            //qDebug() << "New device" << xDevInfo->name;
            bool hasX = false;
            bool hasY = false;
            bool hasP = false;
            for (int i = 0; i < xDevInfo->num_classes; ++i)
            {
                if (XIValuatorClass == xDevInfo->classes[i]->type)
                {
                    XIValuatorClassInfo *valuator = reinterpret_cast<XIValuatorClassInfo *>(xDevInfo->classes[i]);

                    int axis = -1;

                    if (valuator->label == AbsX) {
                        hasX = true;
                        axis = 0;
                    }
                    else if (valuator->label == AbsY) {
                        hasY = true;
                        axis = 1;
                    }
                    else if (valuator->label == Pressure) {
                        hasP = true;
                        axis = 2;
                    }
                    else if (valuator->label == TiltX)
                        axis = 3;
                    else if (valuator->label == TiltY)
                        axis = 4;
                    else if (valuator->label == AbsWheel)
                        axis = 5;

                    devInfo.mapping.push_back({valuator->min, valuator->max, axis});
                    //qDebug() << valuator->min << valuator->max << axis;
                }
            }

            if (hasX && hasY && hasP)
                devInfo.isTablet = checkDeviceNameWhitelist(xDevInfo->name);

            XIFreeDeviceInfo(xDevInfo);
        }

        devIter = deviceMap.insert(deviceID, devInfo);
    }

    DeviceInfo &devInfo = *devIter;
    if (!devInfo.isTablet)
        return false;

    int bitLimit;
    unsigned char *buttonMask = (unsigned char*)&xiEvent[1];
    bitLimit = xiEvent->buttons_len * 32;
    bitLimit = std::min<int>(bitLimit, LAST_QT_MOUSE_BUTTON + 4);
    for (int i = 1; i < bitLimit; ++i)
    {
        Qt::MouseButton maskedButton = Qt::NoButton;
        if (XIMaskIsSet(buttonMask, i) && (maskedButton = xButtonToQtButton(i)))
        {
            buttons |= maskedButton;
        }
    }
//   qDebug() << "b" << clickButton << buttons;

    unsigned char *valuatorMask = buttonMask + xiEvent->buttons_len * 4;
    bitLimit = xiEvent->valuators_len * 32;
    bitLimit = std::min<int>(bitLimit, devInfo.mapping.size());
    QBitArray valuatorBits(bitLimit);
    for (int i = 0; i < bitLimit; ++i)
        valuatorBits[i] = XIMaskIsSet(valuatorMask, i);
//    qDebug() << "v" << xiEvent->valuators_len << valuatorBits;

    QVector<double> result(6, 0.0);
    FP3232 *valuatorData = reinterpret_cast<FP3232 *>(valuatorMask + xiEvent->valuators_len * 4);
    for (int i = 0; i < valuatorBits.count(); ++i)
    {
        if (valuatorBits[i])
        {
            if (devInfo.mapping[i].axis != -1)
            {
                double value = double(valuatorData->integral) + double(valuatorData->frac) / double(1l << 32);
                value = (value - devInfo.mapping[i].min) / (devInfo.mapping[i].max - devInfo.mapping[i].min);
                result[devInfo.mapping[i].axis] = value;
            }
            valuatorData++;
        }
    }

    QRect desktop = QDesktopWidget().geometry();
    result[0] *= desktop.width();
    result[1] *= desktop.height();


    if (type == QEvent::Type(0))
        return false;

    QWidget *targetWidget = QWidget::find(xiEvent->event);
    QWidget *modalWidget = QApplication::activeModalWidget();

    if (modalWidget && modalWidget != targetWidget)
        return false;

    if (targetWidget)
    {
        if (targetWidget->objectName() != "mainCanvas")
            return false;

        QPoint global(result[0], result[1]);
        QPoint local = targetWidget->mapFromGlobal(global);
        QPointF globalF(result[0], result[1]);
        QPointF localF = globalF - (global - local);

        if (type == QEvent::TabletMove && buttons == Qt::NoButton)
        {
            // Qt does not report tablet hover motion but we may need to generate mouse motion

            if (generateMouseEvents)
            {
                QMouseEvent mev(QEvent::MouseMove, localF, globalF, clickButton, buttons, QGuiApplication::keyboardModifiers());
                mev.setTimestamp(ulong{xiEvent->time});
                QGuiApplication::sendEvent(targetWidget, &mev);
            }
        }
        else
        {
            /* Evil secrets: The Tool and PointerType values don't matter because we only use them on the ProxIn/Out events.
             * SerialId and Buttons are also never used, and key modifiers are not set by the official Qt tablet events.
             */
            QTabletEvent ev(type, localF, globalF,
                            QTabletEvent::Stylus, QTabletEvent::Pen,
                            result[2], result[3], result[4],
                            0 /* tangentialPressure */, 0 /* rotation */, 0 /* z */,
                            QGuiApplication::keyboardModifiers(), 0 /* uid */, clickButton, buttons);
            ev.setTimestamp(ulong{xiEvent->time});
            QGuiApplication::sendEvent(targetWidget, &ev);

            if (generateMouseEvents && (type == QEvent::TabletPress || type == QEvent::TabletRelease))
            {
                QEvent::Type mouseEventType = type == QEvent::TabletPress ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease;
                QMouseEvent mev(mouseEventType, localF, globalF, clickButton, buttons, QGuiApplication::keyboardModifiers());
                mev.setTimestamp(ulong{xiEvent->time});
                QGuiApplication::sendEvent(targetWidget, &mev);
            }
        }
    }

    return true;
}

#endif /* USE_NATIVE_EVENT_FILTER */
