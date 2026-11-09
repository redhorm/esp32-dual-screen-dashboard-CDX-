#include <unity.h>
#include "core/button_gesture.h"
#include "core/anim.h"
void setUp(){} void tearDown(){}
void short_press(){
 ButtonGesture b;TEST_ASSERT_EQUAL_INT(0,(int)b.update(true,10));
 b.update(true,50);b.update(false,100);
 TEST_ASSERT_EQUAL_INT((int)ButtonAction::TOGGLE_SHOW,(int)b.update(false,140));
 TEST_ASSERT_EQUAL_INT(0,(int)b.update(false,180));
}
void bounce_ignored(){
 ButtonGesture b;b.update(true,10);b.update(false,20);b.update(true,30);b.update(false,40);
 TEST_ASSERT_EQUAL_INT(0,(int)b.update(false,100));
}
void long_press_does_not_start_show_on_release(){
 ButtonGesture b;b.update(true,0);b.update(true,40);
 TEST_ASSERT_EQUAL_INT((int)ButtonAction::REVEAL_WIFI,(int)b.update(true,2040));
 TEST_ASSERT_EQUAL_INT(0,(int)b.update(true,5000));b.update(false,5050);
 TEST_ASSERT_EQUAL_INT(0,(int)b.update(false,5090));
}
void wraps_millis(){
 ButtonGesture b;uint32_t t=UINT32_MAX-20;
 b.update(true,t);b.update(true,t+40);
 TEST_ASSERT_EQUAL_INT((int)ButtonAction::REVEAL_WIFI,(int)b.update(true,t+2040));
 b.update(false,t+2100);TEST_ASSERT_EQUAL_INT(0,(int)b.update(false,t+2140));
}
void animation_wrap(){TEST_ASSERT_FLOAT_WITHIN(.001f,.5f,Anim::progress(49,UINT32_MAX-50,200));}
int main(){UNITY_BEGIN();RUN_TEST(short_press);RUN_TEST(bounce_ignored);RUN_TEST(long_press_does_not_start_show_on_release);RUN_TEST(wraps_millis);RUN_TEST(animation_wrap);return UNITY_END();}
