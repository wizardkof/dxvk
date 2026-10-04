"""Compile the exact production Vulkan submit method, never a copied algorithm."""
import hashlib
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text(encoding='utf-8')
a = source.index('  VkResult DxvkCommandSubmission::submit(')
b = source.index('\n  void DxvkCommandSubmission::reset()', a)
body = source[a:b]
Path(sys.argv[2]).write_text('#include "queue_fixture_device.h"\nnamespace dxvk {\n' + body + '\n}\n', encoding='utf-8')
print('production DxvkCommandSubmission::submit sha256=' + hashlib.sha256(body.encode()).hexdigest())
