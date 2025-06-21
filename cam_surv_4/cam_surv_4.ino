#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "config.h"
#include "reset.h"
#include "llittlefs.h"
#include "wifi_send.h"
#include "wwifi.h"
#include "cam.h"
#include "outils_video.h"

#ifdef SD
#include "ntp.h"
#include "ssd.h"
#include "video_sd.h"
#endif

#include "video_psram.h"
#include "telegram_simple.h"
#include "web.h"
#include "fifo.h"

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200);
  Serial.println("Demarrage");

  init_reset();

  init_littlefs_mdp();
  init_cam();
  init_wifi();

#ifdef SD
  init_sd();
  init_ntp();
#endif

  init_fifo();
  init_web();

  start_task_compare();
}

void loop()
{
  vTaskDelete(NULL);
}
