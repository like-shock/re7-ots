#include "native_options.h"
#include <string.h>
static const char setting_off[]="auto_disable_with_melee_weapons=0\n";
static const char head_off[]="hide_head_in_first_person=0\n";
void native_options_init(NativeOptions*o){
 atomic_init(&o->requested,NATIVE_OPTIONS_DEFAULT);atomic_init(&o->result,NATIVE_OPTIONS_DEFAULT<<1);atomic_init(&o->ready,0);
}
void native_options_load(NativeOptions*o,int value){
 unsigned v=value>=0&&value<=3?(unsigned)value:NATIVE_OPTIONS_DEFAULT;
 atomic_store(&o->requested,v);atomic_store(&o->result,(uint64_t)v<<1);atomic_store(&o->ready,1);
}
int native_options_get(const NativeOptions*o,int*v,int*s){
 if(!atomic_load(&o->ready))return 0;
 unsigned requested=atomic_load(&o->requested);uint64_t result=atomic_load(&o->result);
 *v=(int)(requested&3);*s=(result>>1)!=requested?0:(result&1)?-1:1;return 1;
}
int native_options_request(NativeOptions*o,int value){
 if((value<0||value>3)||!atomic_load(&o->ready))return 0;
 unsigned old=atomic_load(&o->requested),next;
 do{next=((old+4u)&~3u)|(unsigned)value;}
 while(!atomic_compare_exchange_weak(&o->requested,&old,next));
 return 1;
}
int native_options_pending(const NativeOptions*o,int*v,unsigned*t){
 if(!atomic_load(&o->ready))return 0;
 *t=atomic_load(&o->requested);*v=(int)(*t&3);
 return (atomic_load(&o->result)>>1)!=*t;
}
void native_options_complete(NativeOptions*o,unsigned token,int saved){
 atomic_store(&o->result,((uint64_t)token<<1)|(saved?0u:1u));
}
int native_options_request_field(NativeOptions*o,unsigned flag,int value){
 if((flag!=NATIVE_OPTION_MELEE&&flag!=NATIVE_OPTION_HIDE_HEAD)||
    (value!=0&&value!=1)||!atomic_load(&o->ready))return 0;
 unsigned old=atomic_load(&o->requested),next;
 do{next=((old+4u)&~3u)|((old&3u)&~flag)|(value?flag:0);}
 while(!atomic_compare_exchange_weak(&o->requested,&old,next));
 return 1;
}
int native_options_parse(const char*data,size_t size,int*value){
 const size_t a=sizeof setting_off-1,b=sizeof head_off-1;
 if(!data||!value||(size!=a&&size!=a+b))return 0;
 if(memcmp(data,setting_off,a-2)||data[a-1]!='\n'||(data[a-2]!='0'&&data[a-2]!='1'))return 0;
 int v=data[a-2]-'0';
 if(size==a)v|=NATIVE_OPTION_HIDE_HEAD; /* v0.1.2 migration */
 else{
  if(memcmp(data+a,head_off,b-2)||data[a+b-1]!='\n'||(data[a+b-2]!='0'&&data[a+b-2]!='1'))return 0;
  if(data[a+b-2]=='1')v|=NATIVE_OPTION_HIDE_HEAD;
 }
 *value=v;return 1;
}
size_t native_options_encode(int value,char*out,size_t capacity){
 const size_t a=sizeof setting_off-1,b=sizeof head_off-1;
 if(!out||value<0||value>3||capacity<a+b)return 0;
 memcpy(out,setting_off,a);memcpy(out+a,head_off,b);
 out[a-2]=(value&NATIVE_OPTION_MELEE)?'1':'0';
 out[a+b-2]=(value&NATIVE_OPTION_HIDE_HEAD)?'1':'0';return a+b;
}
