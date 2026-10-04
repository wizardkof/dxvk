cs_test_deps = [dxvk_dep, dxbc_spirv_dep, vkcommon_dep, dependency('threads')]
if platform == 'windows'
  foreach name : ['gdi32', 'user32', 'ole32', 'oleaut32', 'version', 'advapi32', 'shell32', 'cfgmgr32']
    cs_test_deps += cpp.find_library(name)
  endforeach
endif
cs_failure_test = executable('test_cs_failure', 'test_cs_failure.cpp',
  dependencies: cs_test_deps,
  include_directories: [dxvk_include_path, include_directories('../subprojects/dxbc-spirv')])
test('cs_failure', cs_failure_test, timeout: 60)

# CPU-only production queue translation units with external dependencies isolated.
queue_python = import('python').find_installation()
queue_submit_body = custom_target('queue_submit_body',
  input: '../src/dxvk/dxvk_cmdlist.cpp', output: 'queue_submit_body.cpp',
  command: [queue_python, files('generate_queue_submit.py'), '@INPUT@', '@OUTPUT@'])
queue_error_test = executable('test_queue_error',
  'test_queue_error.cpp', 'queue_test_trace.cpp', queue_submit_body,
  '../src/dxvk/dxvk_queue.cpp', '../src/dxvk/dxvk_device_idle.cpp',
  cpp_args: ['-DDXVK_QUEUE_TEST'],
  include_directories: [dxvk_include_path, include_directories('.')],
  dependencies: dependency('threads'))
foreach scenario : ['finish', 'submit', 'success', 'lost-finish', 'lost-submit', 'previous', 'pending', 'independent']
  test('queue-' + scenario, queue_error_test, args: scenario, timeout: 10)
endforeach
