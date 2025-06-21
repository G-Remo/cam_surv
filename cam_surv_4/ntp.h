#include <time.h>

TaskHandle_t task_ntp_handle = NULL;

const char* ntpServer1 = "ntp.viarouge.net";
const char* ntpServer2 = "192.168.1.47";
const char* ntpServer3 = "192.168.4.1";
const char* time_zone = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";

String horodatage()
{
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char tim[13];
  snprintf( tim, 13, "%4d%02d%02d%02d%02d", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min);
  return tim;
}

void print_time()
{
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  
  if ( timeinfo ) Serial.printf("%02d/%02d/%4d, %02d:%02d\n", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900, timeinfo->tm_hour, timeinfo->tm_min);
  else            Serial.println("No time available");
}

String print_t()
{
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  char tim[20];
  if (timeinfo) strftime(tim, sizeof(tim), "%d/%m-%Y", timeinfo);
  else          snprintf(tim, sizeof(tim), "No time available");

  return tim;
}

bool get_time()
{
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Erreur de syncro");
    return false;
  }
  
  return true;
}

void task_ntp(void *t)
{
//  while (!wifi_ok()) vTaskDelay(500);
  uint32_t task_delay;
  for(;;)
  {
    task_delay = 28800000UL;
    if(!get_time()) task_delay = 10000;
    else            print_time();

    vTaskDelay(task_delay);

//    unsigned int temp1 = uxTaskGetStackHighWaterMark(nullptr);
//    Serial.print("task_ntp: "); Serial.println(temp1);
  }
}

void init_ntp()
{
  configTzTime(time_zone, ntpServer1, ntpServer2, ntpServer3);
  
  xTaskCreatePinnedToCore(task_ntp, "task_ntp", 3400, NULL, 1, &task_ntp_handle, 1);
}
