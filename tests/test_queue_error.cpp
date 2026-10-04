#include "queue_fixture_device.h"
#include <future>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#endif
using namespace dxvk;
using namespace std::chrono_literals;
static void require(bool b,const char* s){if(!b){std::printf("FAIL %s\n",s);std::fflush(stdout);std::_Exit(1);}}
template<class P> void until(P p,const char* s){auto end=std::chrono::steady_clock::now()+2s;while(!p()&&std::chrono::steady_clock::now()<end)std::this_thread::yield();require(p(),s);}
static void enqueue(DxvkDevice& d,Backend& b,DxvkSubmitStatus& s){s.result=VK_NOT_READY;d.m_submissionQueue.submit({new DxvkCommandList(&b,&d)},{},&s);}
static void drain(DxvkDevice& d){auto f=std::async(std::launch::async,[&]{d.waitForIdle();});require(f.wait_for(2s)==std::future_status::ready,"drain did not progress after Vulkan returned");f.get();}
int main(int argc,char** argv){
 require(argc==2,"case argument");std::string c=argv[1];Backend b;
 // DeviceFn, command-list boundary and resource observer are fixture-only.
 // All queue algorithms/workers and device waitForIdle body are production.
 if(c=="finish"||c=="lost-finish"){
  b.waitResult=c=="finish"?VK_ERROR_OUT_OF_HOST_MEMORY:VK_ERROR_DEVICE_LOST;
  auto d=std::make_unique<DxvkDevice>(&b);DxvkSubmitStatus s;enqueue(*d,b,s);
  until([&]{return b.waitReturned.load()>0;},"valid submitted entry reached Vulkan wait");
  require(s.result==VK_SUCCESS,"submit was accepted");drain(*d);
  require(d->m_submissionQueue.getLastError()==b.waitResult,"finish preserves exact VkResult");
  require(b.notified==0&&b.reset==0&&b.recycled==0,"unretired resources not released or recycled");
  d.reset();require(b.destroyed==0,"unretired list retained beyond queue destruction");
 }else if(c=="submit"||c=="lost-submit"){
  b.submitResult=c=="submit"?VK_ERROR_OUT_OF_HOST_MEMORY:VK_ERROR_DEVICE_LOST;
  auto d=std::make_unique<DxvkDevice>(&b);DxvkSubmitStatus s;enqueue(*d,b,s);
  until([&]{return s.result!=VK_NOT_READY;},"submission returned exact error");
  require(s.result==b.submitResult,"submit exact error publication");drain(*d);
  require(queue_test::recursiveLocks==0,"submit readquired owned mutex");
  require(d->m_submissionQueue.getLastError()==b.submitResult,"submission error retained");
  require(b.waits==0&&b.notified==0&&b.reset==0&&b.recycled==0,"possibly partial submit retained");
  d.reset();require(b.destroyed==0,"failed submission lifetime beyond teardown");
 }else if(c=="success"){
  DxvkDevice d(&b);DxvkSubmitStatus s;enqueue(d,b,s);drain(d);
  until([&]{return b.recycled.load()==1;},"retired list recycling finished");
  require(s.result==VK_SUCCESS&&b.waits==1&&b.notified==1&&b.reset==1&&b.recycled==1,"normal retirement and notification");
 }else if(c=="pending"||c=="independent"||c=="previous"){
  b.pending=true;DxvkDevice d(&b);DxvkSubmitStatus s;enqueue(d,b,s);
  until([&]{return b.entered.load();},"first accepted work is pending");
  auto f=std::async(std::launch::async,[&]{d.waitForIdle();});
  until([&]{return queue_test::finishPredicates.load()>0;},"waiter tested nonempty finish queue");
  auto n=queue_test::finishPredicates.load();d.m_submissionQueue.notifyCsError();
  until([&]{return queue_test::finishPredicates.load()>n;},"notified waiter rechecked unchanged predicate");
  require(f.wait_for(30ms)==std::future_status::timeout,"notification cannot fabricate completion");
  if(c=="independent") {Backend other;DxvkDevice d2(&other);DxvkSubmitStatus s2;enqueue(d2,other,s2);drain(d2);require(other.notified==1&&b.notified==0,"independent queues");}
  if(c=="previous") {
   b.submitResult=VK_ERROR_OUT_OF_HOST_MEMORY;DxvkSubmitStatus later;enqueue(d,b,later);
   until([&]{return later.result==VK_ERROR_OUT_OF_HOST_MEMORY;},"later operation failed");
   // synchronizeSubmission must finish while earlier accepted work remains pending.
   auto g=std::async(std::launch::async,[&]{d.m_submissionQueue.synchronizeSubmission(&later);});
   require(g.wait_for(2s)==std::future_status::ready,"failed later entry must leave submit queue");g.get();
   require(b.notified==0&&b.reset==0,"earlier accepted work not retired by later error");
  }
  require(b.waitReturned==0&&b.notified==0,"backend still pending without error");
  std::puts("MARK PENDING_QUERY_NOT_FIXED_BY_ERROR_HANDLING");b.release();
  require(f.wait_for(2s)==std::future_status::ready,"backend release permits drain");f.get();
  until([&]{return b.recycled.load()==1;},"successful earlier work retired");
  require(b.notified==1&&b.reset==1,"only verified completion releases resources");
 }else require(false,"unknown case");
#ifdef _WIN32
 require(!GetModuleHandleA("vulkan-1.dll")&&!GetModuleHandleA("vulkan_virtio.dll")&&!GetModuleHandleA("helios_umd.dll"),"no installed Vulkan loader/ICD/UMD loaded");
 std::puts("MARK NO_INSTALLED_GPU_MODULES");
#endif
 std::printf("PASS %s\n",c.c_str());return 0;
}
