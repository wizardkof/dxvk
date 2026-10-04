project('dxvk-queue-cpu-regressions', 'cpp', default_options: ['cpp_std=c++17'])
cpp = meson.get_compiler('cpp')
queue_python = import('python').find_installation()
queue_submit_body = custom_target('queue_submit_body',
  input: '../../src/dxvk/dxvk_cmdlist.cpp', output: 'queue_submit_body.cpp',
  command: [queue_python, files('../generate_queue_submit.py'), '@INPUT@', '@OUTPUT@'])
queue_test = executable('test_queue_error',
  '../test_queue_error.cpp', '../queue_test_trace.cpp', queue_submit_body,
  '../../src/dxvk/dxvk_queue.cpp', '../../src/dxvk/dxvk_device_idle.cpp',
  cpp_args: ['-DDXVK_QUEUE_TEST'],
  include_directories: [include_directories('../../include/vulkan/include'), include_directories('..')],
  dependencies: [dependency('threads')])
foreach scenario : ['finish','submit','success','lost-finish','lost-submit','previous','pending','independent']
  test('queue-' + scenario, queue_test, args: scenario, timeout: 10)
endforeach
