// render: image | skeleton with minutiae (ending red, bifurcation green) as PPM grid
#include <stdio.h>
#include <string.h>
#include "fpmatch.h"
int main(int c,char**v){int S=3,n=c-2,W2=FP_W*2*S+8,H2=FP_H*S+4;static unsigned char o[64][FP_H*3*3][FP_W*2*3+8][3];
FILE*O=fopen(v[1],"wb");fprintf(O,"P6\n%d %d\n255\n",W2*4,H2*((n+3)/4));
static unsigned char canvas[16*400][4*450][3];memset(canvas,128,sizeof canvas);
for(int f=0;f<n;f++){unsigned char img[FP_N];FILE*F=fopen(v[f+2],"rb");fseek(F,-FP_N,SEEK_END);fread(img,1,FP_N,F);fclose(F);
fp_img im;fp_prep(img,&im);fp_minu m[128];unsigned char sk[FP_N];int k=fp_minutiae(&im,m,128,sk);fprintf(stderr,"%s %d minutiae\n",v[f+2],k);
int ox=(f%4)*W2,oy=(f/4)*H2;
for(int y=0;y<FP_H*S;y++)for(int x=0;x<FP_W*S;x++){int i=(y/S)*FP_W+x/S;for(int q=0;q<3;q++){canvas[oy+y][ox+x][q]=img[i];canvas[oy+y][ox+FP_W*S+4+x][q]=sk[i]?0:(im.m[i]?255:180);}}
for(int j=0;j<k;j++){int cx=m[j].x*S+1,cy=m[j].y*S+1;for(int dy=-3;dy<=3;dy++)for(int dx=-3;dx<=3;dx++){if(dx*dx+dy*dy>9||dx*dx+dy*dy<4)continue;
for(int side=0;side<2;side++){unsigned char*p=canvas[oy+cy+dy][ox+side*(FP_W*S+4)+cx+dx];p[0]=m[j].type==1?255:0;p[1]=m[j].type==3?200:0;p[2]=0;}}}}
for(int y=0;y<H2*((n+3)/4);y++)fwrite(canvas[y],3,W2*4,O);fclose(O);(void)o;return 0;}
