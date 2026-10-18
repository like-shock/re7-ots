#include "native_options.h"
#include <string.h>
static const char setting_off[]="auto_disable_with_melee_weapons=0\n";
static const char setting_on[]="auto_disable_with_melee_weapons=1\n";
void native_options_init(NativeOptions*o){
 atomic_init(&o->requested,1);atomic_init(&o->result,2);atomic_init(&o->ready,0);
}
void native_options_load(NativeOptions*o,int value){
 unsigned v=value==0?0:1;
 atomic_store(&o->requested,v);atomic_store(&o->result,(uint64_t)v<<1);atomic_store(&o->ready,1);
}
int native_options_get(const NativeOptions*o,int*v,int*s){
 if(!atomic_load(&o->ready))return 0;
 unsigned requested=atomic_load(&o->requested);uint64_t result=atomic_load(&o->result);
 *v=(int)(requested&1);*s=(result>>1)!=requested?0:(result&1)?-1:1;return 1;
}
int native_options_request(NativeOptions*o,int value){
 if((value!=0&&value!=1)||!atomic_load(&o->ready))return 0;
 unsigned old=atomic_load(&o->requested),next;
 do{next=((old+2u)&~1u)|(unsigned)value;}
 while(!atomic_compare_exchange_weak(&o->requested,&old,next));
 return 1;
}
int native_options_pending(const NativeOptions*o,int*v,unsigned*t){
 if(!atomic_load(&o->ready))return 0;
 *t=atomic_load(&o->requested);*v=(int)(*t&1);
 return (atomic_load(&o->result)>>1)!=*t;
}
void native_options_complete(NativeOptions*o,unsigned token,int saved){
 atomic_store(&o->result,((uint64_t)token<<1)|(saved?0u:1u));
}
int native_options_parse(const char*data,size_t size,int*value){
 if(!data||!value||size!=sizeof setting_off-1)return 0;
 if(!memcmp(data,setting_off,size)){*value=0;return 1;}
 if(!memcmp(data,setting_on,size)){*value=1;return 1;}return 0;
}
size_t native_options_encode(int value,char*out,size_t capacity){
 if(!out||(value!=0&&value!=1)||capacity<sizeof setting_off-1)return 0;
 memcpy(out,value?setting_on:setting_off,sizeof setting_off-1);return sizeof setting_off-1;
}
