// Reference snippet: Android consumer integration contract (NOT compiled here).
//
// The cps library is Qt-free. On Android the consumer app must:
//   1. Bundle libcps.so for the chosen ABIs in the APK.
//   2. Call System.loadLibrary("cps") once (standard for any JNI native library;
//      this is NOT "linking java files"). This triggers JNI_OnLoad.
//   3. Call cps_serial_init_android(NULL, <android Context>) once before opening a port.
//
// From a Qt for Android app, the Context comes from QtAndroid::androidContext()
// (cast jobject to void*). From a plain native-activity app, obtain the Context in
// your Activity/Kotlin code and pass it down.

#include <cps/cps.h>

extern "C" {

// Example: call this from your Activity's onCreate (or equivalent), passing the
// Android Context jobject obtained from the framework.
void my_app_init_android(void* android_context_jobject) {
    cps_serial_init_android(/*java_vm=*/nullptr, android_context_jobject);
}

}  // extern "C"
