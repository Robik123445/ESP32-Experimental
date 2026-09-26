#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "experimental/ftm.h"
static ftm_t m;
static const ftm_config_t config={20000,200}; /* 20 MHz, 1 ms, max 100 kHz. */
static void init(void) { int32_t zero[]={0,0,0};assert(ftm_init(&m,&config,zero)); }
static void consume(int64_t pos[3],uint64_t *ticks) {
 ftm_event_t e;assert(ftm_pop(&m,&e)==FTM_OK);assert(e.ticks>=config.min_event_ticks);
 *ticks+=e.ticks;for(unsigned a=0;a<3;a++)if(e.step_mask&(1u<<a))pos[a]+=(e.direction_mask&(1u<<a))?-1:1;
}
int main(void) {
 init();assert(ftm_start(&m)==FTM_IDLE);ftm_event_t e;
 int64_t pos[3]={0};uint64_t ticks=0;
 for(unsigned n=1;n<=200000;n++) {
  double p[]={n*0.03125,n*0.0125,-(double)n*0.0078125};
  assert(ftm_submit(&m,p)==FTM_OK);assert(ftm_pump(&m)==FTM_OK);
  if(n==1)assert(ftm_start(&m)==FTM_OK);
  while(ftm_fill(&m))consume(pos,&ticks);
 }
 assert(pos[0]==6250 && pos[1]==2500 && pos[2]==-1563);assert(ticks==200000ULL*config.sample_ticks);
 assert(ftm_finish(&m)==FTM_OK);assert(ftm_pop(&m,&e)==FTM_END);assert(ftm_pop(&m,&e)==FTM_END);assert(!m.underruns);
 /* Burst, reverse and fractional endpoint; exact sample duration after DDA. */
 init();pos[0]=pos[1]=pos[2]=0;ticks=0;
 const double endpoints[][3]={{100,30,-50},{0,0,0},{-100,-31,50},{-99.5,-31.49,49.51}};
 for(unsigned i=0;i<4;i++) {
  assert(ftm_submit(&m,endpoints[i])==FTM_OK);assert(ftm_pump(&m)==FTM_OK);
  if(!i)assert(ftm_start(&m)==FTM_OK);
  while(ftm_fill(&m))consume(pos,&ticks);
 }
 assert(pos[0]==-100 && pos[1]==-31 && pos[2]==50);assert(ticks==80000);
 /* Starvation is latched, subsequent producer input cannot silently restart. */
 assert(ftm_pop(&m,&e)==FTM_UNDERRUN);assert(!e.step_mask);assert(m.underruns==1);
 assert(ftm_submit(&m,endpoints[0])==FTM_UNDERRUN);assert(ftm_pop(&m,&e)==FTM_UNDERRUN);assert(m.underruns==1);
 init();double p[]={101,0,0};assert(ftm_submit(&m,p)==FTM_RATE_LIMIT);assert(!ftm_fill(&m));
 init();p[0]=NAN;assert(ftm_submit(&m,p)==FTM_INVALID);
 init();p[0]=2147483648.;assert(ftm_submit(&m,p)==FTM_INVALID);
 /* Full FIFO is producer backpressure, not overwritten history. */
 init();p[0]=0;for(unsigned i=0;i<FTM_QUEUE_CAPACITY;i++){assert(ftm_submit(&m,p)==FTM_OK);assert(ftm_pump(&m)==FTM_OK);}
 assert(ftm_fill(&m)==FTM_QUEUE_CAPACITY);assert(ftm_submit(&m,p)==FTM_OK);assert(ftm_pump(&m)==FTM_FULL);
 assert(ftm_start(&m)==FTM_OK);assert(ftm_pop(&m,&e)==FTM_OK);assert(ftm_pump(&m)==FTM_OK);
 ftm_abort(&m);assert(ftm_pop(&m,&e)==FTM_ABORTED);assert(!e.step_mask);
 /* Monotonic SPSC counters may wrap without corrupting indexing. */
 init();m.head=m.tail=UINT32_MAX;assert(ftm_submit(&m,p)==FTM_OK);assert(ftm_pump(&m)==FTM_OK);
 assert(ftm_start(&m)==FTM_OK);assert(ftm_pop(&m,&e)==FTM_OK);assert(ftm_fill(&m)==0);
 printf("FTM tests passed; state=%zu bytes, event=%zu bytes\n",sizeof(m),sizeof(e));
}
