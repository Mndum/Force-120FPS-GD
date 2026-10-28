#include <Geode/Geode.hpp>
#include "platform.hpp"

#ifdef GEODE_IS_ANDROID

#include <Geode/cocos/platform/android/jni/JniHelper.h>
#include <jni.h>

using namespace geode::prelude;

// On Android the Geode launcher owns the frame loop: the game's
// setAnimationInterval() calls are ignored and the renderer simply draws once
// per screen refresh. So to get 120 FPS we have to get the *screen* to run at
// 120 Hz, which is done by asking Android for that rate on the game's surface
// (Surface.setFrameRate, Android 11+). We also lift the launcher's optional
// "Limit Frame Rate" setting if it is below the target.

namespace {
    // Cached weak reference to the game's GLSurfaceView.
    jweak s_glView = nullptr;
    double s_maxRefresh = 0.0;
    double s_currentRefresh = 0.0;

    JNIEnv* getEnv() {
        JavaVM* vm = JniHelper::getJavaVM();
        JNIEnv* env = nullptr;
        if (!vm || vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
            return nullptr;
        }
        return env;
    }

    // Clears any pending Java exception. Returns true if there was one.
    bool failed(JNIEnv* env) {
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return true;
        }
        return false;
    }

    int sdkInt(JNIEnv* env) {
        static int cached = -1;
        if (cached >= 0) return cached;
        jclass cls = env->FindClass("android/os/Build$VERSION");
        if (failed(env) || !cls) return 0;
        jfieldID f = env->GetStaticFieldID(cls, "SDK_INT", "I");
        if (failed(env) || !f) return 0;
        cached = env->GetStaticIntField(cls, f);
        return cached;
    }

    // The launcher keeps the running activity in BaseRobTopActivity.me
    // (a WeakReference<Activity>).
    jobject getActivity(JNIEnv* env) {
        jclass cls = env->FindClass("com/customRobTop/BaseRobTopActivity");
        if (failed(env) || !cls) return nullptr;
        jfieldID f = env->GetStaticFieldID(cls, "me", "Ljava/lang/ref/WeakReference;");
        if (failed(env) || !f) return nullptr;
        jobject ref = env->GetStaticObjectField(cls, f);
        if (failed(env) || !ref) return nullptr;
        jclass wr = env->FindClass("java/lang/ref/WeakReference");
        jmethodID get = env->GetMethodID(wr, "get", "()Ljava/lang/Object;");
        if (failed(env) || !get) return nullptr;
        jobject activity = env->CallObjectMethod(ref, get);
        if (failed(env)) return nullptr;
        return activity;
    }

    jobject findGLView(JNIEnv* env, jobject view, jclass glCls, jclass groupCls, jmethodID count, jmethodID childAt) {
        if (env->IsInstanceOf(view, glCls)) return env->NewLocalRef(view);
        if (!env->IsInstanceOf(view, groupCls)) return nullptr;
        jint n = env->CallIntMethod(view, count);
        if (failed(env)) return nullptr;
        for (jint i = 0; i < n; i++) {
            jobject child = env->CallObjectMethod(view, childAt, i);
            if (failed(env) || !child) continue;
            jobject found = findGLView(env, child, glCls, groupCls, count, childAt);
            env->DeleteLocalRef(child);
            if (found) return found;
        }
        return nullptr;
    }

    // Returns a local ref to the GLSurfaceView, using the cache when possible.
    jobject getGLView(JNIEnv* env, jobject activity) {
        if (s_glView && !env->IsSameObject(s_glView, nullptr)) {
            return env->NewLocalRef(s_glView);
        }
        if (!activity) return nullptr;

        jclass actCls = env->GetObjectClass(activity);
        jmethodID getWindow = env->GetMethodID(actCls, "getWindow", "()Landroid/view/Window;");
        if (failed(env) || !getWindow) return nullptr;
        jobject window = env->CallObjectMethod(activity, getWindow);
        if (failed(env) || !window) return nullptr;

        jclass winCls = env->FindClass("android/view/Window");
        jmethodID getDecor = env->GetMethodID(winCls, "getDecorView", "()Landroid/view/View;");
        if (failed(env) || !getDecor) return nullptr;
        jobject decor = env->CallObjectMethod(window, getDecor);
        if (failed(env) || !decor) return nullptr;

        jclass glCls = env->FindClass("android/opengl/GLSurfaceView");
        jclass groupCls = env->FindClass("android/view/ViewGroup");
        if (failed(env) || !glCls || !groupCls) return nullptr;
        jmethodID count = env->GetMethodID(groupCls, "getChildCount", "()I");
        jmethodID childAt = env->GetMethodID(groupCls, "getChildAt", "(I)Landroid/view/View;");
        if (failed(env) || !count || !childAt) return nullptr;

        jobject gl = findGLView(env, decor, glCls, groupCls, count, childAt);
        if (gl) {
            if (s_glView) env->DeleteWeakGlobalRef(s_glView);
            s_glView = env->NewWeakGlobalRef(gl);
        }
        return gl;
    }

    void readDisplayInfo(JNIEnv* env, jobject activity) {
        if (!activity) return;
        jclass actCls = env->GetObjectClass(activity);
        jobject display = nullptr;

        if (sdkInt(env) >= 30) {
            jmethodID getDisplay = env->GetMethodID(actCls, "getDisplay", "()Landroid/view/Display;");
            if (!failed(env) && getDisplay) {
                display = env->CallObjectMethod(activity, getDisplay);
                if (failed(env)) display = nullptr;
            }
        }
        if (!display) {
            jmethodID getWM = env->GetMethodID(actCls, "getWindowManager", "()Landroid/view/WindowManager;");
            if (failed(env) || !getWM) return;
            jobject wm = env->CallObjectMethod(activity, getWM);
            if (failed(env) || !wm) return;
            jclass wmCls = env->FindClass("android/view/WindowManager");
            jmethodID getDefault = env->GetMethodID(wmCls, "getDefaultDisplay", "()Landroid/view/Display;");
            if (failed(env) || !getDefault) return;
            display = env->CallObjectMethod(wm, getDefault);
            if (failed(env) || !display) return;
        }

        jclass dispCls = env->FindClass("android/view/Display");
        jmethodID getRate = env->GetMethodID(dispCls, "getRefreshRate", "()F");
        if (!failed(env) && getRate) {
            s_currentRefresh = env->CallFloatMethod(display, getRate);
            if (failed(env)) s_currentRefresh = 0;
        }

        jmethodID getModes = env->GetMethodID(dispCls, "getSupportedModes", "()[Landroid/view/Display$Mode;");
        if (failed(env) || !getModes) return;
        auto modes = static_cast<jobjectArray>(env->CallObjectMethod(display, getModes));
        if (failed(env) || !modes) return;

        jclass modeCls = env->FindClass("android/view/Display$Mode");
        jmethodID modeRate = env->GetMethodID(modeCls, "getRefreshRate", "()F");
        if (failed(env) || !modeRate) return;

        double best = 0;
        jsize n = env->GetArrayLength(modes);
        for (jsize i = 0; i < n; i++) {
            jobject mode = env->GetObjectArrayElement(modes, i);
            if (!mode) continue;
            double r = env->CallFloatMethod(mode, modeRate);
            if (!failed(env) && r > best) best = r;
            env->DeleteLocalRef(mode);
        }
        if (best > 0) s_maxRefresh = best;
    }

    // Ask Android to run the game's surface at `fps`.
    void requestSurfaceRate(JNIEnv* env, jobject glView, float fps) {
        int sdk = sdkInt(env);
        if (sdk < 30) return; // Surface.setFrameRate is Android 11+

        jclass svCls = env->FindClass("android/view/SurfaceView");
        jmethodID getHolder = env->GetMethodID(svCls, "getHolder", "()Landroid/view/SurfaceHolder;");
        if (failed(env) || !getHolder) return;
        jobject holder = env->CallObjectMethod(glView, getHolder);
        if (failed(env) || !holder) return;

        jclass holderCls = env->FindClass("android/view/SurfaceHolder");
        jmethodID getSurface = env->GetMethodID(holderCls, "getSurface", "()Landroid/view/Surface;");
        if (failed(env) || !getSurface) return;
        jobject surface = env->CallObjectMethod(holder, getSurface);
        if (failed(env) || !surface) return;

        jclass surfCls = env->FindClass("android/view/Surface");
        jmethodID isValid = env->GetMethodID(surfCls, "isValid", "()Z");
        if (failed(env) || !isValid) return;
        if (!env->CallBooleanMethod(surface, isValid) || failed(env)) return;

        constexpr jint FRAME_RATE_COMPATIBILITY_DEFAULT = 0;
        constexpr jint CHANGE_FRAME_RATE_ALWAYS = 1;

        if (sdk >= 31) {
            jmethodID setRate = env->GetMethodID(surfCls, "setFrameRate", "(FII)V");
            if (!failed(env) && setRate) {
                env->CallVoidMethod(surface, setRate, fps, FRAME_RATE_COMPATIBILITY_DEFAULT, CHANGE_FRAME_RATE_ALWAYS);
                if (!failed(env)) return;
            }
        }
        jmethodID setRate = env->GetMethodID(surfCls, "setFrameRate", "(FI)V");
        if (failed(env) || !setRate) return;
        env->CallVoidMethod(surface, setRate, fps, FRAME_RATE_COMPATIBILITY_DEFAULT);
        failed(env);
    }

    // The launcher's renderer can sleep between frames ("Limit Frame Rate"
    // setting). Raise that limit to the target, and on screens faster than the
    // target (e.g. 144 Hz) cap it at the target so it really is 120.
    void fixRendererLimit(JNIEnv* env, jobject glView, int fps) {
        jclass cocosView = env->FindClass("org/cocos2dx/lib/Cocos2dxGLSurfaceView");
        if (failed(env) || !cocosView || !env->IsInstanceOf(glView, cocosView)) return;

        jfieldID rendererField = env->GetFieldID(cocosView, "cocos2dxRenderer", "Lorg/cocos2dx/lib/Cocos2dxRenderer;");
        if (failed(env) || !rendererField) return;
        jobject renderer = env->GetObjectField(glView, rendererField);
        if (failed(env) || !renderer) return;

        jclass rendererCls = env->GetObjectClass(renderer);
        jmethodID limit = env->GetMethodID(rendererCls, "limitFrameRate", "(I)V");
        if (failed(env) || !limit) return;

        bool needsLimit = false;

        // Current launcher limit (null = unlimited)
        jfieldID intervalField = env->GetFieldID(rendererCls, "mAnimationInterval", "Ljava/lang/Long;");
        if (!failed(env) && intervalField) {
            jobject boxed = env->GetObjectField(renderer, intervalField);
            if (!failed(env) && boxed) {
                jclass longCls = env->FindClass("java/lang/Long");
                jmethodID longValue = env->GetMethodID(longCls, "longValue", "()J");
                if (!failed(env) && longValue) {
                    jlong nanos = env->CallLongMethod(boxed, longValue);
                    if (!failed(env)) {
                        jlong wanted = 1'000'000'000LL / fps;
                        // Launcher limit is slower than our target -> raise it
                        if (nanos > wanted + 1000) needsLimit = true;
                        // Already exactly ours -> nothing to do
                        if (nanos >= wanted - 1000 && nanos <= wanted + 1000) return;
                    }
                }
            }
        }

        // Screen is running faster than the target (144/165 Hz panels)
        if (s_currentRefresh > fps + 1.0) needsLimit = true;

        if (needsLimit) {
            env->CallVoidMethod(renderer, limit, static_cast<jint>(fps));
            failed(env);
        }
    }
}

namespace fps_platform {
    void apply(double fps) {
        JNIEnv* env = getEnv();
        if (!env) return;
        if (env->PushLocalFrame(64) != JNI_OK) {
            failed(env);
            return;
        }

        jobject activity = getActivity(env);
        readDisplayInfo(env, activity);

        if (jobject gl = getGLView(env, activity)) {
            requestSurfaceRate(env, gl, static_cast<float>(fps));
            fixRendererLimit(env, gl, static_cast<int>(fps));
        }

        failed(env);
        env->PopLocalFrame(nullptr);
    }

    double maxRefreshRate() { return s_maxRefresh; }

    std::string blockerHint(double fps) {
        if (s_maxRefresh > 0 && s_maxRefresh + 1.0 < fps) {
            return fmt::format(
                "Your screen tops out at {:.0f} Hz, so that's the most FPS it can show.", s_maxRefresh
            );
        }
        if (s_currentRefresh > 0 && s_currentRefresh + 1.0 < fps) {
            return fmt::format(
                "Your phone is holding the screen at {:.0f} Hz. Turn off Battery Saver and set "
                "the display's refresh rate to High / Adaptive in Android settings.", s_currentRefresh
            );
        }
        return "";
    }

    // The launcher ignores the director interval, but setting it is harmless
    // and keeps GD's internal timing consistent.
    bool controlsDirectorInterval() { return true; }
}

#endif
