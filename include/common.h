#ifndef _COMMON_H_
#define _COMMON_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <float.h>
#include <errno.h>
#include <time.h>
#include <termios.h>
#include <inttypes.h>

#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { SUCCESS=0, ERROR=1 };

#ifndef STDFS
#define STDFS stderr
#endif

#define LOG_LEVEL_DEBUG   5
#define LOG_LEVEL_STATUS  4
#define LOG_LEVEL_INFO    3
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR   1
#define LOG_LEVEL_NONE    0

#ifndef logLevel
#define logLevel LOG_LEVEL_DEBUG
#endif

// TODO: make lvl<=logLevel a cpp macro

#define MESSAGE(fs,lvl,prefix,...)        do{if(lvl<=logLevel) {fprintf (fs,           "%18s %3d %18s %s",    __FILE__,__LINE__,__func__,prefix);fprintf (fs,__VA_ARGS__);fprintf(fs, "\n");        fflush(fs);}}while(0)
#define MESSAGEn(fs,lvl,prefix,...)       do{if(lvl<=logLevel) {fprintf (fs,           "%18s %3d %18s %s",    __FILE__,__LINE__,__func__,prefix);fprintf (fs,__VA_ARGS__);                          fflush(fs);}}while(0)
#define MESSAGEc(fs,lvl,prefix,col,...)   do{if(lvl<=logLevel) {fprintf (fs, "\033[0;%dm%18s %3d %18s %s",col,__FILE__,__LINE__,__func__,prefix);fprintf (fs,__VA_ARGS__);fprintf(fs, "\033[0m\n"); fflush(fs);}}while(0)
#define MESSAGEcn(fs,lvl,prefix,col,...)  do{if(lvl<=logLevel) {fprintf (fs, "\033[0;%dm%18s %3d %18s %s",col,__FILE__,__LINE__,__func__,prefix);fprintf (fs,__VA_ARGS__);fprintf(fs, "\033[0m");   fflush(fs);}}while(0)

#define fprint_debug(fs,...)    MESSAGE   (fs, 5, "debug: ",   __VA_ARGS__)
#define fprint_status(fs,...)   MESSAGE   (fs, 4, "status: ",  __VA_ARGS__)
#define fprint_info(fs,...)     MESSAGE   (fs, 3, "info: ",    __VA_ARGS__)
#define fprint_warning(fs,...)  MESSAGE   (fs, 2, "warning: ", __VA_ARGS__)
#define fprint_error(fs,...)    MESSAGE   (fs, 1, "error: ",   __VA_ARGS__)
#define fprint_abort(fs,...)    MESSAGE   (fs, 1, "abort: ",   __VA_ARGS__)
#define fprint(fs,...)          MESSAGE   (fs, 0, "",  __VA_ARGS__)
#define fprintn(fs,...)         MESSAGEn  (fs, 0, "",  __VA_ARGS__)
#define fprint_col(fs,col,...)  MESSAGEc  (fs, 0, "", col,  __VA_ARGS__)
#define fprintn_col(fs,col,...) MESSAGEcn (fs, 0, "", col,  __VA_ARGS__)

#define fexit_debug(fs,...)    do {fprint_debug   (fs, __VA_ARGS__); exit (1); } while (0)
#define fexit_status(fs,...)   do {fprint_status  (fs, __VA_ARGS__); exit (1); } while (0)
#define fexit_info(fs,...)     do {fprint_info    (fs, __VA_ARGS__); exit (1); } while (0)
#define fexit_warning(fs,...)  do {fprint_warning (fs, __VA_ARGS__); exit (1); } while (0)
#define fexit_error(fs,...)    do {fprint_error   (fs, __VA_ARGS__); exit (1); } while (0)
#define fexit_abort(fs,...)    do {fprint_abort   (fs, __VA_ARGS__); exit (1); } while (0)

#define print_debug(...)       fprint_debug   (STDFS, __VA_ARGS__)
#define print_status(...)      fprint_status  (STDFS, __VA_ARGS__)
#define print_info(...)        fprint_info    (STDFS, __VA_ARGS__)
#define print_warning(...)     fprint_warning (STDFS, __VA_ARGS__)
#define print_error(...)       fprint_error   (STDFS, __VA_ARGS__)
#define print(...)             fprint         (STDFS, __VA_ARGS__)
#define printn(...)            fprintn        (STDFS, __VA_ARGS__)

#define ASCII_RED         31
#define ASCII_GREEN       32
#define ASCII_YELLOW      33
#define ASCII_BLUE        34
#define ASCII_PURPLE      35
#define ASCII_CYAN        36
#define ASCII_GRAY        37

#define print_red(...)         fprint_col     (STDFS, ASCII_RED   , __VA_ARGS__)
#define print_green(...)       fprint_col     (STDFS, ASCII_GREEN , __VA_ARGS__)
#define print_yellow(...)      fprint_col     (STDFS, ASCII_YELLOW, __VA_ARGS__)
#define print_blue(...)        fprint_col     (STDFS, ASCII_BLUE  , __VA_ARGS__)
#define print_purple(...)      fprint_col     (STDFS, ASCII_PURPLE, __VA_ARGS__)
#define print_cyan(...)        fprint_col     (STDFS, ASCII_CYAN  , __VA_ARGS__)
#define print_gray(...)        fprint_col     (STDFS, ASCII_GRAY  , __VA_ARGS__)

#define exit_debug(...)        fexit_debug    (STDFS, __VA_ARGS__)
#define exit_error(...)        fexit_error    (STDFS, __VA_ARGS__)
#define exit_abort(...)        fexit_abort    (STDFS, __VA_ARGS__)
#define exit_warning(...)      fexit_warning  (STDFS, __VA_ARGS__)
#define exit_status(...)       fexit_status   (STDFS, __VA_ARGS__)
#define exit_info(...)         fexit_info     (STDFS, __VA_ARGS__)

#define fprintf_exit(fs,...) do {fprintf(fs,__VA_ARGS__); fprintf(fs,"\n"); exit(1); } while (0)
#define printf_exit(...)  fprintf_exit (STDFS,__VA_ARGS__)

#define wtf exit_error ("wtf")
#define FOO print_debug ("foo")
#define BAR print_debug ("bar")
#define foobar print_debug ("foobar");

#define assert_verbose(test,...) if(!(test))exit_error(__VA_ARGS__)

#define equal(foo,bar) (strcmp(foo,bar)==0)
#define catch_nan(x) if (isnan(x)){exit_error ("caught nan! %s", #x);}
#define sign(x) (((x) > 0) - ((x) < 0))

double date_string_to_double (char* str);
char* double_to_date_string (double uClock);
double get_time (void);

#ifdef __cplusplus
} /* end extern C */
#endif

#endif /* _COMMON_H_ */
