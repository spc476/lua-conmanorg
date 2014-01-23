/***************************************************************************
*
* Copyright 2011 by Sean Conner.
*
* This library is free software; you can redistribute it and/or modify it
* under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 3 of the License, or (at your
* option) any later version.
*
* This library is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
* License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this library; if not, see <http://www.gnu.org/licenses/>.
*
* Comments, questions and criticisms can be sent to: sean@conman.org
*
*************************************************************************/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <unistd.h>
#include <zlib.h>

extern char *optarg;
extern int   optind;
extern int   opterr;

/*************************************************************************/

int load_file(
	const char *const fname,
	unsigned char   **pdata,
	size_t     *const psize
)
{
  FILE          *fp;
  long           size;
  unsigned char *data;
  
  assert(fname != NULL);
  assert(pdata != NULL);
  assert(psize != NULL);
  
  *pdata = NULL;
  *psize = 0;
  
  fp = fopen(fname,"rb");
  if (fp == NULL)
    return errno;
  
  if (fseek(fp,0,SEEK_END) < 0)
  {
    int err = errno;
    fclose(fp);
    return err;
  }
  
  size = ftell(fp);
  if (size == -1)
  {
    int err = errno;
    fclose(fp);
    return err;
  }
  
  if (fseek(fp,0,SEEK_SET) < 0)
  {
    int err = errno;
    fclose(fp);
    return err;
  }
  
  data = malloc(size);
  if (data == NULL)
  {
    int err = errno;
    fclose(fp);
    return err;
  }
  
  *psize = size;
  *pdata = data;
  
  fread(data,1,size,fp);
  
  fclose(fp);
  return 0;
}

/*************************************************************************/

int compress_data(
	const int       level,
	unsigned char **pdata,
	const size_t    size,
	size_t         *newsize
)
{
  z_stream  sout;
  Bytef    *buffer;
  int       rc;
  
  if (level == 0)
  {
    *newsize = size;
    return 0;
  }
  
  buffer = malloc(size);
  if (buffer == NULL)
    return ENOMEM;
  
  sout.zalloc = Z_NULL;
  sout.zfree  = Z_NULL;
  sout.opaque = NULL;
  rc          = deflateInit(&sout,level);
  
  if (rc != Z_OK)
  {
    free(buffer);
    return EINVAL;
  }
  
  sout.next_in   = (Bytef *)*pdata;
  sout.avail_in  = size;
  sout.next_out  = buffer;
  sout.avail_out = size;
  
  deflate(&sout,Z_FINISH);
  deflateEnd(&sout);
  
  free(*pdata);
  *pdata   = (unsigned char *)buffer;
  *newsize = size - sout.avail_out;
  return 0;
}

/*************************************************************************/  

void process(
	FILE                *fpout,
	const unsigned char *data,
	const size_t         size,
	const size_t         ucsize,
	const char          *tag
)
{
  int cnt = 8;
  
  fprintf(
  	fpout,
  	"#include <stddef.h>\n"
  	"const unsigned char c_%s[] =\n"
  	"{"
  	"",
  	tag
  );
 
  for (size_t i = 0 ; i < size ; i++)
  {
    if (cnt == 8)
    {
      fputs("\n  ",fpout);
      cnt = 0;
    }
    
    fprintf(fpout,"0x%02X , ",data[i]);
    cnt++;
  }
  
  fprintf(
  	fpout,
  	"\n};\n\nconst size_t c_%s_size = %lu;\n",
  	tag,
  	(unsigned long)size
  );
  
  if (size != ucsize)
  {
    fprintf(
    	fpout,
    	"const size_t c_%s_ucsize = %lu;\n",
    	tag,
    	(unsigned long)ucsize
    );
  }
}

/**************************************************************************/

int main(int argc,char *argv[])
{
  FILE *fpout;
  char *tag;
  int   c;
  int   level;
  
  fpout = stdout;
  tag   = "data";
  level = 0;
  
  while((c = getopt(argc,argv,"o:t:hz0123456789")) != EOF)
  {
    switch(c)
    {
      default:
      case 'h': 
           fprintf(stderr,"usage: %s [-o output] [-t tag] [-z] [-h] files... \n",argv[0]);
           return EXIT_FAILURE;
      case 't':
           tag = optarg;
           for ( ; *optarg ; optarg++)
             if (*optarg == '-') 
               *optarg = '_';
           break;
           
      case 'o':
           fpout = fopen(optarg,"wb");
           if (fpout == NULL)
           {
             perror(optarg);
             exit(EXIT_FAILURE);
           }
           break;
      case 'z':
           level = Z_DEFAULT_COMPRESSION;
           break;
           
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
           level = c - '0';
           break;
    }
  }
  
  if (optind == argc)
  {
    fprintf(stderr,"usage: %s [-o output] [-t tag] [-z] [-h] files... \n",argv[0]);
    return EXIT_FAILURE;
  }
  else
  {
    for (int i = optind ; i < argc ; i++)
    {
      unsigned char *data;
      size_t         datasize;
      size_t         realdatasize;
      int            rc;
      
      rc = load_file(argv[i],&data,&realdatasize);
      if (rc != 0)
      {
        fprintf(stderr,"%s: %s\n",argv[i],strerror(rc));
        free(data);
        return EXIT_FAILURE;
      }
      
      rc = compress_data(level,&data,realdatasize,&datasize);
      if (rc != 0)
      {
        fprintf(stderr,"%s: %s\n",argv[1],strerror(rc));
        free(data);
        return EXIT_FAILURE;
      }

      process(fpout,data,datasize,realdatasize,tag);
      free(data);
    }
  }

  return EXIT_SUCCESS;
}
