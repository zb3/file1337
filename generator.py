import os
import base64

shell_init = b"> '$FILE'; chmod $MODE '$FILE'"

drivers = {
   'shell_en': {
     'init': shell_init,
     'chunk': b"echo -en '$HEX' >> '$FILE'"
   },
   'shell': {
     'init': shell_init,
     'chunk': b"echo '$B64' | base64 -di >> '$FILE'"
   },
   'perl': {
     'init': b"""perl -e 'open FH, ">", "$FILE"; chmod oct "$MODE";'""",
     'chunk': b"""perl -e 'use MIME::Base64; open FH, ">>:raw", "$FILE"; print FH decode_base64("$B64")'"""
   }
}

def encode_hex(data):
  if type(b'zb3'[0]) != int:
    ret = b''.join(['\\x%02x' % ord(c) for c in data])
  else:
    ret = ''.join(['\\x%02x' % c for c in data]).encode('ascii')
    
  return ret


def upload_file(name, target_name=None, chunk_size=130000, driver='shell'):
  if not target_name:
    target_name = os.path.basename(name)

  if type(target_name) != bytes:
    target_name = target_name.encode()
    
  info = os.stat(name)
  driver = drivers[driver]
  
  mode = '%03o' % (info.st_mode&0x1ff)
  
  if type(mode) != bytes:
    mode = mode.encode()
    
  init = driver['init'].replace(b'$MODE', mode).replace(b'$FILE', target_name)
  
  yield init
  
  if b'$B64' in driver['chunk']:
    mode = 'base64'
    chunk_size = chunk_size*3//4
  else:
    mode = 'hex'
    chunk_size = chunk_size//4
  
  with open(name, 'rb') as f:
    while True:
      data = f.read(chunk_size)
      if not data:
        break
      
      chunk = driver['chunk']
    
      if mode == 'base64':
        chunk = chunk.replace(b'$B64', base64.b64encode(data))
      
      elif mode == 'hex':
        chunk = chunk.replace(b'$HEX', encode_hex(data))
    
      chunk = chunk.replace(b'$FILE', target_name)
      
      yield chunk
 
