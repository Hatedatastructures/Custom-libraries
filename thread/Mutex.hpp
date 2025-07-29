#include <pthread.h>
#include <unistd.h>
#include <mutex>
namespace con
{
  class mutex
  {
  private:
    pthread_mutex_t* _thread_mutex;
    bool _mutex_status_markers = false;
  public:
    mutex():_thread_mutex(nullptr),_mutex_status_markers(true){pthread_mutex_init(_thread_mutex,nullptr);}
    explicit mutex(pthread_mutex_t* _mutex_pointer):_thread_mutex(_mutex_pointer),_mutex_status_markers(false){}
    mutex(const mutex& ) = delete;
    mutex& operator=(const mutex&)= delete;
    mutex(mutex&& temporary_mutex)
    {
      if(temporary_mutex._thread_mutex != nullptr)
      {
        _thread_mutex = temporary_mutex._thread_mutex;
        temporary_mutex._thread_mutex = nullptr;
        _mutex_status_markers = true;
      }
    }
    mutex& operator=(mutex&& temporary_mutex)
    {
      if(this != &temporary_mutex)
      {
        if(_mutex_status_markers != false)
        {
          pthread_mutex_destroy(_thread_mutex);
        }
        _thread_mutex = temporary_mutex._thread_mutex;
        temporary_mutex._thread_mutex = nullptr;
        temporary_mutex._mutex_status_markers = false;
      }
      return *this;
    }
    bool lock()
    {
      if(_mutex_status_markers)
      {
        pthread_mutex_lock(_thread_mutex);
        return true;
      }
      return false;
    }
    bool unlock()
    {
      if(_mutex_status_markers)
      {
        pthread_mutex_unlock(_thread_mutex);
        return true;
      }
      return false;
    }
    ~mutex()
    {
      if(_mutex_status_markers)
      {
        pthread_mutex_destroy(_thread_mutex);
        _thread_mutex = nullptr;
      }
    }
  };
  template<typename concurrent_mutex>
  class lock_guard
  {
  private:
    concurrent_mutex& _concurrent_mutex;
  public:
    lock_guard(pthread_mutex_t* _thread_mutex_value)
    : _concurrent_mutex(_thread_mutex_value){_concurrent_mutex.lock();}
    lock_guard(concurrent_mutex& _mutex_value){_concurrent_mutex.lock();}
    ~lock_guard()
    {
      _concurrent_mutex.unlock();
    }
  };
}
