/* Android: a second screen (the AYN Thor's lower one). The activity's Java
 * side opens a window there (PokeParkActivity.showSecondScreen) and hands
 * its surface and touches here; the game's thread draws in it with Vulkan
 * (vk_backend.c), taking the window at its next frame after it changes.
 *
 * The surface's callbacks come on the activity's UI thread; when one goes,
 * the callback waits (a second at most) for the game's thread to let go
 * of it, as Android takes it away once the callback returns. */
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sChanged = PTHREAD_COND_INITIALIZER;
static ANativeWindow* sWindow; /* the screen's, NULL if none */
static ANativeWindow* sHeld;   /* the one the game's thread draws in */
static unsigned sGen;          /* counts sWindow's changes */

/* Touches, in the window's pixels: action 0 down, 1 up, 2 moved. */
typedef struct {
    int action;
    float x, y;
} Touch;
static Touch sTouches[64];
static unsigned sTouchIn, sTouchOut;

static void JNICALL second_surface(JNIEnv* env, jclass cls, jobject surface, jint w, jint h) {
    ANativeWindow* win = surface ? ANativeWindow_fromSurface(env, surface) : NULL;
    ANativeWindow* old;
    (void)cls;
    fprintf(stderr, "PokePark: second screen's window %p, %dx%d\n", (void*)win, (int)w, (int)h);
    pthread_mutex_lock(&sLock);
    old = sWindow;
    sWindow = win;
    sGen++;
    if (old && old != win) {
        struct timespec until;
        clock_gettime(CLOCK_REALTIME, &until);
        until.tv_sec += 1;
        while (sHeld == old && pthread_cond_timedwait(&sChanged, &sLock, &until) == 0) {}
    }
    pthread_mutex_unlock(&sLock);
    if (old) ANativeWindow_release(old);
}

static void JNICALL second_touch(JNIEnv* env, jclass cls, jint action, jfloat x, jfloat y) {
    (void)env;
    (void)cls;
    pthread_mutex_lock(&sLock);
    if (sTouchIn - sTouchOut < sizeof(sTouches) / sizeof(sTouches[0])) {
        Touch* t = &sTouches[sTouchIn++ % (sizeof(sTouches) / sizeof(sTouches[0]))];
        t->action = action;
        t->x = x;
        t->y = y;
    }
    pthread_mutex_unlock(&sLock);
}

/* Registers the activity's native methods here and asks it for the
 * window, on the activity's thread. */
void second_screen_start(JavaVM* vm, jobject activity) {
    static const JNINativeMethod kMethods[] = {
        {"secondSurface", "(Landroid/view/Surface;II)V", (void*)second_surface},
        {"secondTouch", "(IFF)V", (void*)second_touch},
    };
    JNIEnv* env;
    jclass cls;
    if ((*vm)->AttachCurrentThread(vm, &env, NULL) != JNI_OK) return;
    cls = (*env)->GetObjectClass(env, activity);
    if ((*env)->RegisterNatives(env, cls, kMethods, 2) == 0)
        (*env)->CallVoidMethod(env, activity, (*env)->GetMethodID(env, cls, "showSecondScreen", "()V"));
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }
    (*env)->DeleteLocalRef(env, cls);
}

/* For the game's thread: how many times the window changed, and the
 * window (NULL if none), to draw in until second_window_drop. */
unsigned second_window_gen(void) {
    unsigned gen;
    pthread_mutex_lock(&sLock);
    gen = sGen;
    pthread_mutex_unlock(&sLock);
    return gen;
}

ANativeWindow* second_window_take(unsigned* gen) {
    ANativeWindow* win;
    pthread_mutex_lock(&sLock);
    win = sHeld = sWindow;
    *gen = sGen;
    pthread_mutex_unlock(&sLock);
    return win;
}

void second_window_drop(void) {
    pthread_mutex_lock(&sLock);
    sHeld = NULL;
    pthread_cond_broadcast(&sChanged);
    pthread_mutex_unlock(&sLock);
}

/* The next touch, if there is one. */
int second_touch_next(int* action, float* x, float* y) {
    int got = 0;
    pthread_mutex_lock(&sLock);
    if (sTouchOut != sTouchIn) {
        const Touch* t = &sTouches[sTouchOut++ % (sizeof(sTouches) / sizeof(sTouches[0]))];
        *action = t->action;
        *x = t->x;
        *y = t->y;
        got = 1;
    }
    pthread_mutex_unlock(&sLock);
    return got;
}
