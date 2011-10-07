
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

extern char *optarg;
extern int   optind;
extern int   opterr;

/*************************************************************************/

void process(FILE *fpin,FILE *fpout,const char *tag)
{
  size_t bytes = 0;
  int    cnt   = 8;
  int    c;
  
  fprintf(
  	fpout,
  	"#include <stddef.h>\n"
  	"const char g_%s[] =\n"
  	"{"
  	"",
  	tag
  );
  
  while((c = fgetc(fpin)) != EOF)
  {
    if (cnt == 8)
    {
      fputs("\n  ",fpout);
      cnt = 0;
    }
    
    fprintf(fpout,"0x%02X , ",c);
    cnt++;
    bytes++;
  }
  
  fprintf(fpout,"\n};\n\n#define BINSIZE_%s %lu\n",tag,(unsigned long)bytes);
}

/**************************************************************************/

int main(int argc,char *argv[])
{
  FILE *fpout;
  char *tag;
  int   c;
  
  fpout = stdout;
  tag   = "data";
  
  while((c = getopt(argc,argv,"o:t:h")) != EOF)
  {
    switch(c)
    {
      default:
      case 'h': 
           fprintf(stderr,"usage: %s [-o output] [-t tag] [-h] files... \n",argv[0]);
           return EXIT_FAILURE;
      case 't':
           tag = optarg;
           break;
      case 'o':
           fpout = fopen(optarg,"w");
           if (fpout == NULL)
           {
             perror(optarg);
             exit(EXIT_FAILURE);
           }
           break;
    }
  }
  
  if (optind == argc)
    process(stdin,fpout,tag);
  else
  {
    int i;
    
    for (i = optind ; i < argc ; i++)
    {
      FILE *fpin;
      
      fpin = fopen(argv[i],"rb");
      if (fpin == NULL)
        perror(argv[i]);
      else
      {
        process(fpin,fpout,tag);
        fclose(fpin);
      }
    }
  }
  
  return EXIT_SUCCESS;
}
