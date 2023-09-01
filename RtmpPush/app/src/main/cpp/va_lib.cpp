#include <jni.h>

#include <string>

#include "core/Pusher.h"

Pusher* pusher = nullptr;

extern "C" JNIEXPORT void JNICALL Java_com_example_rtmp_core_Pusher_native_1Init(
    JNIEnv* env, jobject thiz, jint fps, jint bit_rate, jint sample_rate, jint channels) {
  pusher = new Pusher(fps, bit_rate, sample_rate, channels);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_rtmp_core_Pusher_native_1Prepare(JNIEnv* env, jobject thiz, jstring path_) {
  const char* path = env->GetStringUTFChars(path_, 0);
  char* url = new char[strlen(path) + 1];
  strcpy(url, path);
  if (!pusher) {
    return;
  }
  pusher->Prepare(url);
  env->ReleaseStringUTFChars(path_, path);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_rtmp_core_Pusher_native_1Stop(JNIEnv* env, jobject thiz) {
  if (!pusher) {
    return;
  }
  pusher->Stop();
}

extern "C" JNIEXPORT void JNICALL Java_com_example_rtmp_core_Pusher_native_1AudioPush(
    JNIEnv* env, jobject thiz, jbyteArray bytes) {
  if (!pusher || !pusher->is_ready_pushing_) {
    return;
  }
  jbyte* data = env->GetByteArrayElements(bytes, NULL);
  pusher->EncodeAudioData(data);
  env->ReleaseByteArrayElements(bytes, data, 0);
}

extern "C" JNIEXPORT void JNICALL Java_com_example_rtmp_core_Pusher_native_1VideoDataChange(
    JNIEnv* env, jobject thiz, jint width, jint height) {
  if (!pusher) {
    return;
  }
  pusher->VideoDataChange(width, height);
}

extern "C" JNIEXPORT void JNICALL Java_com_example_rtmp_core_Pusher_native_1VideoPush(
    JNIEnv* env, jobject thiz, jbyteArray bytes) {
  if (!pusher || !pusher->is_ready_pushing_) {
    return;
  }
  jbyte* data = env->GetByteArrayElements(bytes, NULL);
  pusher->EncodeVideoData(data);
  env->ReleaseByteArrayElements(bytes, data, 0);
}
extern "C" JNIEXPORT jint JNICALL
Java_com_example_rtmp_core_Pusher_native_1AudioGetSamples(JNIEnv* env, jobject thiz) {
  if (!pusher) {
    return -1;
  }
  return pusher->AudioGetSamples();
}