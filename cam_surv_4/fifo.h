#include <queue>

// Déclaration de la file d'attente globale (si nécessaire)
std::queue<camera_fb_t> dataQueue;

// Fonction pour envoyer les données reçues dans la queue
void sendToQueue(camera_fb_t input)
{
  // Ajouter les données à la file d'attente
  dataQueue.push(input);

  Serial.println("Données envoyées à la file d'attente.");
}

void task_fifo(void *pvParameters)
{
  uint32_t debut;
  uint16_t loop_delay;
  for (;;)
  {
    debut = millis();
    loop_delay = 1000;
    
    if (!dataQueue.empty()) 
    {
      // Extraire les données de la file d'attente
      camera_fb_t data = dataQueue.front();
      dataQueue.pop();

      // Traiter les données
      enregistre_derniere_photo(data);
      send_photo(data.buf, data.len);
      if (envoi_photo_tablette) wifi_send(data.buf, data.len);

      // Libérer la mémoire
      purge_fb(data);
    }

    loop_telegram();

    if (millis() - debut >= loop_delay) loop_delay = 200;
    else                                loop_delay -= millis() - debut;

//    Serial.printf("loop_delay: %u\n", loop_delay);
    // Ajouter un délai pour éviter une surcharge CPU
    vTaskDelay(pdMS_TO_TICKS(loop_delay));

//    uint32_t s = uxTaskGetStackHighWaterMark(nullptr);
//    Serial.printf("task_fifo: %u\n", s);
  }
}

void init_fifo()
{
  xTaskCreatePinnedToCore(task_fifo, "task_fifo", 6500, NULL, 1, NULL, 1);
}
