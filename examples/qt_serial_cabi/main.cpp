#include "cabicontroller.h"
#include "portlistmodel_cabi.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#ifdef Q_OS_ANDROID
#  include <QJniEnvironment>
#  include <QJniObject>
#  if __has_include(<QNativeInterface/QAndroidApplication>)
#    include <QNativeInterface/QAndroidApplication>
#    define CPS_HAVE_QT_ANDROID_CONTEXT 1
#  endif
#  include <cps/cps.h>
#endif

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("cps Qt serial terminal (C ABI)");

#ifdef Q_OS_ANDROID
    // The cps Android backend (JNI + embedded usb-serial-for-android dex) is only
    // initialized once cps_serial_init_android() is called with the JVM + a Context.
    // Without it, enumeration returns nothing. The QJniObject must stay alive across
    // the call (cps takes a NewGlobalRef on it).
    {
        JavaVM* vm = QJniEnvironment::javaVM();
        QJniObject context;
#if defined(CPS_HAVE_QT_ANDROID_CONTEXT)
        context = QNativeInterface::QAndroidApplication::context();
#endif
        if (!context.isValid()) {
            // Fallback for Qt kits without the QNativeInterface header.
            context = QJniObject::callStaticObjectMethod(
                "android/app/ActivityThread", "currentApplication",
                "()Landroid/app/Application;");
        }
        if (vm && context.isValid())
            cps_serial_init_android(reinterpret_cast<void*>(vm), context.object<jobject>());
    }
#endif

    PortListModelCabi portModel;
    portModel.refresh(); // populate the combo on startup

    CabiController controller;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("portModel", &portModel);
    engine.rootContext()->setContextProperty("serial", &controller);
    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
