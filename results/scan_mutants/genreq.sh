#!/bin/bash
# 300 request lines, seeds 1..300, all parameters drawn uniformly (density log-uniform, ints uniform) from the scan ranges; awk srand(20261101)
awk 'BEGIN{srand(20261101); for(i=1;i<=300;i++){
 b=1+int(rand()*30); sz=0.1+rand()*1.1; e=rand(); f=rand(); d=exp(log(0.1)+rand()*(log(10)-log(0.1))); g=rand()*30; sp=rand()*60; dt=0.004+rand()*0.046;
 vi=1+int(rand()*12); pi=1+int(rand()*6); sl=int(rand()*2); sh=int(rand()*3); jo=int(rand()*2); bu=int(rand()*2); cc=int(rand()*2);
 printf "{\"scenario\":{\"seed\":%d,\"steps\":200,\"params\":{\"balls\":%d,\"size\":%.6g,\"restitution\":%.6g,\"friction\":%.6g,\"density\":%.6g,\"gravity\":%.6g,\"speed\":%.6g,\"dt\":%.6g,\"viter\":%d,\"piter\":%d,\"sleep\":%d,\"shape\":%d,\"joints\":%d,\"bullet\":%d,\"ccd\":%d}}}\n", i,b,sz,e,f,d,g,sp,dt,vi,pi,sl,sh,jo,bu,cc}}'
