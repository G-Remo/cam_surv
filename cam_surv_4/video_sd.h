TaskHandle_t task_capture_sd_handle = NULL;

bool task_capture_sd_active, kill_task_capture_sd;
uint8_t duree;
uint8_t fps;
uint16_t numFrames;

bool vid_vers_telegram;
bool envoi_video_sd;
uint8_t video_sd_h, video_sd_m;
String avi_file_name;

uint32_t temps_rec()
{
  uint32_t rec = 60 * 60 * video_sd_h + 60 * video_sd_m;
  if (rec == 0) return 20;

  return rec;
}

void sd_header(File &file, uint32_t width, uint32_t height, uint32_t numFrames)
{
  uint8_t buffer[212];
  uint32_t reserved = 0;
  uint32_t bufferSize = 0;
  uint32_t microSecPerFrame = 100000;
  uint32_t totalFrames = numFrames;

  memcpy(buffer, "RIFF", 4);
  memcpy(buffer + 4, &bufferSize, 4); // Taille du fichier à remplir à la fin
  memcpy(buffer + 8, "AVI ", 4);
  memcpy(buffer + 12, "LIST", 4);
  uint32_t listSize = 192;
  memcpy(buffer + 16, &listSize, 4);
  memcpy(buffer + 20, "hdrl", 4);
  memcpy(buffer + 24, "avih", 4);
  uint32_t avihSize = 56;
  memcpy(buffer + 28, &avihSize, 4);
  memcpy(buffer + 32, &microSecPerFrame, 4);
  memcpy(buffer + 36, &reserved, 4);
  memcpy(buffer + 40, &reserved, 4);
  uint32_t flags = 0x10;
  memcpy(buffer + 44, &flags, 4);
  memcpy(buffer + 48, &totalFrames, 4);
  memcpy(buffer + 52, &reserved, 4);
  uint32_t streams = 1;
  memcpy(buffer + 56, &streams, 4);
  memcpy(buffer + 60, &reserved, 4);
  memcpy(buffer + 64, &width, 4);
  memcpy(buffer + 68, &height, 4);
  memcpy(buffer + 72, &reserved, 4);
  memcpy(buffer + 76, &reserved, 4);
  memcpy(buffer + 80, &reserved, 4);
  memcpy(buffer + 84, &reserved, 4);
  memcpy(buffer + 88, "LIST", 4);
  uint32_t strlSize = 116;
  memcpy(buffer + 92, &strlSize, 4);
  memcpy(buffer + 96, "strl", 4);
  memcpy(buffer + 100, "strh", 4);
  uint32_t strhSize = 56;
  memcpy(buffer + 104, &strhSize, 4);
  memcpy(buffer + 108, "vids", 4);
  memcpy(buffer + 112, "MJPG", 4);
  memcpy(buffer + 116, &reserved, 4);
  memcpy(buffer + 120, &reserved, 4);
  memcpy(buffer + 124, &reserved, 4);
  uint32_t scale = 100;
  uint32_t rate = 10;
  memcpy(buffer + 128, &scale, 4);
  memcpy(buffer + 132, &rate, 4);
  memcpy(buffer + 136, &reserved, 4);
  memcpy(buffer + 140, &totalFrames, 4);
  memcpy(buffer + 144, &reserved, 4);
  memcpy(buffer + 148, &reserved, 4);
  memcpy(buffer + 152, &reserved, 4);
  memcpy(buffer + 156, &reserved, 4);
  memcpy(buffer + 160, &reserved, 4);
  memcpy(buffer + 164, "strf", 4);
  uint32_t strfSize = 40;
  memcpy(buffer + 168, &strfSize, 4);
  uint32_t biSize = 40;
  uint32_t biPlanes = 1;
  uint32_t biBitCount = 24;
  memcpy(buffer + 172, &biSize, 4);
  memcpy(buffer + 176, &width, 4);
  memcpy(buffer + 180, &height, 4);
  memcpy(buffer + 184, &biPlanes, 2);
  memcpy(buffer + 186, &biBitCount, 2);
  memcpy(buffer + 188, "MJPG", 4);
  uint32_t biSizeImage = width * height * 3;
  memcpy(buffer + 192, &biSizeImage, 4);
  memcpy(buffer + 196, &reserved, 4);
  memcpy(buffer + 200, &reserved, 4);
  memcpy(buffer + 204, &reserved, 4);
  memcpy(buffer + 208, &reserved, 4);

  file.write(buffer, 212);
}

inline void sd_frame(File &file, camera_fb_t fb)
{
  uint8_t buf[8];
  memcpy(buf, "00dc", 4);
  memcpy(buf + 4, &fb.len, 4);
  file.write(buf, 8);
  file.write(fb.buf, fb.len);
}

void sd_header_maj(File &file, uint32_t file_size, uint32_t numFrames, float fps)
{
  fps *= 100.0;

  file_size -= 8;
  Serial.printf("fileSize: %u\n", file_size);
  file.seek(4);
  file.write((uint8_t*)&file_size, 4);

  uint32_t microSecPerFrame = round( 100000000.0 / fps );
  file.seek(32);
  file.write((uint8_t*)&microSecPerFrame, 4);

  file.seek(48);
  file.write((uint8_t*)&numFrames, 4);

  uint32_t rate = round(fps);
  file.seek(132);
  file.write((uint8_t*)&rate, 4);

  file.seek(140);
  file.write((uint8_t*)&numFrames, 4);
}

void task_capture_sd(void* t)
{
  Serial.println("task_capture_sd");

  uint16_t avi_width, avi_height;
  uint32_t moy_fb_size;
  uint32_t moy_fb_time;
  for (uint8_t i = 0; i < 10; i++)
  {
    uint32_t debut = millis();
    camera_fb_t fb = newCameraFrame();
    moy_fb_time += millis() - debut;
    moy_fb_size += fb.len;
    avi_width = fb.width;
    avi_height = fb.height;
    purge_fb(fb);
    vTaskDelay(2);
  }

  moy_fb_size /= 10;
  moy_fb_time /= 10;
  Serial.printf("moy_fb_size: %u\n", moy_fb_size);
  float fps_max = 1000.0 / moy_fb_time;
  Serial.printf("fps max: %.2f\n", fps_max);

  uint8_t fps = fps_max;
  uint32_t duree = temps_rec();
  uint32_t numFrames = duree * fps;
  uint32_t frame_interval = 1000.0 / fps;
  uint32_t bufPos = 0;
  uint32_t buffer_max = 2 * 1024 * 1024 * 1024;
  float speed_up_factor = 1.0;

  if (vid_vers_telegram)
  {
    buffer_max = 50 * 1024 * 1024;

    numFrames = 600;
    fps = numFrames / duree;
    while (duree * fps * moy_fb_size > buffer_max) fps--;

    frame_interval = round( 1000.0 * duree / numFrames );
    speed_up_factor = duree / 20.0;
  }

  Serial.printf("fps: %u\n", fps);
  Serial.printf("numFrames: %u\n", numFrames);
  Serial.printf("frame_interval: %u\n", frame_interval);

  createDir("/Videos");
  avi_file_name = "/Videos/" + String(horodatage()) + ".avi";
  Serial.println("Creation de: " + avi_file_name);

  File videoFile = SD_MMC.open(avi_file_name, FILE_WRITE);
  if (!videoFile)
  {
    Serial.println("Failed to create video file");
    task_capture_sd_active = false;
    vTaskDelete(NULL);
  }

  if ( zoom > 0 || crop_x > 0 || crop_y > 0 )
  {
    avi_width = window_width;
    avi_height = window_height;
  }

  sd_header(videoFile, avi_width, avi_height, numFrames);

  bufPos = 212;

  uint32_t debut = millis();
  uint32_t avi_end_time = millis() + duree * 1000;
  uint16_t numFrames_control, delay_frame_rec;
  uint32_t debut_frame_rec, temp_av_fin;
  for (int i = 0; i < numFrames; i++)
  {
    debut_frame_rec = millis();
    delay_frame_rec = frame_interval;
    temp_av_fin = avi_end_time - millis();
    
    if (temp_av_fin >= 7200000) info = "Enregistrement: " + String(temp_av_fin / 3600000.0, 2) + " H";
    else if (temp_av_fin < 7200000 && temp_av_fin >= 120000) "Enregistrement: " + String(temp_av_fin / 60000.0, 2) + " min";
    else info = "Enregistrement: " + String(temp_av_fin / 1000.0, 2) + " sec";

    if (kill_task_capture_sd)
    {
      videoFile.close();
      info = "Abandonné";
      task_capture_sd_active = false;
      vTaskDelete(NULL);
    }

    camera_fb_t fb = newCameraFrame();
    
    if (bufPos + fb.len + 8 > buffer_max || millis() - debut > duree * 1000)
    {
      purge_fb(fb);
      numFrames = i - 1;
      break;
    }

    sd_frame(videoFile, fb);
    
    bufPos += fb.len + 8;

    purge_fb(fb);

    uint32_t fin_frame_rec = millis() - debut_frame_rec;
    if (fin_frame_rec >= delay_frame_rec) delay_frame_rec = 1;
    else                                  delay_frame_rec -= fin_frame_rec;

    numFrames_control++;
    vTaskDelay(delay_frame_rec);
  }

  uint32_t fin = millis();
  float fps_reel = 1000.0 / ( ( fin  - debut ) / numFrames_control );
  Serial.printf("fps_reel: %.2f\nduree_reel: %u\nnumFrames: %u\nnumFrames_control: %u\n", fps_reel, ( fin  - debut ) / 1000, numFrames, numFrames_control);
  fps_reel *= speed_up_factor;
  sd_header_maj(videoFile, bufPos, numFrames_control, fps_reel);

  videoFile.close();
  Serial.println("End the Avi");
  info = "End the Avi";

  if (vid_vers_telegram) envoi_video_sd = true;

//  uint16_t stackHighWaterMark = uxTaskGetStackHighWaterMark(nullptr);
//  Serial.printf("task_rec: %u octets\n", stackHighWaterMark);

  task_capture_sd_active = false;
  vTaskDelete(NULL);
}

void start_stop_task_capture_sd()
{
  static uint32_t Time;
  if (Time - millis() < 4000) return;
  Time = millis();

  if (envoi_video_sd) return;

  if (task_capture_sd_active)
  {
    Serial.println("Stop capture_sd");
    kill_task_capture_sd = true;
    return;
  }
  kill_task_capture_sd = false;
  task_capture_sd_active = true;
  vTaskDelay(200);
  xTaskCreatePinnedToCore(task_capture_sd, "task_capture_sd", 5000, NULL, 1, &task_capture_sd_handle, 0);
}
