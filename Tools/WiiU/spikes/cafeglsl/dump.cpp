#include <cafeglsl/CafeGLSLCompiler.h>
#include <cstdio>
#include <string>
#include <fstream>
#include <sstream>
static std::string rd(const char*p){std::ifstream f(p);std::stringstream s;s<<f.rdbuf();return s.str();}
template<class S> void common(const char*t,S*s){
 printf(" %s: mode=%d blocks=%u vars=%u init=%u samplers=%u\n",t,(int)s->mode,s->uniformBlockCount,s->uniformVarCount,s->initialValueCount,s->samplerVarCount);
 for(unsigned i=0;i<s->uniformBlockCount;i++)printf("  block[%u] %s offset(binding)=%u size=%u\n",i,s->uniformBlocks[i].name,s->uniformBlocks[i].offset,s->uniformBlocks[i].size);
 for(unsigned i=0;i<s->uniformVarCount;i++)printf("  var %s type=%d count=%u offset=%u block=%d\n",s->uniformVars[i].name,(int)s->uniformVars[i].type,s->uniformVars[i].count,s->uniformVars[i].offset,s->uniformVars[i].block);
 for(unsigned i=0;i<s->samplerVarCount;i++)printf("  sampler %s type=%d location=%u\n",s->samplerVars[i].name,(int)s->samplerVars[i].type,s->samplerVars[i].location);
}
int main(int c,char**v){
 InitGLSLCompiler();
 for(int a=1;a<c;a++){
  std::string n=v[a],vs=rd((n+".vert").c_str()),fs=rd((n+".frag").c_str());
  GX2VertexShader*V;GX2PixelShader*P;char log[4096];
  auto m=CompileShaderPair(vs.c_str(),fs.c_str(),GLSL_COMPILE_AUTO,&V,&P,log,sizeof log,GLSL_COMPILER_FLAG_NONE);
  printf("== %s mode=%d\n",v[a],(int)m);
  if(!V){printf("%s\n",log);continue;}
  common("VS",V);
  for(unsigned i=0;i<V->attribVarCount;i++)printf("  attrib %s type=%d count=%u location=%u\n",V->attribVars[i].name,(int)V->attribVars[i].type,V->attribVars[i].count,V->attribVars[i].location);
  printf("  vs out ids=%u, sq_vtx_semantic=%u\n",V->regs.num_spi_vs_out_id,V->regs.num_sq_vtx_semantic);
  common("PS",P);
  printf("  ps inputs=%u\n",P->regs.num_spi_ps_input_cntl);
 }
}
