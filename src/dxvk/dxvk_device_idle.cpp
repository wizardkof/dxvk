#ifdef DXVK_QUEUE_TEST
#include "../../tests/queue_fixture_device.h"
#else
#include "dxvk_device.h"
#endif

namespace dxvk {
  void DxvkDevice::waitForIdle() {
    m_submissionQueue.waitForIdle();
    m_submissionQueue.lockDeviceQueue();

    if (m_vkd->vkDeviceWaitIdle(m_vkd->device()) != VK_SUCCESS)
      Logger::err("DxvkDevice: waitForIdle: Operation failed");

    m_submissionQueue.unlockDeviceQueue();
  }

}
