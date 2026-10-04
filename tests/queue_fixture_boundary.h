#pragma once
// Test-only dependencies. The production queue and device idle method are
// compiled unchanged; no Vulkan loader, instance, KMD or GPU is opened.
#include <vulkan/vulkan.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include "../src/util/rc/util_rc.h"
#include "../src/util/rc/util_rc_ptr.h"
namespace dxvk {
inline constexpr uint32_t MaxNumQueuedCommandBuffers=32;
class DxvkSubmissionQueue;
namespace queue_test {
inline thread_local const char* role = "caller";
inline std::atomic<unsigned> recursiveLocks {0}, idleEntries {0}, finishPredicates {0};
void event(const char*, DxvkSubmissionQueue*);
inline void marker(const char* s) { std::printf("MARK %s role=%s\n",s,role); std::fflush(stdout); }
}
class mutex {
#ifdef _WIN32
  SRWLOCK m = SRWLOCK_INIT;
#else
  std::mutex m;
#endif
  std::atomic<size_t> owner {0};
  static size_t id() { return std::hash<std::thread::id>{}(std::this_thread::get_id()) | 1; }
public:
  bool owned() const { return owner.load()==id(); }
  void lock() {
    if (owned()) { queue_test::recursiveLocks++; queue_test::marker("OWNED_MUTEX_REACQUIRE"); }
#ifdef _WIN32
    AcquireSRWLockExclusive(&m);
#else
    m.lock();
#endif
    owner=id();
  }
  void unlock() { owner=0;
#ifdef _WIN32
    ReleaseSRWLockExclusive(&m);
#else
    m.unlock();
#endif
  }
#ifdef _WIN32
  void sleep(CONDITION_VARIABLE* cv) {
    owner=0;
    SleepConditionVariableSRW(cv,&m,INFINITE,0);
    owner=id();
  }
#endif
};
class condition_variable {
#ifdef _WIN32
  CONDITION_VARIABLE cv = CONDITION_VARIABLE_INIT;
public:
  template<class L,class P> void wait(L& l,P p) { while(!p()) l.mutex()->sleep(&cv); }
  void notify_all() { WakeAllConditionVariable(&cv); }
#else
  std::condition_variable_any cv;
public:
  template<class L,class P> void wait(L& l,P p) { cv.wait(l,p); }
  void notify_all() { cv.notify_all(); }
#endif
};

using thread=std::thread;
using high_resolution_clock=std::chrono::steady_clock;
using DxvkQueueCallback=std::function<void(bool)>;
using DxvkError=std::runtime_error;
namespace env { inline void setThreadName(const char* n) { queue_test::role=n; } }
namespace str { template<typename... T> std::string format(T&&... t) { std::ostringstream s; (s << ... << t); return s.str(); } }
struct Logger { static void err(const std::string& s) { std::printf("ERROR %s\n",s.c_str()); std::fflush(stdout); } };
struct DxvkTimelineSemaphores { VkSemaphore graphics{},transfer{}; };
struct DxvkTimelineSemaphoreValues { uint64_t graphics=0,transfer=0; };
struct DxvkCheckpointBuffer { void printHangInfo() {} };
struct DxvkLatencyTracker:RcObject {
 void notifyQueueSubmit(uint64_t){} void notifyQueuePresent(uint64_t){}
 void notifyQueuePresentBegin(uint64_t){} void notifyQueuePresentEnd(uint64_t,VkResult){}
 void notifyGpuExecutionBegin(uint64_t){} void notifyGpuExecutionEnd(uint64_t){}
};
struct Presenter:RcObject { VkResult presentImage(uint64_t,const Rc<DxvkLatencyTracker>&){return VK_SUCCESS;} void signalFrame(uint64_t,const Rc<DxvkLatencyTracker>&){} };
struct Backend {
 std::atomic<int> submitResult{VK_SUCCESS},waitResult{VK_SUCCESS},idleResult{VK_SUCCESS};
 std::atomic<unsigned> submitted{0},waits{0},waitReturned{0},notified{0},reset{0},recycled{0},destroyed{0};
 std::atomic<bool> pending{false},entered{false};
 std::mutex gate; std::condition_variable changed;
 VkResult submit() { submitted++; queue_test::marker("VULKAN_QUEUE_SUBMIT_RETURN"); return VkResult(submitResult.load()); }
 VkResult wait() {
  waits++; entered=true; changed.notify_all();
  std::unique_lock<std::mutex> l(gate); changed.wait(l,[&]{return !pending.load();});
  waitReturned++; queue_test::marker("VULKAN_WAIT_RETURN");return VkResult(waitResult.load());
 }
 void release() { std::lock_guard<std::mutex> l(gate);pending=false;changed.notify_all(); }
};
class DxvkDevice;
class DxvkCommandSubmission {
 std::vector<VkSemaphoreSubmitInfo> m_semaphoreWaits, m_semaphoreSignals;
 std::vector<VkCommandBufferSubmitInfo> m_commandBuffers;
public:
 DxvkCommandSubmission() { VkCommandBufferSubmitInfo c{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO}; c.commandBuffer=reinterpret_cast<VkCommandBuffer>(uintptr_t(1));m_commandBuffers.push_back(c); }
 VkResult submit(DxvkDevice*,VkQueue,uint64_t);
 bool isEmpty() const {return m_semaphoreWaits.empty()&&m_semaphoreSignals.empty()&&m_commandBuffers.empty();}
 void reset(){m_semaphoreWaits.clear();m_semaphoreSignals.clear();m_commandBuffers.clear();}
};
namespace helios_feed {bool enabled();void vkQueueSubmit(uint64_t,uint64_t,uint64_t,uint64_t);}
struct DxvkCommandList:RcObject {
 Backend* backend; DxvkDevice* device; explicit DxvkCommandList(Backend* b,DxvkDevice* d):backend(b),device(d){}
 ~DxvkCommandList(){backend->destroyed++;}
 VkResult waitProducers(std::atomic<bool>&){return VK_SUCCESS;}
 VkResult submit(const DxvkTimelineSemaphores&,DxvkTimelineSemaphoreValues& t,uint64_t);
 void producerSubmitted(bool){}
 void notifyObjects(){backend->notified++;}
 void reset(){backend->reset++;}
};
struct FakeDeviceFn {
 Backend* b;
 VkResult vkQueueSubmit2(VkQueue,uint32_t n,const VkSubmitInfo2* s,VkFence){ if(n!=1||s->commandBufferInfoCount!=1)throw DxvkError("invalid submit fixture");return b->submit();}
 VkDevice device(){return reinterpret_cast<VkDevice>(b);}
 VkResult vkCreateSemaphore(VkDevice,const VkSemaphoreCreateInfo*,void*,VkSemaphore* s){*s=reinterpret_cast<VkSemaphore>(b);return VK_SUCCESS;}
 void vkDestroySemaphore(VkDevice,VkSemaphore,void*){}
 VkResult vkWaitSemaphores(VkDevice,const VkSemaphoreWaitInfo*,uint64_t){return b->wait();}
 VkResult vkDeviceWaitIdle(VkDevice){queue_test::marker("VULKAN_DEVICE_IDLE");return VkResult(b->idleResult.load());}
};
}
