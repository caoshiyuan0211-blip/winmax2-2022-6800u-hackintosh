// dump per pair, per candidate: probe tmpl ncc overlap np nt matched
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fpmatch.h"
static fp_img I[256]; static char nm[256][64];
int main(int c, char **v){int n=0;unsigned char b[FP_N];
for(int i=1;i<c;i++){FILE*f=fopen(v[i],"rb");fseek(f,-FP_N,SEEK_END);if(fread(b,1,FP_N,f)!=FP_N)return 1;fclose(f);fp_prep(b,&I[n]);
const char*s=strrchr(v[i],'/');strcpy(nm[n++],s?s+1:v[i]);}
#pragma omp parallel for schedule(dynamic)
for(int i=0;i<n;i++){char out[65536];int o=0;for(int j=0;j<n;j++){if(i==j||nm[j][0]!='a')continue;fp_result r;fp_match(&I[i],&I[j],&r);
for(int k=0;k<r.nc;k++)o+=snprintf(out+o,sizeof out-o,"%s %s %.4f %d %d %d %d %d\n",nm[i],nm[j],r.c[k].ncc,r.c[k].overlap,r.c[k].np,r.c[k].nt,r.c[k].matched,k);}
#pragma omp critical
fputs(out,stdout);}return 0;}
