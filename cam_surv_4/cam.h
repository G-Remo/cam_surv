#include <esp_camera.h>
#include "camera_pins.h"

#define uS_TO_S_FACTOR 1000000
#define TIME_TO_SLEEP 5

volatile float fb_fps;
volatile uint32_t fb_periode;
volatile uint16_t window_width, window_height;
//volatile bool 
uint8_t zoom, crop_x, crop_y;
int16_t xOffset, yOffset;
uint16_t full_width, full_height;

camera_fb_t to_stream;
bool to_stream_demande, to_stream_pret;

bool etat_flash_led;

void flash(bool value)
{
  etat_flash_led = value;
  digitalWrite(PIN_FLASH_LED, etat_flash_led);
}

void init_flash()
{
  static bool isInit;

  if (isInit) return;

  pinMode(PIN_FLASH_LED, OUTPUT);
  flash(false);
  isInit = true;
}

void mode_nuit_auto_fps()
{
  sensor_t * s = esp_camera_sensor_get();
  uint8_t val = s->status.framesize;
  s->set_framesize(s, (framesize_t)val);
  s->set_reg(s,0xff,0xff,0x01);//banksel 1
  s->set_reg(s,0x11,0xff,0x00);
  s->set_reg(s,0x0f,0xff,0x4b);
  s->set_reg(s,0x03,0xff,0xcf);
//  s->set_reg(s,0xff,0xff,0x00);
}

void mode_max_fps()
{
  sensor_t * s = esp_camera_sensor_get();
  uint8_t val = s->status.framesize;
  s->set_framesize(s, (framesize_t)val);
  s->set_gainceiling(s, (gainceiling_t)0);
  s->set_reg(s,0xff,0xff,0x01);//banksel 1
  s->set_reg(s,0x11,0xff,0x00);
//  s->set_reg(s,0xff,0xff,0x00);
}

void mode_sombre()
{
  sensor_t * s = esp_camera_sensor_get();
  uint8_t val = s->status.framesize;
  s->set_framesize(s, (framesize_t)val);
  s->set_reg(s,0xff,0xff,0x01);//banksel 1
  s->set_reg(s,0x11,0xff,0x01);
//  s->set_reg(s,0xff,0xff,0x00);
}

void init_cam()
{
  init_flash();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  config.frame_size = FRAMESIZE_HD;
  config.jpeg_quality = 10;
  config.fb_count = 2;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  
  if(!psramFound())
  {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 20;
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    esp_camera_deinit();
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    vTaskDelay(1000);
    esp_deep_sleep_start();
  }

  vTaskDelay(20); // Petite pause pour stabiliser le capteur
  
  sensor_t * s = esp_camera_sensor_get();
    s->set_quality(s, 12);
    s->set_wpc(s, 1);
    s->set_lenc(s, 1);

    s->set_raw_gma(s, 1); // (1 or 0)?
    s->set_gain_ctrl(s, 1); // auto gain off (1 or 0)
    s->set_exposure_ctrl(s, 1); // auto exposure off (1 or 0)
    s->set_whitebal(s, 1); // white balance
    s->set_aec2(s, 1); // automatic exposure sensor? (0 or 1)

//    s->set_bpc(s, 1);
//    s->set_hmirror(s, 0);
//    s->set_vflip(s, 0);
//    s->set_contrast(s, 0);
//    s->set_saturation(s, 1);
//    s->set_agc_gain(s, 0); // set gain manually (0 - 30)
//    s->set_aec_value(s, 800); // set exposure manually (0-1200)
//    s->set_wb_mode(s, 0); // white balance mode (0 to 4)

//  mode_nuit_auto_fps();
//  mode_sombre();
  mode_max_fps();
}

void purge_fb(camera_fb_t &source)
{
  if (source.len > 0) source.len = 0;
  else                return;
  
  if (source.buf != nullptr)
  {
    free(source.buf);
    source.buf = nullptr;
  }
}

camera_fb_t copyCameraFrame(const camera_fb_t &source)
{
  camera_fb_t destination;

  destination.len = source.len;
  destination.width = source.width;
  destination.height = source.height;
  destination.format = source.format;
//  destination.timestamp = source.timestamp;
  destination.buf = (uint8_t *)malloc(source.len);
  if (destination.buf != NULL) memcpy(destination.buf, source.buf, source.len);

  return destination;
}

//camera_fb_t newCameraFrame()
//{
//  static uint32_t debut;
//  static uint32_t duree_vie;
//  static bool isUse;
//
//  while (isUse) vTaskDelay(30);
//  
//  isUse = true;
//
//  camera_fb_t destination;
//  camera_fb_t *source;
//
//  uint8_t nb_erreur = 0;
//
//  for(;;)
//  {
//    source = esp_camera_fb_get();
//    if (source) break;
//    
//    Serial.println("Erreur fb");
//    
//    if (nb_erreur >= 3)
//    {
//      Serial.println("Camera error");
//      esp_camera_deinit();
//      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR); //5 secs
//      vTaskDelay(100);
//      esp_deep_sleep_start();
//    }
//    
//    esp_camera_fb_return(source);
//    nb_erreur++;
//    vTaskDelay(100);
//  }
//
//  destination.len = source->len;
//  destination.width = source->width;
//  destination.height = source->height;
//  full_width = source->width;
//  full_height = source->height;
//  destination.format = source->format;
//  destination.timestamp = source->timestamp;
//  destination.buf = (uint8_t *)malloc(source->len);
//  
//  if (destination.buf != NULL) memcpy(destination.buf, source->buf, source->len);
//  else                         Serial.println("Erreur malloc destination.buf");
//  
//  esp_camera_fb_return(source);
//
//  if (millis() - duree_vie > 4000)
//  {
//    purge_fb(to_stream);
//    to_stream_pret = false;
//  }
//
//  if (to_stream_demande)
//  {
//    if (!surv)
//    {
//      purge_fb(to_stream);
//      to_stream_demande = false;
//      to_stream_pret = false;
//    }
//    else
//    {
//      duree_vie = millis();
//      to_stream = copyCameraFrame(destination);
//      to_stream_demande = false;
//      to_stream_pret = true;
//    }
//  }
//
//  fb_fps = 1000.0 / (millis() - debut);
////  Serial.printf("camera_fb_t newCameraFrame(): %.1f fps, %u ms\n", fb_fps, millis() - debut);
//  debut = millis();
//
//  isUse = false;
//  return destination;
//}

camera_fb_t newCameraFrame()
{
  static uint32_t debut;
  static uint32_t duree_vie;
  static bool isUse;

  while (isUse) vTaskDelay(30);
  isUse = true;

  camera_fb_t destination;
  camera_fb_t *source;

  uint8_t nb_erreur = 0;

  for(;;)
  {
    source = esp_camera_fb_get();
    if (source) break;

    info = "Erreur fb";
    Serial.println(info);
    
    if (nb_erreur >= 3)
    {
      info = "Camera error";
      Serial.println(info);
      esp_camera_deinit();
      vTaskDelay(500);
      init_cam();
    }
    
    nb_erreur++;
    vTaskDelay(100);
  }

  destination.len = source->len;
  destination.width = source->width;
  destination.height = source->height;
  full_width = source->width;
  full_height = source->height;
  destination.format = source->format;
//  destination.timestamp = source->timestamp;
  destination.buf = (uint8_t *)malloc(source->len);
  
  if (destination.buf == NULL) {
    info = "Erreur malloc destination";
    Serial.println(info);
    esp_camera_fb_return(source);
    isUse = false;
    destination.len = 0;
    return destination;
  }
  
  memcpy(destination.buf, source->buf, source->len);

  
  esp_camera_fb_return(source);

  if (millis() - duree_vie > 4000)
  {
    purge_fb(to_stream);
    to_stream_pret = false;
  }

  if (to_stream_demande)
  {
    if (!surv)
    {
      purge_fb(to_stream);
      to_stream_demande = false;
      to_stream_pret = false;
    }
    else
    {
      duree_vie = millis();
      to_stream = copyCameraFrame(destination);
      to_stream_demande = false;
      to_stream_pret = true;
    }
  }

  fb_fps = 1000.0 / (millis() - debut);
//  Serial.printf("camera_fb_t newCameraFrame(): %.1f fps, %u ms\n", fb_fps, millis() - debut);
  debut = millis();

  isUse = false;
  return destination;
}

//camera_fb_t newCameraFrame()
//{
//  static uint32_t debut;
//  static uint32_t duree_vie;
//  static bool isUse;
//
//  while (isUse) vTaskDelay(30);
//  
//  isUse = true;
//
//  camera_fb_t destination;
//  camera_fb_t *source;
//
//  source = esp_camera_fb_get();
//  if (!source)
//  {
//    Serial.println("Erreur fb");
////    esp_camera_fb_return(source);
//    destination.len = 0;
//    destination.buf = nullptr;
//    isUse = false;
//
//    return destination;
//  }
//
//  destination.len = source->len;
//  destination.width = source->width;
//  destination.height = source->height;
//  full_width = source->width;
//  full_height = source->height;
//  destination.format = source->format;
//  destination.timestamp = source->timestamp;
//  destination.buf = (uint8_t *)malloc(source->len);
//  
//  if (destination.buf != NULL) memcpy(destination.buf, source->buf, source->len);
//  else                         Serial.println("Erreur malloc destination.buf");
//  
//  esp_camera_fb_return(source);
//
//  if (millis() - duree_vie > 4000)
//  {
//    purge_fb(to_stream);
//    to_stream_pret = false;
//  }
//
//  if (to_stream_demande)
//  {
//    if (!surv)
//    {
//      purge_fb(to_stream);
//      to_stream_demande = false;
//      to_stream_pret = false;
//    }
//    else
//    {
//      duree_vie = millis();
//      to_stream = copyCameraFrame(destination);
//      to_stream_demande = false;
//      to_stream_pret = true;
//    }
//  }
//
//  fb_fps = 1000.0 / (millis() - debut);
////  Serial.printf("camera_fb_t newCameraFrame(): %.1f fps, %u ms\n", fb_fps, millis() - debut);
//  debut = millis();
//
//  isUse = false;
//  return destination;
//}

void set_crop_x(uint8_t x)
{
  uint8_t tmp_crop_x;
  sensor_t * s = esp_camera_sensor_get();
  uint8_t f_size, facteur, max_crop_x;

  switch ( s->status.framesize )
  {
      case 15:
          f_size = 0;
          facteur = 4;
          break;
      case 9:
          f_size = 1;
          facteur = 2;
          break;
      case 6:
          f_size = 2;
          facteur = 1;
          break;
      default:
          Serial.println("Erreur init_window");
          return;
  }

  tmp_crop_x = crop_x;
  if( x > 12) crop_x = 12;
  else crop_x = x;

  if( crop_x == 0)
  {
    xOffset = 0;
    window_width = full_width;
    Serial.printf("full_width: %u\n", full_width);
    Serial.printf("window_width: %u\n", window_width);
    s->set_res_raw(s, f_size, 0, 0, 0, xOffset, yOffset, full_width, window_height, full_width, window_height, true, true);
    return;
  }

  window_width = full_width - ( crop_x * 32 * facteur);
  Serial.printf("full_width: %u\n", full_width);
  Serial.printf("window_width: %u\n", window_width);

  if( window_height == 0) window_height = full_height;

  if(xOffset == 0)
  {
    xOffset = ( full_width - window_width) / 2;
  }
  else
  {
    xOffset += ( crop_x - tmp_crop_x) * 32 * facteur / 2;
  }
  Serial.printf("xOffset: %u, yOffset: %u\n", xOffset, yOffset);

  s->set_res_raw(s, f_size, 0, 0, 0, xOffset, yOffset, window_width, window_height, window_width, window_height, true, true);
}

void set_crop_y(uint8_t y)
{
  uint8_t tmp_crop_y;
  sensor_t * s = esp_camera_sensor_get();
  uint8_t f_size, facteur;

  switch ( s->status.framesize )
  {
    case 15:
        f_size = 0;
        facteur = 4;
        break;
    case 11:
        f_size = 1;
        facteur = 2;
        break;
    case 8:
        f_size = 2;
        facteur = 1;
        break;
    default:
        Serial.println("Erreur init_window");
        return;
  }

  tmp_crop_y = crop_y;
  if( y > 12) crop_y = 12;
  else crop_y = y;

  if( crop_y == 0)
  {
    yOffset = 0;
    window_height = full_height;
    Serial.printf("full_height: %u\n", full_height);
    Serial.printf("window_height: %u\n", window_height);
    s->set_res_raw(s, f_size, 0, 0, 0, xOffset, yOffset, window_width, full_height, window_width, full_height, true, true);
    return;
  }

  window_height = full_height - ( crop_y * 24 * facteur);
  Serial.printf("full_height: %u\n", full_height);
  Serial.printf("window_height: %u\n", window_height);

  if( window_width == 0) window_width = full_width;

  if(yOffset == 0)
  {
    yOffset = ( full_height - window_height) / 2;
  }
  else
  {
    yOffset += ( crop_y - tmp_crop_y) * 24 * facteur / 2;
  }
  Serial.printf("xOffset: %u, yOffset: %u\n", xOffset, yOffset);

  s->set_res_raw(s, f_size, 0, 0, 0, xOffset, yOffset, window_width, window_height, window_width, window_height, true, true);
}

void window_left()
{
  if( xOffset == 0) return;
  
  sensor_t * s = esp_camera_sensor_get();
  uint8_t f_size, facteur;
  
  switch ( s->status.framesize )
  {
      case 15:
          f_size = 0;
          facteur = 4;
          break;
      case 11:
          f_size = 1;
          facteur = 2;
          break;
      case 8:
          f_size = 2;
          facteur = 1;
          break;
      default:
          return;
  }

  Serial.println("Gauche");
  xOffset -= facteur * 24;
  if( xOffset < 0) xOffset = 0;
  Serial.printf("xOffset: %u\n", xOffset);
  
  s->set_res_raw(s, f_size, 0, 0, 0, xOffset, yOffset, window_width, window_height, window_width, window_height, true, true);
}

void window_right()
{
  if( xOffset == full_width - window_width) return;
  
  sensor_t * s = esp_camera_sensor_get();
  uint8_t f_size, facteur;
  
  switch ( s->status.framesize )
  {
      case 15:
          f_size = 0;
          facteur = 4;
          break;
      case 11:
          f_size = 1;
          facteur = 2;
          break;
      case 8:
          f_size = 2;
          facteur = 1;
          break;
      default:
          return;
  }

  Serial.println("Droite");
  xOffset += facteur * 24;
  if( xOffset > full_width - window_width) xOffset = full_width - window_width;
  Serial.printf("xOffset: %u\n", xOffset);
  
  s->set_res_raw(s, f_size, 0, 0, 0, xOffset, yOffset, window_width, window_height, window_width, window_height, true, true);
}

void window_up()
{
  if( yOffset == 0) return;
  
  sensor_t * s = esp_camera_sensor_get();
  uint8_t f_size, facteur;
  
  switch ( s->status.framesize )
  {
      case 15:
          f_size = 0;
          facteur = 4;
          break;
      case 11:
          f_size = 1;
          facteur = 2;
          break;
      case 8:
          f_size = 2;
          facteur = 1;
          break;
      default:
          return;
  }

  Serial.println("Haut");
  yOffset -= facteur * 32;
  if( yOffset < 0) yOffset = 0;
  Serial.printf("yOffset: %u\n", yOffset);

  s->set_res_raw(s, f_size, 0, 0, 0, xOffset, yOffset, window_width, window_height, window_width, window_height, true, true);
}

void window_down()
{
  if( yOffset == full_height - window_height) return;
  
  sensor_t * s = esp_camera_sensor_get();
  uint8_t f_size, facteur;
  
  switch ( s->status.framesize )
  {
      case 15:
          f_size = 0;
          facteur = 4;
          break;
      case 11:
          f_size = 1;
          facteur = 2;
          break;
      case 8:
          f_size = 2;
          facteur = 1;
          break;
      default:
          return;
  }

  Serial.println("Bas");
  yOffset += facteur * 32;
  if( yOffset > full_height - window_height) yOffset = full_height - window_height;
  Serial.printf("yOffset: %u\n", yOffset);

  s->set_res_raw(s, f_size, 0, 0, 0, xOffset, yOffset, window_width, window_height, window_width, window_height, true, true);
}

void window_center()
{
  sensor_t * s = esp_camera_sensor_get();
  uint8_t f_size, facteur;
  
  switch ( s->status.framesize )
  {
      case 15:
          f_size = 0;
          facteur = 4;
          break;
      case 11:
          f_size = 1;
          facteur = 2;
          break;
      case 8:
          f_size = 2;
          facteur = 1;
          break;
      default:
          return;
  }

  Serial.println("Centrer");
  xOffset = ( full_width - window_width) / 2;
  yOffset = ( full_height - window_height) / 2;
  
  s->set_res_raw(s, f_size, 0, 0, 0, xOffset, yOffset, window_width, window_height, window_width, window_height, true, true);
}

void set_zoom(int8_t zoom_val)
{
  uint8_t tmp_zoom;
  sensor_t * s = esp_camera_sensor_get();
  uint8_t f_size, facteur;
  uint8_t max_zoom;

  switch ( s->status.framesize )
  {
      case 15:
          f_size = 0;
          facteur = 4;
          max_zoom = 11;
          break;
      case 11:
          f_size = 1;
          facteur = 2;
          max_zoom = 10;
          break;
      case 8:
          f_size = 2;
          facteur = 1;
          max_zoom = 9;
          break;
      default:
          Serial.println("Erreur init_wndow");
          return;
  }

  tmp_zoom = zoom;
  if( zoom_val > max_zoom) zoom == max_zoom;
  else if( zoom_val < 0) zoom == 0;
  else zoom = zoom_val;
  Serial.printf("zoom: %u\n", zoom);

  window_width = full_width - zoom * 32 * facteur;
  window_height = full_height - zoom * 24 * facteur;
  Serial.printf("window_width: %u\nwindow_height: %u\n", window_width, window_height);

  if( zoom == 0)
  {
    xOffset = 0;
    yOffset = 0;
    s->set_res_raw(s, f_size, 0, 0, 0, xOffset, yOffset, full_width, full_height, full_width, full_height, true, true);
    return;
  }

  xOffset += ( zoom - tmp_zoom) * 32 * facteur / 2;
  yOffset += ( zoom - tmp_zoom) * 24 * facteur / 2;
  Serial.printf("xOffset: %u, yOffset: %u\n", xOffset, yOffset);

  s->set_res_raw(s, f_size, 0, 0, 0, xOffset, yOffset, window_width, window_height, window_width, window_height, true, true);
}

uint8_t lum_moy()
{
  sensor_t * s = esp_camera_sensor_get();
  s->set_reg(s,0xff,0xff,0x01);//banksel 1
  return s->get_reg(s,0x2f,0xff);
}
