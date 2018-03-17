#define SELF_COMPILE /*

#see https://gist.github.com/zb3/74125fb1af82c2474e2e4a8e4704cb96

TARGET=file1337
rm $TARGET 2>/dev/null
gcc -x c -o $TARGET - <<'SELF_COMPILE'

//*/


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <libgen.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>


#define MAX_FILENAME 512
#define MAX_PREAMBLE_LENGTH 1024
#define PREAMBLE_BUFSIZE MAX_FILENAME+MAX_PREAMBLE_LENGTH
#define BUFSIZE 4096

#define NEED_SHUTDOWN 1
#define NEED_CATMODE 2

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))


struct driver
{
 char *name;
 char *fmt;
 int flags; 
};

struct driver upload_drivers[] = {
  {"perl", "perl -e '($f,$s,$m,$o,$c)=@ARGV; binmode STDIN; select((select(STDOUT), $| = 1)[0]); print \"U\".\"PLOADREADY\"; open (FH, ($o?\">>\":\">\").\":raw\", $f) && chmod (oct $m, $f) && (!$o || seek FH, $o, 0) && print \"OK\"; print \"UP\".\"LOADREADY\"; while ($s) { $d = read (STDIN, $b, $s < $c ? $s : $c); ($d > 0) or exit; $s -= $d; print FH $b}print \"F\".\"ILEDONE\"' %s %d %03o %d %d"},
  {"shell", "F=%s;FS=%d;FM=%03o;O=%d;CH=%d;echo \"U\"PLOADREADY`>> $F && chmod $FM $F && echo OK`\"UP\"LOADREADY; cat %s $F; echo \"F\"ILEDONE", NEED_SHUTDOWN | NEED_CATMODE},
  NULL
};

struct driver download_drivers[] = {
  {"perl", "perl -e '$f=shift;$o=shift;@s = stat $f; printf \"F\".\"ILEINFO%%d %%03oFI\".\"LEINFO\\n\", $s[7], $s[2] & 0777; binmode STDOUT; open FH, \"<:raw\", $f; seek FH,$o*4096,0; while (read FH, $buf, 4096) {print $buf}' %s %d"},
  {"shell_nodd", "F=%s;O=%d;echo \"F\"ILEINFO`stat -L -c '%%s %%a' $F`\"FI\"LEINFO;cat $F"},
  {"shell", "F=%s;O=%d;echo \"F\"ILEINFO`stat -L -c '%%s %%a' $F`\"FI\"LEINFO;dd if=$F bs=4096 skip=$O"},
  NULL
};



char *command = "x";
char *wanted_driver = "shell";
struct driver *driver;
int listen_port = 0, offset = 0;
char *send_addr = NULL, *send_port = "31337";
char *source_file_name = "file.283";
char target_name_buff[1024];
char *target_file_name = NULL;

void fail(const char *fmt, ...)
{
 va_list args;
 va_start(args, fmt);
 vprintf(fmt, args);
 va_end(args);
 
 printf("\n");
 fflush(stdout);
 
 exit(1);
}

  
void parse_args(int argc, char **argv)
{
 if (argc < 5)
 {
  printf("%s d|u [-d driver] [-o offset] -l listen_port source_file [target_file = basename]\n", argv[0]);
  printf("%s d|u [-d driver] [-o offset] HOST PORT source_file [target_file = basename]\n", argv[0]);
  exit(1);
 }
 
 command = argv[1][0] == 'u' ? "upload" : "download";
 
 int arg_offset = 2;
 int pos_read = 0;
 
 while(arg_offset < argc)
 {
  char more = arg_offset < argc-1;
    
  if (!strcmp(argv[arg_offset], "-l") && more)
  {
   listen_port = atoi(argv[++arg_offset]);
   pos_read += 2;
  }
  
  else if (!strcmp(argv[arg_offset], "-o") && more)
  offset = atoi(argv[++arg_offset]);
  
  else if (!strcmp(argv[arg_offset], "-a") && more)
  send_addr = argv[++arg_offset];
  
  else if (!strcmp(argv[arg_offset], "-p") && more)
  send_port = argv[++arg_offset];
  
  else if (!strcmp(argv[arg_offset], "-d") && more)
  wanted_driver = argv[++arg_offset];
  
  else
  {
   if (!pos_read)
   send_addr = argv[arg_offset];
   else if (pos_read == 1)
   send_port = argv[arg_offset];
   else if (pos_read == 2)
   source_file_name = argv[arg_offset];
   else if (pos_read == 3)
   target_file_name = argv[arg_offset];
   
   pos_read++;
  }

  arg_offset++;
 }
 
 if (!target_file_name)
 target_file_name = basename(source_file_name);
 
 if (target_file_name[strlen(target_file_name)-1] == '/')
 {
  snprintf(target_name_buff, sizeof(target_name_buff), "%s%s", target_file_name, basename(source_file_name));
  target_file_name = target_name_buff;
 }
 
 struct driver *drivers = *command == 'u' ? upload_drivers : download_drivers;
 
 // find driver
 for(struct driver *drv = drivers; drv->name; drv++)
 {
  if (!strcmp(wanted_driver, drv->name))
  {
   driver = drv;
   break;
  }
 }
}

int listen_and_get_fd(int port)
{
 int lsock = socket(AF_INET6, SOCK_STREAM, 0);
 if (lsock == -1) err(1, "Socket fail");
 
 int on = 1;
 setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
 
 on = 0;  
 if (setsockopt(lsock, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&on, sizeof(on)))
 err(2, "Can't turn IPV6_ONLY off. BSD?");
 
 struct sockaddr_in6 serveraddr, clientaddr;
 int addrlen = sizeof(clientaddr);
 
 memset(&serveraddr, 0, sizeof(serveraddr));
 serveraddr.sin6_family = AF_INET6;
 serveraddr.sin6_port   = htons(port);
 serveraddr.sin6_addr   = in6addr_any;
 
 if (bind(lsock, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
 err(3, "Bind fail");
 
 if (listen(lsock, 1) < 0)
 err(4, "Listen fail");
 
 int client;
 
 if ((client = accept(lsock, NULL, NULL)) < 0)
 err(5, "Accept fail");
 
 close(lsock);
 
 return client;
}

int connect_and_get_fd(char *addr, char *port)
{
 struct addrinfo hints;
 struct addrinfo *result, *ai;
 
 memset(&hints, 0, sizeof(struct addrinfo));
 hints.ai_family = AF_UNSPEC;
 hints.ai_flags = 0;
 hints.ai_protocol = 0;
 
 if (getaddrinfo(addr, port, &hints, &result) < 0)
 err(1, "getaddrinfo fail");
 
 int client;
 int cres;
 
 for(ai = result; ai != NULL; ai = ai->ai_next)
 {
  client = socket(ai->ai_family, SOCK_STREAM, 0);

  if (client == -1)
  continue;

  if ((cres = connect(client, ai->ai_addr, ai->ai_addrlen)) != -1)
  break;

  close(client);
 }
 
 if (cres == -1)
 err(2, "Connect fail");

 freeaddrinfo(result);
 
 return client;
}


int write_all(int fd, char *buff, size_t len)
{
 size_t written = 0;
 int ret;
 
 while(written < len)
 {
  ret = write(fd, buff+written, len-written);
  
  if (ret <= 0)
  return ret;
  
  written += ret;
 }
 
 return len;
}

char *memstr(char *haystack, size_t size, char *needle, size_t nsize)
{
 char *p;

 for (p = haystack; p <= (haystack-nsize+size); p++)
 {
  if (memcmp(p, needle, nsize) == 0)
  return p;
 }
 
 return NULL;
}

char *read_till(int fd, char *buffer, size_t bufsize, char *marker, size_t msize, size_t fill)
{
 char *ptr;
 
 while(1)
 {
  if (fill<bufsize)
  {
   int ret = read(fd, buffer+fill, bufsize-fill);
   if (ret <= 0)
   return NULL;
   
   fill += ret;
  }
  
  if (ptr = memstr(buffer, fill, marker, msize))
  {
   size_t left = buffer- ptr+ bufsize - msize ;
   memmove(buffer, ptr+msize, left);
   
   return buffer+fill-(ptr-buffer+msize);
  } 
  
  if (fill == bufsize)
  {
   memmove(buffer, buffer+bufsize-msize+1, msize-1);
   fill = msize-1;
  }
 }
 
 return NULL;
}

size_t extract_tag(int fd, char *buffer, size_t bufsize, char *marker, size_t msize, size_t fill)
{
 char *bret = read_till(fd, buffer, bufsize, marker, msize, fill);
 if (!bret)
 {
  *buffer = 0;
  return 0;
 }
 
 fill = bret-buffer;
 char *ptr;
  
 while (1)
 {
  if (ptr = memstr(buffer, fill, marker, msize))
  {
   buffer[ptr-buffer] = 0;
   break;
  }
  
  if (fill == bufsize-1)
  {
   buffer[bufsize-1] = 0;
   break;
  } 

  int ret = read(fd, buffer+fill, bufsize-1-fill);
  if (ret <= 0) break;
  
  fill += ret;
  if (ptr = memstr(buffer, fill, marker, msize))
  {
   buffer[ptr-buffer] = 0;
   break;
  } 
 }
 
 return fill;
}

void upload(int output_fd)
{
 int fd = open(source_file_name, O_RDONLY);
 if (fd==-1)
 err(1, "Can't open file");
 
 struct stat st;
 if (fstat(fd, &st)<0)
 err(2, "Can't stat");
 
 if (offset && lseek(fd, offset, SEEK_SET)<0)
 fail("Can't seek");
  
 char preamble[PREAMBLE_BUFSIZE];
 
 //yeah, define in the middle of a function...
 #define base_fmt_args target_file_name, st.st_size - offset, st.st_mode&0777, offset, 4096
  
 if (driver->flags & NEED_CATMODE)
 sprintf(preamble, driver->fmt, base_fmt_args, offset?">>":">");
 else
 sprintf(preamble, driver->fmt, base_fmt_args);
 
 strcat(preamble, "\n");

 //printf("Preamble is %s\n", preamble);
 //fflush(stdout);
 
 if (write_all(output_fd, preamble, strlen(preamble)) <= 0)
 err(3, "Can't write preamble");
 
 char buff[BUFSIZE] = {0};
 
 //read tag first
 extract_tag(output_fd, buff, BUFSIZE, "UPLOADREADY", strlen("UPLOADREADY"), 0);
 
 if (strncmp(buff, "OK", 2))
 fail("Can't start upload");
 
 printf("Starting upload...\n");
 
 while(1)
 {
  int ret = read(fd, buff, sizeof(buff));
  if (ret <= 0) break;
  
  if (write_all(output_fd, buff, ret) <= 0)
  err(4, "Can't write file");
 }
 
 //a TCP trick to signal EOF, without this we'd never get FILEDONE
 if (driver->flags & NEED_SHUTDOWN)
 shutdown(output_fd, SHUT_WR);
 
 if (!read_till(output_fd, buff, BUFSIZE, "FILEDONE", 8, 0))
 err(5, "Did not read marker");
 
 close(fd); 
}

void download(int conn_fd)
{
 //since some versions of dd don't support iflag to specify the offset in bytes, we'll skip by 4096 byte blocks
 int offset_blocks = offset >> 12;
 
 char preamble[PREAMBLE_BUFSIZE];
 sprintf(preamble, driver->fmt, source_file_name, offset_blocks);
 strcat(preamble, "\n");
 
 //printf("Preamble is: %s\n", preamble);
 //fflush(stdout);
 
 if (write_all(conn_fd, preamble, strlen(preamble)) <= 0)
 err(3, "Can't write preamble");

 char buffer[BUFSIZE];
 size_t whole_read = extract_tag(conn_fd, buffer, BUFSIZE, "FILEINFO", strlen("FILEINFO"), 0);
 int tag_length = strlen(buffer);
 
 size_t file_size;
 int file_mode;
 
 if (!tag_length)
 fail("Failed to read tag");
 
 if (sscanf(buffer, "%d %o", &file_size, &file_mode) < 2)
 fail("Could not read file info (%s)", buffer);
 
 if (offset_blocks)
 file_size -= offset_blocks << 12;
 
 int fd = open(target_file_name, O_WRONLY | O_CREAT | (offset_blocks?0:O_TRUNC));
 if (fd == -1)
 err(5, "Can't open file");
 
 fchmod(fd, file_mode);
 
 if (offset_blocks && lseek(fd, offset_blocks << 12, SEEK_SET)<0)
 fail("Can't seek");
 
 int received = 0;
 int tmp;
 
 if ((tmp = whole_read - tag_length - strlen("FILEINFO") - 1) > 0) //1 is for newline
 {
  write(fd, buffer+tag_length+strlen("FILEINFO")+1, tmp);
  received += tmp;
 }

 while(received < file_size)
 {
  tmp = read(conn_fd, buffer, MIN(BUFSIZE, file_size-received));
  if (tmp <= 0) break;
  
  if (write_all(fd, buffer, tmp) <= 0)
  err(7, "Can't write to file");
  
  received += tmp;
 }
 
 if (received < file_size)
 {
  printf("Warning: incomplete file received, got %d of %d bytes.\n", received, file_size);
 }
 
 close(fd);
}

int main(int argc, char **argv)
{
 parse_args(argc, argv);

 int conn_fd = 1;
 
 if (listen_port)
 conn_fd = listen_and_get_fd(listen_port);
 else if (send_addr)
 conn_fd = connect_and_get_fd(send_addr, send_port);
 
 if (*command == 'd')
 download(conn_fd);
 else
 upload(conn_fd);
 
 if (listen_port || send_addr)
 close(conn_fd);
}


SELF_COMPILE

#/*
./$F $@
#*/