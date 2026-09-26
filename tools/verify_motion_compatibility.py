#!/usr/bin/env python3
"""Compare compiled all-off motion code and immutable original baseline."""
import json, subprocess, re
from pathlib import Path
root=Path(__file__).resolve().parent.parent
objdump=Path('/home/robert/.espressif/tools/xtensa-esp32s3-elf/esp-2021r2-patch5-8.4.0/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-objdump')
results={}
for source in ['grbl/planner','grbl/stepper','grbl/gcode','grbl/protocol','grbl/motion_control','grbl/state_machine','driver']:
    outputs=[]
    for profile in ['baseline','off']:
        p=root/'_experimental'/('build-'+profile)/'esp-idf/main/CMakeFiles/__idf_main.dir'/(source+'.c.obj')
        text=subprocess.check_output([str(objdump),'-dr',str(p)],text=True)
        outputs.append(re.sub(r'\$[0-9]+', '$LOCAL', '\n'.join(text.splitlines()[3:])))
    results[source]=outputs[0]==outputs[1]
assert all(results.values()),results
print(json.dumps({'all_off_disassembly_and_relocations_identical':results},indent=2))
