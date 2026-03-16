#include <jni.h>
#include <string>
#include <vector>
#include <android/log.h>

#include "voice/voice_engine.hpp"

#define LOG_TAG "VoiceJNI"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace grotto::voice;

// ============================================================================
// JNI Helper Functions
// ============================================================================

static std::string jstringToString(JNIEnv* env, jstring str) {
    if (!str) return "";
    const char* cstr = env->GetStringUTFChars(str, nullptr);
    std::string result(cstr);
    env->ReleaseStringUTFChars(str, cstr);
    return result;
}

static std::vector<uint8_t> jbyteArrayToVector(JNIEnv* env, jbyteArray arr) {
    if (!arr) return {};
    jsize len = env->GetArrayLength(arr);
    std::vector<uint8_t> result(len);
    env->GetByteArrayRegion(arr, 0, len, reinterpret_cast<jbyte*>(result.data()));
    return result;
}

static jbyteArray vectorToJbyteArray(JNIEnv* env, const std::vector<uint8_t>& data) {
    if (data.empty()) return nullptr;
    jbyteArray arr = env->NewByteArray(data.size());
    env->SetByteArrayRegion(arr, 0, data.size(), 
                           reinterpret_cast<const jbyte*>(data.data()));
    return arr;
}

// ============================================================================
// JNI Callback Setup
// ============================================================================

static JavaVM* g_vm = nullptr;
static jclass g_voice_class = nullptr;
static jmethodID g_send_ice_mid = nullptr;
static jmethodID g_peer_joined_mid = nullptr;
static jmethodID g_peer_left_mid = nullptr;
static jmethodID g_audio_level_mid = nullptr;

static void setupVoiceCallbacks(VoiceEngine* engine) {
    engine->setIceCandidateCallback([](const std::string& peer_id, 
                                        const std::vector<uint8_t>& candidate) {
        if (!g_vm || !g_voice_class || !g_send_ice_mid) return;
        
        JNIEnv* env;
        jint attach_result = g_vm->AttachCurrentThread(&env, nullptr);
        if (attach_result != JNI_OK) return;
        
        jstring jpeer = env->NewStringUTF(peer_id.c_str());
        jbyteArray jcandidate = vectorToJbyteArray(env, candidate);
        
        env->CallStaticVoidMethod(g_voice_class, g_send_ice_mid, jpeer, jcandidate);
        
        env->DeleteLocalRef(jpeer);
        env->DeleteLocalRef(jcandidate);
        
        g_vm->DetachCurrentThread();
    });
    
    engine->setPeerJoinedCallback([](const std::string& peer_id) {
        if (!g_vm || !g_voice_class || !g_peer_joined_mid) return;
        
        JNIEnv* env;
        jint attach_result = g_vm->AttachCurrentThread(&env, nullptr);
        if (attach_result != JNI_OK) return;
        
        jstring jpeer = env->NewStringUTF(peer_id.c_str());
        env->CallStaticVoidMethod(g_voice_class, g_peer_joined_mid, jpeer);
        env->DeleteLocalRef(jpeer);
        
        g_vm->DetachCurrentThread();
    });
    
    engine->setPeerLeftCallback([](const std::string& peer_id) {
        if (!g_vm || !g_voice_class || !g_peer_left_mid) return;
        
        JNIEnv* env;
        jint attach_result = g_vm->AttachCurrentThread(&env, nullptr);
        if (attach_result != JNI_OK) return;
        
        jstring jpeer = env->NewStringUTF(peer_id.c_str());
        env->CallStaticVoidMethod(g_voice_class, g_peer_left_mid, jpeer);
        env->DeleteLocalRef(jpeer);
        
        g_vm->DetachCurrentThread();
    });
    
    engine->setAudioLevelCallback([](const std::string& peer_id, float level) {
        if (!g_vm || !g_voice_class || !g_audio_level_mid) return;
        
        JNIEnv* env;
        jint attach_result = g_vm->AttachCurrentThread(&env, nullptr);
        if (attach_result != JNI_OK) return;
        
        jstring jpeer = env->NewStringUTF(peer_id.c_str());
        env->CallStaticVoidMethod(g_voice_class, g_audio_level_mid, jpeer, level);
        env->DeleteLocalRef(jpeer);
        
        g_vm->DetachCurrentThread();
    });
}

// ============================================================================
// JNI Exports
// ============================================================================

extern "C" {

JNIEXPORT jboolean JNICALL
Java_fi_grotto_android_native_NativeVoice_nativeInit(
        JNIEnv* env,
        jclass clazz,
        jint sample_rate,
        jint frames_per_buffer) {
    
    // Cache JVM and method IDs for callbacks
    env->GetJavaVM(&g_vm);
    g_voice_class = (jclass)env->NewGlobalRef(clazz);
    
    g_send_ice_mid = env->GetStaticMethodID(clazz, "sendIceCandidate", 
                                            "(Ljava/lang/String;[B)V");
    g_peer_joined_mid = env->GetStaticMethodID(clazz, "onPeerJoined",
                                               "(Ljava/lang/String;)V");
    g_peer_left_mid = env->GetStaticMethodID(clazz, "onPeerLeft",
                                             "(Ljava/lang/String;)V");
    g_audio_level_mid = env->GetStaticMethodID(clazz, "onAudioLevel",
                                               "(Ljava/lang/String;F)V");
    
    auto* engine = getVoiceEngine();
    setupVoiceCallbacks(engine);
    
    return engine->init(sample_rate, frames_per_buffer) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_fi_grotto_android_native_NativeVoice_joinRoom(
        JNIEnv* env,
        jclass /*clazz*/,
        jstring channel_id,
        jboolean is_private_call) {
    
    auto* engine = getVoiceEngine();
    if (!engine) return;
    
    std::string channel = jstringToString(env, channel_id);
    engine->joinRoom(channel, is_private_call == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_fi_grotto_android_native_NativeVoice_leaveRoom(
        JNIEnv* /*env*/,
        jclass /*clazz*/) {
    
    auto* engine = getVoiceEngine();
    if (!engine) return;
    
    engine->leaveRoom();
}

JNIEXPORT void JNICALL
Java_fi_grotto_android_native_NativeVoice_call(
        JNIEnv* env,
        jclass /*clazz*/,
        jstring peer_id) {
    
    auto* engine = getVoiceEngine();
    if (!engine) return;
    
    std::string peer = jstringToString(env, peer_id);
    engine->call(peer);
}

JNIEXPORT void JNICALL
Java_fi_grotto_android_native_NativeVoice_acceptCall(
        JNIEnv* /*env*/,
        jclass /*clazz*/) {
    
    auto* engine = getVoiceEngine();
    if (!engine) return;
    
    engine->acceptCall();
}

JNIEXPORT void JNICALL
Java_fi_grotto_android_native_NativeVoice_declineCall(
        JNIEnv* /*env*/,
        jclass /*clazz*/) {
    
    auto* engine = getVoiceEngine();
    if (!engine) return;
    
    engine->declineCall();
}

JNIEXPORT void JNICALL
Java_fi_grotto_android_native_NativeVoice_hangup(
        JNIEnv* /*env*/,
        jclass /*clazz*/) {
    
    auto* engine = getVoiceEngine();
    if (!engine) return;
    
    engine->hangup();
}

JNIEXPORT void JNICALL
Java_fi_grotto_android_native_NativeVoice_setMuted(
        JNIEnv* /*env*/,
        jclass /*clazz*/,
        jboolean muted) {
    
    auto* engine = getVoiceEngine();
    if (!engine) return;
    
    engine->setMuted(muted == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_fi_grotto_android_native_NativeVoice_setDeafened(
        JNIEnv* /*env*/,
        jclass /*clazz*/,
        jboolean deafened) {
    
    auto* engine = getVoiceEngine();
    if (!engine) return;
    
    engine->setDeafened(deafened == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_fi_grotto_android_native_NativeVoice_onVoiceSignal(
        JNIEnv* env,
        jclass /*clazz*/,
        jstring from_user,
        jint signal_type,
        jbyteArray data) {
    
    auto* engine = getVoiceEngine();
    if (!engine) return;
    
    std::string from = jstringToString(env, from_user);
    std::vector<uint8_t> signal_data = jbyteArrayToVector(env, data);
    
    engine->onSignal(from, static_cast<SignalType>(signal_type), signal_data);
}

JNIEXPORT jobject JNICALL
Java_fi_grotto_android_native_NativeVoice_getAudioStats(
        JNIEnv* env,
        jclass /*clazz*/) {
    
    auto* engine = getVoiceEngine();
    if (!engine) return nullptr;
    
    AudioStats stats = engine->getAudioStats();
    
    // Create AudioStats object
    jclass statsClass = env->FindClass("fi/grotto/android/native/NativeVoice$AudioStats");
    jmethodID constructor = env->GetMethodID(statsClass, "<init>", "(IFI)V");
    
    jobject result = env->NewObject(statsClass, constructor,
                                    stats.latency_ms,
                                    stats.packet_loss_percent,
                                    stats.jitter_ms);
    
    env->DeleteLocalRef(statsClass);
    return result;
}

JNIEXPORT jboolean JNICALL
Java_fi_grotto_android_native_NativeVoice_isInRoom(
        JNIEnv* /*env*/,
        jclass /*clazz*/) {
    
    auto* engine = getVoiceEngine();
    if (!engine) return JNI_FALSE;
    
    return engine->isInRoom() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobjectArray JNICALL
Java_fi_grotto_android_native_NativeVoice_getParticipants(
        JNIEnv* env,
        jclass /*clazz*/) {
    
    auto* engine = getVoiceEngine();
    if (!engine) return nullptr;
    
    auto participants = engine->getParticipants();
    
    jobjectArray result = env->NewObjectArray(participants.size(),
                                               env->FindClass("java/lang/String"),
                                               nullptr);
    
    for (size_t i = 0; i < participants.size(); i++) {
        jstring str = env->NewStringUTF(participants[i].c_str());
        env->SetObjectArrayElement(result, i, str);
        env->DeleteLocalRef(str);
    }
    
    return result;
}

JNIEXPORT void JNICALL
Java_fi_grotto_android_native_NativeVoice_destroy(
        JNIEnv* /*env*/,
        jclass /*clazz*/) {
    
    destroyVoiceEngine();
}

} // extern "C"
