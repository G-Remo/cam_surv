#if CONFIG_IDF_TARGET_ESP32  // ESP32/PICO-D4
#include "esp32/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32C2
#include "esp32c2/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32C6
#include "esp32c6/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32H2
#include "esp32h2/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32P4
#include "esp32p4/rom/rtc.h"
#else
#error Target CONFIG_IDF_TARGET is not supported
#endif

String reset_cpu_0, reset_cpu_1;

String reset_reason(int reason)
{
  switch (reason)
  {
    case 1:  return "POWERON_RESET";          /**<1,  Vbat power on reset*/
    case 3:  return "SW_RESET";               /**<3,  Software reset digital core*/
    case 4:  return "OWDT_RESET";             /**<4,  Legacy watch dog reset digital core*/
    case 5:  return "DEEPSLEEP_RESET";        /**<5,  Deep Sleep reset digital core*/
    case 6:  return "SDIO_RESET";             /**<6,  Reset by SLC module, reset digital core*/
    case 7:  return "TG0WDT_SYS_RESET";       /**<7,  Timer Group0 Watch dog reset digital core*/
    case 8:  return "TG1WDT_SYS_RESET";       /**<8,  Timer Group1 Watch dog reset digital core*/
    case 9:  return "RTCWDT_SYS_RESET";       /**<9,  RTC Watch dog Reset digital core*/
    case 10: return "INTRUSION_RESET";        /**<10, Instrusion tested to reset CPU*/
    case 11: return "TGWDT_CPU_RESET";        /**<11, Time Group reset CPU*/
    case 12: return "SW_CPU_RESET";           /**<12, Software reset CPU*/
    case 13: return "RTCWDT_CPU_RESET";       /**<13, RTC Watch dog Reset CPU*/
    case 14: return "EXT_CPU_RESET";          /**<14, for APP CPU, reset by PRO CPU*/
    case 15: return "RTCWDT_BROWN_OUT_RESET"; /**<15, Reset when the vdd voltage is not stable*/
    case 16: return "RTCWDT_RTC_RESET";       /**<16, RTC Watch dog reset digital core and rtc module*/
    default: return "NO_MEAN";
  }

  return "NO_MEAN";
}

void init_reset()
{
  reset_cpu_0 = reset_reason(rtc_get_reset_reason(0));
  reset_cpu_1 = reset_reason(rtc_get_reset_reason(1));
}
