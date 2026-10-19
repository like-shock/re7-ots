#include "native_options.h"
#include <assert.h>
#include <string.h>
int main(void){
 NativeOptions o;native_options_init(&o);int value=0,status=99;unsigned token;
 assert(!native_options_get(&o,&value,&status));
 assert(!native_options_request(&o,0));
 assert(native_options_parse("auto_disable_with_melee_weapons=0\n",34,&value)&&value==6);
 assert(native_options_parse("auto_disable_with_melee_weapons=1\n",34,&value)&&value==7);
 assert(!native_options_parse("auto_disable_with_melee_weapons=2\n",34,&value));
 assert(!native_options_parse("auto_disable_with_melee_weapons=0\nx",35,&value));
 assert(!native_options_parse("",0,&value));
 native_options_load(&o,0);
 assert(native_options_get(&o,&value,&status)&&value==0&&status==1);
 assert(!native_options_pending(&o,&value,&token));
 assert(!native_options_request(&o,8));
 assert(native_options_request(&o,1));
 assert(native_options_get(&o,&value,&status)&&value==1&&status==0);
 assert(native_options_pending(&o,&value,&token)&&value==1);
 unsigned first=token;
 /* Checkbox changes during slow file IO: completion cannot mark new value saved. */
 assert(native_options_request(&o,0));native_options_complete(&o,first,1);
 assert(native_options_get(&o,&value,&status)&&value==0&&status==0);
 assert(native_options_pending(&o,&value,&token)&&value==0&&token!=first);
 native_options_complete(&o,token,0);
 assert(native_options_get(&o,&value,&status)&&value==0&&status==-1);
 assert(!native_options_pending(&o,&value,&token)); /* no 20ms write retry loop */
 assert(native_options_request(&o,0)); /* explicit retry of failed same value */
 assert(native_options_pending(&o,&value,&token));native_options_complete(&o,token,1);
 assert(native_options_get(&o,&value,&status)&&value==0&&status==1);
 /* v0.1.3 configurations retain both settings; only the new limiter defaults ON. */
 assert(native_options_parse("auto_disable_with_melee_weapons=0\nhide_head_in_first_person=0\n",62,&value)&&value==4);
 assert(native_options_parse("auto_disable_with_melee_weapons=1\nhide_head_in_first_person=0\n",62,&value)&&value==5);
 assert(native_options_parse("auto_disable_with_melee_weapons=0\nhide_head_in_first_person=1\n",62,&value)&&value==6);
 /* Two checkbox updates must preserve the other setting, even before save. */
 assert(native_options_request(&o,3));
 assert(native_options_request_field(&o,1,0));
 assert(native_options_get(&o,&value,&status)&&value==2);
 assert(native_options_request_field(&o,2,0));
 assert(native_options_get(&o,&value,&status)&&value==0);
 assert(!native_options_request_field(&o,3,1));
 assert(!native_options_request_field(&o,2,7));
 /* Toggling one field during an outstanding save must retain all three bits. */
 assert(native_options_request(&o,7));
 assert(native_options_request_field(&o,4,0));
 assert(native_options_get(&o,&value,&status)&&value==3);
 assert(native_options_pending(&o,&value,&token));first=token;
 assert(native_options_request_field(&o,2,0));
 assert(native_options_request_field(&o,4,1));native_options_complete(&o,first,1);
 assert(native_options_get(&o,&value,&status)&&value==5&&status==0);
 assert(!native_options_request_field(&o,8,0));
 char buffer[128];size_t size=native_options_encode(0,buffer,sizeof buffer);
 assert(size==108&&native_options_parse(buffer,size,&value)&&value==0);
 assert(!native_options_encode(0,buffer,1));
 for(int mask=0;mask<8;mask++){
  size=native_options_encode(mask,buffer,sizeof buffer);
  assert(size==108&&native_options_parse(buffer,size,&value)&&value==mask);
 }
 assert(!native_options_parse("auto_disable_with_melee_weapons=0\nhide_head_in_first_person=2\n",62,&value));
 /* Truncation, invalid bool and appended garbage cannot overwrite preferences. */
 size=native_options_encode(7,buffer,sizeof buffer);value=5;
 assert(!native_options_parse(buffer,size-1,&value)&&value==5);
 buffer[size-2]='2';assert(!native_options_parse(buffer,size,&value)&&value==5);
 buffer[size-2]='1';buffer[size]='x';assert(!native_options_parse(buffer,size+1,&value)&&value==5);
 value=0;
 NativeOptions restart;native_options_init(&restart);native_options_load(&restart,value);
 assert(native_options_get(&restart,&value,&status)&&value==0&&status==1);
 return 0;
}
