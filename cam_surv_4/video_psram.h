
TaskHandle_t task_capture_psram_handle = NULL;

bool capture_max, envoi_video_psram;
uint8_t *videoBuffer;
uint32_t bufPos;

void psram_header(uint8_t * buffer, size_t bufferSize, uint32_t width, uint32_t height, int numFrames)
{
  uint32_t reserved = 0;
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
  uint32_t rate = 5;
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
}

void psram_frame(uint8_t *buffer, camera_fb_t fb)
{
  // Write the MJPEG frame header
  memcpy(buffer + bufPos, "00dc", 4); // Frame identifier for MJPEG data
  bufPos += 4;

  // Write the frame size
  memcpy(buffer + bufPos, &fb.len, 4);
  bufPos += 4;

  // Write the frame data
  memcpy(buffer + bufPos, fb.buf, fb.len);
  bufPos += fb.len;
}

void psram_header_maj(uint8_t * buffer, size_t bufferSize, uint32_t numFrames, float fps)
{
  fps *= 100.0;
  uint32_t fileSize = bufferSize - 4;
  memcpy(buffer + 4, &fileSize, 4);
  uint32_t microSecPerFrame = round( 100000000.0 / fps );
  memcpy(buffer + 32, &microSecPerFrame, 4);
  memcpy(buffer + 48, &numFrames, 4);
  uint32_t rate = round(fps);
  memcpy(buffer + 132, &rate, 4);
  memcpy(buffer + 140, &numFrames, 4);
}

void task_capture_psram(void *t)
{
  Serial.println("task_capture_psram");

  uint8_t fps = 5;
  uint16_t numFrames = 25;
  if (capture_max)
  {
    fps = 10;
    numFrames = 1000;
  }

  uint16_t avi_width, avi_height;
  
  if ( zoom > 0 || crop_x > 0 || crop_y > 0 )
  {
    avi_width = window_width;
    avi_height = window_height;
  }
  else
  {
    avi_width = full_width;
    avi_height = full_height;
  }

  uint32_t bufferSize = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) - 80000;
  videoBuffer = (uint8_t *) ps_malloc(bufferSize);
  if (!videoBuffer)
  {
    Serial.println("Failed to allocate PSRAM buffer");
    vTaskDelete(NULL);
  }

  psram_header(videoBuffer, bufferSize, avi_width, avi_height, numFrames);

  bufPos = 212;

  uint16_t d = round(1000.0 / fps);
  uint32_t debut = millis();
  for (uint16_t i = 1; i < numFrames; i++)
  {
    uint32_t debut_rec = millis();
    uint16_t delay_t = d;
    
    camera_fb_t fb = newCameraFrame();

    if (bufPos + fb.len + 8 > bufferSize)
    {
      Serial.println( "Enregistrement terminé" );
      purge_fb(fb);
      numFrames = i - 1;
      break;
    }

    psram_frame(videoBuffer, fb);

    purge_fb(fb);
    
    uint32_t fin_rec = millis() - debut_rec;
    
    if (fin_rec >= delay_t) delay_t = 1;
    else                    delay_t -= fin_rec;

    vTaskDelay(delay_t);
  }
  
  uint32_t fin = millis();
  float fps_reel = 1000.0 / ( ( fin  - debut ) / numFrames );
  float duree_reel = ( fin  - debut ) / 1000.0;

  Serial.printf("Nombre d'images: %u\n", numFrames);
  Serial.printf("Taille: %u ko\n", bufPos / 1024);
  Serial.printf("fps: %f\n", fps_reel);
  Serial.printf("duree: %.1f secs\n", duree_reel);

  psram_header_maj(videoBuffer, bufPos, numFrames, fps_reel);

  envoi_video_psram = true;

  vTaskDelete(NULL);
}

void capture_psram_25()
{
  if (task_capture_psram_handle != NULL)
  {
    eTaskState state = eTaskGetState(task_capture_psram_handle);
    if (state == eRunning) return;
  }
  
  capture_max = false;
  xTaskCreatePinnedToCore( task_capture_psram, "task_capture_psram", 6000, NULL, 1, &task_capture_psram_handle, 0 );
}

void capture_psram_max()
{
  if (envoi_video_psram) return;
  
  if (task_capture_psram_handle != NULL)
  {
    eTaskState state = eTaskGetState(task_capture_psram_handle);
    if (state == eRunning) return;
  }
  
  capture_max = true;
  xTaskCreatePinnedToCore( task_capture_psram, "task_capture_psram", 6000, NULL, 5, &task_capture_psram_handle, 0 );
}
