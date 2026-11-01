// mostest a*.pgm -- probes...   multi-mosaic from enrol set; per probe best score at several min-overlap fractions
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "fpmosaic.h"
static fp_img E[64], Pr[256]; static char pn[256][64]; static fp_mosaic mo[32];
static int load(const char *f, fp_img *o){unsigned char b[FP_N];FILE*F=fopen(f,"rb");if(!F)return -1;fseek(F,-FP_N,SEEK_END);int k=fread(b,1,FP_N,F);fclose(F);if(k!=FP_N)return -1;fp_prep(b,o);return 0;}
int main(int c,char**v){int ne=0,np=0,side=0;
for(int i=1;i<c;i++){if(!strcmp(v[i],"--")){side=1;continue;}if(side){if(!load(v[i],&Pr[np])){const char*s=strrchr(v[i],'/');strcpy(pn[np++],s?s+1:v[i]);}}else load(v[i],&E[ne++]);}
clock_t t0=clock();
int nm=fp_mosaic_build_all(mo,32,E,ne,atof(getenv("ADDTHR")),atoi(getenv("ADDOV")));
printf("mosaic: %d mosaics from %d presses (%.1f s cpu): sizes",nm,ne,(double)(clock()-t0)/CLOCKS_PER_SEC);for(int i=0;i<nm;i++)printf(" %d",mo[i].n);printf("\n");
#pragma omp parallel for schedule(dynamic)
for(int i=0;i<np;i++){char line[512];int o=snprintf(line,sizeof line,"%s",pn[i]);float fr[]={0.5f,0.7f,0.85f,0.95f};
for(int k=0;k<4;k++){float best=-1;for(int m=0;m<nm;m++){fp_mres r;float s=fp_mosaic_match(&Pr[i],&mo[m],fr[k],1500,&r);if(s>best)best=s;}o+=snprintf(line+o,sizeof line-o," %.3f",best);}
#pragma omp critical
puts(line);}
return 0;}
