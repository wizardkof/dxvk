#pragma once
#include "../src/dxvk/dxvk_queue.h"
namespace dxvk {
struct FakeObjects { struct Memory {void performTimedTasks(){}}; Memory memory; Memory& memoryManager(){return memory;} };
class DxvkDevice {
 FakeDeviceFn fn;
 FakeDeviceFn* m_vkd=&fn;
public:
 FakeObjects m_objects;
 DxvkSubmissionQueue m_submissionQueue;
 explicit DxvkDevice(Backend* b):fn{b},m_submissionQueue(this,{}){}
 struct Features {bool nvLowLatency2=false;} flags;
 const Features& features(){return flags;}
 FakeDeviceFn* vkd(){return &fn;}
 DxvkCheckpointBuffer* getCheckpointBuffer(){return nullptr;}
 void recycleCommandList(const Rc<DxvkCommandList>& c){c->backend->recycled++;}
 void waitForIdle();
};
class QueueTestAccess {
public:
 static void event(const char* name,DxvkSubmissionQueue* q){
  if (std::string(name)=="finish-predicate") {
   queue_test::finishPredicates++;std::printf("MARK finish-predicate role=%s submit=%zu finish=%zu owned=%d\n",queue_test::role,q->m_submitQueue.size(),q->m_finishQueue.size(),q->m_mutex.owned());std::fflush(stdout);
  } else queue_test::marker(name);
 }
};
namespace queue_test { inline void event(const char* n,DxvkSubmissionQueue* q){QueueTestAccess::event(n,q);} }
}

namespace dxvk {
inline VkResult DxvkCommandList::submit(const DxvkTimelineSemaphores&,DxvkTimelineSemaphoreValues& t,uint64_t id){
 t.graphics++; DxvkCommandSubmission submission;
 return submission.submit(device,reinterpret_cast<VkQueue>(backend),id);
}
}
