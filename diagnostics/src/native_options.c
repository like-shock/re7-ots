#include "native_options.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
static const char setting_off[]="auto_disable_with_melee_weapons=0\n";
static const char head_off[]="hide_head_in_first_person=0\n";
static const char contrast_off[]="limit_light_source_contrast_in_third_person=0\n";
static double field(const NativeSettings*,unsigned);
static int encode_field(unsigned,double,uint32_t*);
void native_options_init(NativeOptions*o){
 NativeSettings defaults=native_settings_defaults();
 for(unsigned j=0;j<NATIVE_OPTIONAL_COUNT;j++){
  uint32_t value=0;encode_field(j,field(&defaults,j),&value);
  atomic_init(&o->optional[j],value);atomic_init(&o->optional_result[j],value);
 }
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

NativeSettings native_settings_defaults(void){
 return (NativeSettings){.flags=NATIVE_OPTIONS_DEFAULT,.hide_mia_hair=1,.shader=1,
  .rgb=0xffffff,.brightness=1,.camera_button=-1,.shoulder_button=-1,
  .camera_distance=1,.camera_shoulder=1,.custom_fov=110,.tp_run_fov=1,
  .run_fov_amount=20,.run_fov_rise=7,.run_fov_fall=.7f};
}
enum {SETTING_INT,SETTING_UINT,SETTING_FLOAT};
typedef struct {const char*key;size_t offset;unsigned kind;double min,max;} SettingSpec;
#define FIELD(key,name,kind,lo,hi) {key,offsetof(NativeSettings,name),kind,lo,hi}
static const SettingSpec fields[NATIVE_OPTIONAL_COUNT]={
 FIELD("hide_mia_hair",hide_mia_hair,SETTING_INT,0,1),
 FIELD("force_custom_filter",custom_filter,SETTING_INT,0,1),
 FIELD("custom_shader",shader,SETTING_INT,0,15),
 FIELD("fullbright",fullbright,SETTING_INT,0,1),
 FIELD("fullbright_rgb",rgb,SETTING_UINT,0,0xffffff),
 FIELD("fullbright_brightness",brightness,SETTING_FLOAT,0,10),
 FIELD("controller_camera_button",camera_button,SETTING_INT,-1,23),
 FIELD("controller_shoulder_button",shoulder_button,SETTING_INT,-1,23),
 FIELD("camera_distance_scale",camera_distance,SETTING_FLOAT,.5,2),
 FIELD("camera_shoulder_scale",camera_shoulder,SETTING_FLOAT,0,2),
 FIELD("camera_height_offset",camera_height,SETTING_FLOAT,-.5,.5),
 FIELD("use_custom_fov",use_custom_fov,SETTING_INT,0,1),
 FIELD("custom_fov",custom_fov,SETTING_FLOAT,60,120),
 FIELD("first_person_running_fov",fp_run_fov,SETTING_INT,0,1),
 FIELD("third_person_running_fov",tp_run_fov,SETTING_INT,0,1),
 FIELD("running_fov_increase_percent",run_fov_amount,SETTING_FLOAT,0,40),
 FIELD("running_fov_rise_seconds",run_fov_rise,SETTING_FLOAT,.1,10),
 FIELD("running_fov_fall_seconds",run_fov_fall,SETTING_FLOAT,.1,5)
};
#undef FIELD
_Static_assert(sizeof(int)==4&&sizeof(float)==4,"settings packed field widths");
static double field(const NativeSettings*s,unsigned j){
 const void*p=(const char*)s+fields[j].offset;
 if(fields[j].kind==SETTING_FLOAT){float v;memcpy(&v,p,4);return v;}
 if(fields[j].kind==SETTING_UINT){uint32_t v;memcpy(&v,p,4);return v;}
 int v;memcpy(&v,p,4);return v;
}
static void assign(NativeSettings*s,unsigned j,uint32_t v){memcpy((char*)s+fields[j].offset,&v,4);}
static int encode_field(unsigned j,double value,uint32_t*out){
 const SettingSpec*f=&fields[j];
 if(!isfinite(value)||value<f->min||value>f->max)return 0;
 if(f->kind==SETTING_FLOAT){float v=(float)value;memcpy(out,&v,4);return 1;}
 if(value!=(double)(int)value)return 0;
 *out=(uint32_t)(int)value;return 1;
}
void native_settings_load(NativeOptions*o,const NativeSettings*s){
 NativeSettings defaults=native_settings_defaults();uint32_t values[NATIVE_OPTIONAL_COUNT];int valid=s&&s->flags>=0&&s->flags<=7;
 for(unsigned j=0;j<NATIVE_OPTIONAL_COUNT&&valid;j++){valid=encode_field(j,field(s,j),&values[j]);}
 if(!valid){s=&defaults;for(unsigned j=0;j<NATIVE_OPTIONAL_COUNT;j++){encode_field(j,field(s,j),&values[j]);}}
 atomic_store(&o->ready,0);
 for(unsigned j=0;j<NATIVE_OPTIONAL_COUNT;j++){atomic_store(&o->optional[j],values[j]);atomic_store(&o->optional_result[j],values[j]);}
 native_options_load(o,s->flags);
}
static int sample(const NativeOptions*o,NativeSettings*s,NativeSettingsToken*t,int*status){
 if(!atomic_load(&o->ready))return 0;
 t->legacy=atomic_load(&o->requested);s->flags=(int)(t->legacy&NATIVE_OPTIONS_MASK);
 uint64_t old=atomic_load(&o->result);int pending=(old>>1)!=t->legacy,failed=(old&1)!=0;
 for(unsigned j=0;j<NATIVE_OPTIONAL_COUNT;j++){
  t->optional[j]=atomic_load(&o->optional[j]);assign(s,j,(uint32_t)t->optional[j]);old=atomic_load(&o->optional_result[j]);
  pending|=(old&INT64_MAX)!=t->optional[j];failed|=(old>>63)!=0;
 }
 *status=pending?0:failed?-1:1;return 1;
}
int native_settings_get(const NativeOptions*o,NativeSettings*s,int*status){NativeSettingsToken token;return sample(o,s,&token,status);}
int native_settings_pending(const NativeOptions*o,NativeSettings*s,NativeSettingsToken*t){int status;return sample(o,s,t,&status)&&status==0;}
void native_settings_complete(NativeOptions*o,const NativeSettingsToken*t,int saved){
 native_options_complete(o,t->legacy,saved);
 for(unsigned j=0;j<NATIVE_OPTIONAL_COUNT;j++){atomic_store(&o->optional_result[j],t->optional[j]|(saved?0:UINT64_C(1)<<63));}
}
int native_settings_request(NativeOptions*o,unsigned id,double value){
 if(id<8||id>=8+NATIVE_OPTIONAL_COUNT||!atomic_load(&o->ready))return 0;
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
 for(unsigned j=0;j<NATIVE_OPTIONAL_COUNT;j++){
  /* Only the complete pre-camera format may end here; partial new records fail. */
  if(j==8&&!*p){*out=s;return 1;}
  size_t k=strlen(fields[j].key);
  if((size_t)(buffer+n-p)<=k||strncmp(p,fields[j].key,k)||p[k]!='=')return 0;
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
 for(unsigned j=0;j<NATIVE_OPTIONAL_COUNT;j++){
  uint32_t v;if(!encode_field(j,field(s,j),&v)||n>=cap)return 0;
  int count=snprintf(out+n,cap-n,"%s=%.9g\n",fields[j].key,field(s,j));
  if(count<0||(size_t)count>=cap-n)return 0;
  n+=(size_t)count;
 }
 return n;
}
