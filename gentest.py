import generator
import subprocess

f = '/usr/bin/bash'

for cmd in generator.upload_file(f, target_name='tst_en', driver='shell_en', chunk_size=130000):
  subprocess.Popen(cmd, shell=True)

for cmd in generator.upload_file(f, target_name='tst_sh', driver='shell', chunk_size=130000):
  subprocess.Popen(cmd, shell=True)
  
for cmd in generator.upload_file(f, target_name='tst_perl', driver='perl', chunk_size=130000):
  subprocess.Popen(cmd, shell=True)
  