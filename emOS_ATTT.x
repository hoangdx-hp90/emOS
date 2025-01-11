#ifndef ATTT_X__
#define ATTT_X__
#include <string.h>
#include "emOS.h"
//#include "unistd.h"
#define sprintf   h_sprintf /* Flawfinder: ignore */
#define printf    h_printf /* Flawfinder: ignore */
#define printf_t  h_printf /* Flawfinder: ignore */
#define usleep_t  usleep /* Flawfinder: ignore */
#define memcpy_t  memcpy /* Flawfinder: ignore */
#define sprintf_t    sprintf   /* Flawfinder: ignore */
#define strncpy_t    strncpy   /* Flawfinder: ignore */
#define strcpy_t  strcpy   /* Flawfinder: ignore */
#define popen_t popen   /* Flawfinder: ignore */
#define pclose_t pclose   /* Flawfinder: ignore */
#define open_t open   /* Flawfinder: ignore */
#define char_t char   /* Flawfinder: ignore */
#define atoi_t atoi   /* Flawfinder: ignore */
#define atol_t atol   /* Flawfinder: ignore */
#define atof_t atof   /* Flawfinder: ignore */
#define read_t read   /* Flawfinder: ignore */
#define fopen_t fopen   /* Flawfinder: ignore */ 
#define attt_fopen(f)   f.open   /* Flawfinder: ignore */
#define attt_fread(f)   f.read    /* Flawfinder: ignore */

#define sscanf_t   sscanf  /* Flawfinder: ignore */
#define sscanf_s_t    sscanf_s   /* Flawfinder: ignore */

#define execvp_t  execvp      /* Flawfinder: ignore */
#define atoi_t    atoi     /* Flawfinder: ignore */
#define strlen_t  strlen      /* Flawfinder: ignore */

#define ARRAY_INDEX3(a,index) a[index] 
#define ARRAY_INDEX2(a,index) ARRAY_INDEX3(a,index)
#define ARRAY_INDEX1(a,index) ARRAY_INDEX2(a,index)
#define ARRAY_INDEX(a,index) ARRAY_INDEX1(a,index)

#endif
