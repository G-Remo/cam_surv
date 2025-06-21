#define SD

#include <esp_camera.h>
#define CAMERA_MODEL_AI_THINKER

extern bool delete_task_compare;

extern bool surv, envoi_photo;
extern bool envoi_photo_tablette;
extern String info;

extern const char *ssid, *ssid2, *password, *hote;
extern IPAddress local_ip, gateway, ip_tablette;
extern bool isConnectedToPrimary;

extern uint8_t surveillance;

extern String reset_cpu_0, reset_cpu_1;

extern const char *botToken;
extern int64_t chatID;
void loop_telegram();

#ifdef SD
String horodatage();
#endif

camera_fb_t newCameraFrame();

void sendToQueue(camera_fb_t input = newCameraFrame());

void purge_fb();
