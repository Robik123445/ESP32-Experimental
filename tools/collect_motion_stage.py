#!/usr/bin/env python3
"""Collect completed build matrix and host evidence without touching a device."""
import json,subprocess,sys,shutil
from pathlib import Path
root=Path(__file__).resolve().parent.parent
stage=sys.argv[1]
assert stage.replace('-','').replace('_','').isalnum()
out=root/'docs/evidence'/stage
out.mkdir(parents=True,exist_ok=True)
result={}
for profile in ('baseline','off','scurve','ftm','bench','all'):
 build=root/'_experimental'/('build-'+profile)
 size=json.loads(subprocess.check_output(['/home/robert/.espressif/python_env/idf4.4_py3.12_env/bin/python','/home/robert/esp/esp-idf/tools/idf_size.py','--json',str(build/'grbl.map')]))
 log=(root/'_experimental'/stage/(profile+'.log')).read_text()
 assert 'error:' not in log and 'cmake failed' not in log
 result[profile]={'binary_bytes':(build/'grbl.bin').stat().st_size,'iram':size['used_iram'],'static_data_ram':size['used_diram'],'warning_classes':sorted(set(s.split('warning:',1)[1].strip() for s in log.splitlines() if 'warning:' in s))}
(out/'builds.json').write_text(json.dumps(result,indent=2)+'\n')
for name in ('core-baseline.csv','core-scurve.csv','core-ftm.csv','core-all.csv','core-shaped.csv','benchmark.csv'):
 p=root/'_experimental/logs'/name
 if p.exists():shutil.copyfile(p,out/name)
print(json.dumps(result,indent=2))
