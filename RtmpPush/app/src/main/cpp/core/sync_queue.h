/// @file sync_queue.h
/// @brief 线程安全队列, 作为编码器和推流中间的缓冲区
/// @author xhunmon
/// @last_editor lq

#include <pthread.h>
#include <queue>

template<typename T>
class SyncQueue {
  typedef void (* ReleaseCallback)(T &);

 public:
  SyncQueue() {
    pthread_mutex_init(&mutex_, NULL);
    pthread_cond_init(&cond_, NULL);
  }

  ~SyncQueue() {
    pthread_mutex_destroy(&mutex_);
    pthread_cond_destroy(&cond_);
  }

  int push(T t) {
    pthread_mutex_lock(&mutex_);
    if (work_) {
      data_.push(t);
      pthread_cond_signal(&cond_);
    } else {
      //不允许为空，否者会造成内存泄露
      release_callback_(t);
    }
    pthread_mutex_unlock(&mutex_);
    return 0;
  }

  /**
   * @param t 传递一个指针进来，然后给该指针指向一个值
   * @return 0为失败
   */
  int pop(T &t) {
    int ret = 0;
    pthread_mutex_lock(&mutex_);
    // 在多核处理器下 由于竞争可能虚假唤醒 包括jdk也说明了
    while (work_ && data_.empty()) {
      pthread_cond_wait(&cond_, &mutex_);
    }
    if (!data_.empty()) {
      t = data_.front();
      data_.pop();
      ret = 1;
    }
    pthread_mutex_unlock(&mutex_);
    return ret;
  }

  void setReleaseCallback(ReleaseCallback release) { this->release_callback_ = release; }

  void setWork(int w) {
    pthread_mutex_lock(&mutex_);
    this->work_ = w;
    pthread_cond_signal(&cond_);
    pthread_mutex_unlock(&mutex_);
  }

  void clear() {
    pthread_mutex_lock(&mutex_);
    while (!data_.empty()) {
      T t = data_.front();
      data_.pop();
      release_callback_(t);
    }
    pthread_mutex_unlock(&mutex_);
  }

  int size() {
    int ret = 0;
    pthread_mutex_lock(&mutex_);
    ret = data_.size();
    pthread_mutex_unlock(&mutex_);
    return ret;
  }

 private:
  std::queue<T> data_;
  pthread_mutex_t mutex_;
  pthread_cond_t cond_;
  int work_;

  ReleaseCallback release_callback_;
};