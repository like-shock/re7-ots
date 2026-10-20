#include "native_options.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
static const char setting_off[]="auto_disable_with_melee_weapons=0\n";
static const char head_off[]="hide_head_in_first_person=0\n";
static const char contrast_off[]="limit_light_source_contrast_in_third_person=0\n";
void native_options_init(NativeOptions*o){
 const uint32_t defaults[]={1,0,1,0,0xffffff,0x3f800000,UINT32_MAX,UINT32_MAX};
 for(unsigned j=0;j<8;j++){atomic_init(&o->optional[j],defaults[j]);atomic_init(&o->optional_result[j],defaults[j]);}
 atomic_init(&o->requested,NATIVE_OPTIONS_DEFAULT);atomic_init(&o->result,NATIVE_OPTIONS_DEFAULT<<1);atomic_init(&o->ready,0);
}
void native_options_load(NativeOptions*o,int value){
 unsigned v=value>=0&&(unsigned)value<=NATIVE_OPTIONS_MASK?(unsigned)value:NATIVE_OPTIONS_DEFAULT;
 atomic_store(&o->requested,v);atomic_store(&o->result,(uint64_t)v<<1);atomic_store(&o->ready,1);
}
int native_options_get(const NativeOptions*o,int*v,int*s){
 if(!atomic_load(&o->ready))return 0;
 unsigned requested=atomic_load(&o->requested);uint64_t result=atomic_load(&o->result);
 *v=(int)(requested&NATIVE_OPTIONS_MASK);*s=(result>>1)!=requested?0:(result&1)?-1:1;return 1;
}
int native_options_request(NativeOptions*o,int value){
 if((value<0||(unsigned)value>NATIVE_OPTIONS_MASK)||!atomic_load(&o->ready))return 0;
 unsigned old=atomic_load(&o->requested),next;
 do{next=((old+NATIVE_OPTIONS_MASK+1u)&~NATIVE_OPTIONS_MASK)|(unsigned)value;}
 while(!atomic_compare_exchange_weak(&o->requested,&old,next));
 return 1;
}
int native_options_pending(const NativeOptions*o,int*v,unsigned*t){
 if(!atomic_load(&o->ready))return 0;
 *t=atomic_load(&o->requested);*v=(int)(*t&NATIVE_OPTIONS_MASK);
 return (atomic_load(&o->result)>>1)!=*t;
}
void native_options_complete(NativeOptions*o,unsigned token,int saved){
 atomic_store(&o->result,((uint64_t)token<<1)|(saved?0u:1u));
}
int native_options_request_field(NativeOptions*o,unsigned flag,int value){
 if((flag!=NATIVE_OPTION_MELEE&&flag!=NATIVE_OPTION_HIDE_HEAD&&flag!=NATIVE_OPTION_LIGHT_CONTRAST)||
    (value!=0&&value!=1)||!atomic_load(&o->ready))return 0;
 unsigned old=atomic_load(&o->requested),next;
 do{next=((old+NATIVE_OPTIONS_MASK+1u)&~NATIVE_OPTIONS_MASK)|((old&NATIVE_OPTIONS_MASK)&~flag)|(value?flag:0);}
 while(!atomic_compare_exchange_weak(&o->requested,&old,next));
 return 1;
}
int native_options_parse(const char*data,size_t size,int*value){
 const size_t a=sizeof setting_off-1,b=sizeof head_off-1,c=sizeof contrast_off-1;
 if(!data||!value||(size!=a&&size!=a+b&&size!=a+b+c))return 0;
 if(memcmp(data,setting_off,a-2)||data[a-1]!='\n'||(data[a-2]!='0'&&data[a-2]!='1'))return 0;
 int v=data[a-2]-'0';
 if(size==a)v|=NATIVE_OPTION_HIDE_HEAD; /* v0.1.2 migration */
 else{
  if(memcmp(data+a,head_off,b-2)||data[a+b-1]!='\n'||(data[a+b-2]!='0'&&data[a+b-2]!='1'))return 0;
  if(data[a+b-2]=='1')v|=NATIVE_OPTION_HIDE_HEAD;
 }
 if(size<a+b+c)v|=NATIVE_OPTION_LIGHT_CONTRAST; /* v0.1.2/v0.1.3 migration */
 else{
  if(memcmp(data+a+b,contrast_off,c-2)||data[a+b+c-1]!='\n'||(data[a+b+c-2]!='0'&&data[a+b+c-2]!='1'))return 0;
  if(data[a+b+c-2]=='1')v|=NATIVE_OPTION_LIGHT_CONTRAST;
 }
 *value=v;return 1;
}
size_t native_options_encode(int value,char*out,size_t capacity){
 const size_t a=sizeof setting_off-1,b=sizeof head_off-1,c=sizeof contrast_off-1;
 if(!out||value<0||(unsigned)value>NATIVE_OPTIONS_MASK||capacity<a+b+c)return 0;
 memcpy(out,setting_off,a);memcpy(out+a,head_off,b);memcpy(out+a+b,contrast_off,c);
 out[a-2]=(value&NATIVE_OPTION_MELEE)?'1':'0';
 out[a+b-2]=(value&NATIVE_OPTION_HIDE_HEAD)?'1':'0';
 out[a+b+c-2]=(value&NATIVE_OPTION_LIGHT_CONTRAST)?'1':'0';return a+b+c;
}

NativeSettings native_settings_defaults(void){return (NativeSettings){NATIVE_OPTIONS_DEFAULT,1,0,1,0,0xffffff,1,-1,-1};}
static const char*keys[]={"hide_mia_hair","force_custom_filter","custom_shader","fullbright","fullbright_rgb","fullbright_brightness","controller_camera_button","controller_shoulder_button"};
static double field(const NativeSettings*s,unsigned j){
 switch(j){case 0:return s->hide_mia_hair;case 1:return s->custom_filter;case 2:return s->shader;case 3:return s->fullbright;case 4:return s->rgb;case 5:return s->brightness;case 6:return s->camera_button;default:return s->shoulder_button;}
}
static void assign(NativeSettings*s,unsigned j,uint32_t v){
 switch(j){case 0:s->hide_mia_hair=(int)v;break;case 1:s->custom_filter=(int)v;break;case 2:s->shader=(int)v;break;case 3:s->fullbright=(int)v;break;case 4:s->rgb=v;break;case 5:memcpy(&s->brightness,&v,4);break;case 6:s->camera_button=v==UINT32_MAX?-1:(int)v;break;default:s->shoulder_button=v==UINT32_MAX?-1:(int)v;break;}
}
static int encode_field(unsigned j,double value,uint32_t*out){
 if(!isfinite(value))return 0;
 if(j==5){if(value<0||value>10)return 0;float v=(float)value;memcpy(out,&v,4);return 1;}
 double min=j>=6?-1:0,max=j>=6?23:j==4?0xffffff:j==2?15:1;
 if(value<min||value>max||value!=(double)(int)value)return 0;
 *out=(uint32_t)(int)value;return 1;
}
void native_settings_load(NativeOptions*o,const NativeSettings*s){
 NativeSettings defaults=native_settings_defaults();uint32_t values[8];int valid=s&&s->flags>=0&&s->flags<=7;
 for(unsigned j=0;j<8&&valid;j++){valid=encode_field(j,field(s,j),&values[j]);}
 if(!valid){s=&defaults;for(unsigned j=0;j<8;j++){encode_field(j,field(s,j),&values[j]);}}
 atomic_store(&o->ready,0);
 for(unsigned j=0;j<8;j++){atomic_store(&o->optional[j],values[j]);atomic_store(&o->optional_result[j],values[j]);}
 native_options_load(o,s->flags);
}
static int sample(const NativeOptions*o,NativeSettings*s,NativeSettingsToken*t,int*status){
 if(!atomic_load(&o->ready))return 0;
 t->legacy=atomic_load(&o->requested);s->flags=(int)(t->legacy&NATIVE_OPTIONS_MASK);
 uint64_t old=atomic_load(&o->result);int pending=(old>>1)!=t->legacy,failed=(old&1)!=0;
 for(unsigned j=0;j<8;j++){
  t->optional[j]=atomic_load(&o->optional[j]);assign(s,j,(uint32_t)t->optional[j]);old=atomic_load(&o->optional_result[j]);
  pending|=(old&INT64_MAX)!=t->optional[j];failed|=(old>>63)!=0;
 }
 *status=pending?0:failed?-1:1;return 1;
}
int native_settings_get(const NativeOptions*o,NativeSettings*s,int*status){NativeSettingsToken token;return sample(o,s,&token,status);}
int native_settings_pending(const NativeOptions*o,NativeSettings*s,NativeSettingsToken*t){int status;return sample(o,s,t,&status)&&status==0;}
void native_settings_complete(NativeOptions*o,const NativeSettingsToken*t,int saved){
 native_options_complete(o,t->legacy,saved);
 for(unsigned j=0;j<8;j++){atomic_store(&o->optional_result[j],t->optional[j]|(saved?0:UINT64_C(1)<<63));}
}
int native_settings_request(NativeOptions*o,unsigned id,double value){
 if(id<8||id>15||!atomic_load(&o->ready))return 0;
 uint32_t v;if(!encode_field(id-8,value,&v))return 0;
 uint_fast64_t old=atomic_load(&o->optional[id-8]),next;
 do{next=((old+UINT64_C(0x100000000))&UINT64_C(0x7fffffff00000000))|v;}
 while(!atomic_compare_exchange_weak(&o->optional[id-8],&old,next));return 1;
}
int native_settings_parse(const char*data,size_t n,NativeSettings*out){
 if(!out)return 0;
 NativeSettings s=native_settings_defaults();if(native_options_parse(data,n,&s.flags)){*out=s;return 1;}
 if(!data||n<=108||n>=1024||!native_options_parse(data,108,&s.flags)||memchr(data,0,n))return 0;
 char buffer[1024];memcpy(buffer,data,n);buffer[n]=0;char*p=buffer+108;
 for(unsigned j=0;j<8;j++){
  size_t k=strlen(keys[j]);if(strncmp(p,keys[j],k)||p[k]!='=')return 0;
  p+=k+1;char*end;double v=strtod(p,&end);uint32_t encoded;
  if(end==p||*end!='\n'||!encode_field(j,v,&encoded))return 0;
  assign(&s,j,encoded);p=end+1;
 }
 if(*p)return 0;
 *out=s;return 1;
}
size_t native_settings_encode(const NativeSettings*s,char*out,size_t cap){
 if(!s)return 0;
 size_t n=native_options_encode(s->flags,out,cap);if(!n)return 0;
 for(unsigned j=0;j<8;j++){
  uint32_t v;if(!encode_field(j,field(s,j),&v)||n>=cap)return 0;
  int count=snprintf(out+n,cap-n,"%s=%.9g\n",keys[j],field(s,j));
  if(count<0||(size_t)count>=cap-n)return 0;
  n+=(size_t)count;
 }
 return n;
}
