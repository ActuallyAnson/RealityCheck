// Minimal Box2D v2.4.1 repro: boxes (optionally bullets) dropped on a grid (no initial overlap) into a closed box. Reports bodies whose centre leaves [0,20]^2.
#include <vector>
#include <box2d/box2d.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
static void wall(b2World& w,float cx,float cy,float hw,float hh){b2BodyDef bd;bd.position.Set(cx,cy);b2Body*b=w.CreateBody(&bd);b2PolygonShape s;s.SetAsBox(hw,hh);b->CreateFixture(&s,0.f);}
int main(int argc,char**argv){
  int n=argc>1?atoi(argv[1]):30; int bullet=argc>2?atoi(argv[2]):1; float dt=argc>3?atof(argv[3]):1/60.f; float g=argc>4?atof(argv[4]):10; float hs=argc>5?atof(argv[5]):0.75f;
  int vi=argc>6?atoi(argv[6]):8, pi=argc>7?atoi(argv[7]):3; int steps=argc>8?atoi(argv[8]):600; int ccd=argc>9?atoi(argv[9]):1; float dens=argc>10?atof(argv[10]):1.f;
  b2World w(b2Vec2(0,-g)); w.SetContinuousPhysics(ccd);
  float W=20,H=20,t=1; wall(w,W/2,-t,W/2+t,t);wall(w,W/2,H+t,W/2+t,t);wall(w,-t,H/2,t,H/2+t);wall(w,W+t,H/2,t,H/2+t);
  std::vector<b2Body*> bs; // grid spacing 2*hs*1.2+ so no overlap
  float sp=hs*2.4f; int cols=(int)((W-2)/sp); if(cols<1)cols=1;
  for(int i=0;i<n;i++){b2BodyDef bd;bd.type=b2_dynamicBody;bd.bullet=bullet;bd.position.Set(1+hs+(i%cols)*sp,5+(i/cols)*sp);
    b2Body*b=w.CreateBody(&bd);b2PolygonShape s;s.SetAsBox(hs,hs);b2FixtureDef fd;fd.shape=&s;fd.density=dens;fd.friction=0.3f;b->CreateFixture(&fd);bs.push_back(b);}
  int maxout=0,first=-1;
  for(int k=0;k<steps;k++){w.Step(dt,vi,pi);int o=0;for(auto b:bs){b2Vec2 q=b->GetPosition();if(q.x<0||q.x>W||q.y<0||q.y>H)o++;}
    if(o&&first<0)first=k; if(o>maxout)maxout=o;}
  printf("n=%d bullet=%d dt=%g g=%g hs=%g vi=%d pi=%d ccd=%d -> maxout=%d firststep=%d\n",n,bullet,dt,g,hs,vi,pi,ccd,maxout,first);
}
