#include <WiFi.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#ifdef SD
#include "z_cam_sd.h"
#else
#include "z_cam.h"
#endif

WiFiServer server(80);

WiFiClient client_stream_1;
WiFiClient client_stream_2;

bool couleur_prev;
uint8_t surveillance;
String info = "Démarré";

camera_fb_t data_web;

String runtime()
{
  uint32_t totalSeconds = millis() / 1000;
  uint8_t days = totalSeconds / 86400;
  uint8_t hours = (totalSeconds % 86400) / 3600;
  uint8_t minutes = (totalSeconds % 3600) / 60;
  uint8_t seconds = totalSeconds % 60;

  // Format avec zéros en tête
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "Runtime: %lu jour(s), %02uh:%02um:%02us",
           days, hours, minutes, seconds);

  return String(buffer);
}

void enregistre_derniere_photo(camera_fb_t data)
{
  purge_fb(data_web);
  data_web.len = data.len;
  
  // Allouer la mémoire pour le buffer
  data_web.buf = (uint8_t *)malloc(data_web.len);
  if (data_web.buf == nullptr)
  {
      Serial.println("Erreur d'allocation de la mémoire pour data_web");
      return;  // Si l'allocation échoue, quitter la fonction
  }
  
  // Copier les données du buffer
  memcpy(data_web.buf, data.buf, data_web.len);
}

void last_photo(WiFiClient client)
{
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: image/jpeg\r\n");
  client.print("Content-Disposition: inline; filename=photo.jpg\r\n");
  client.print("Connection: close\r\n\r\n");
  client.write(data_web.buf, data_web.len);
  client.stop();
}

void not_found(WiFiClient client)
{
  client.print("HTTP/1.1 404 Not Found\r\n");
  client.print("Content-Type: text/html\r\n");
  client.print("\r\n");
  client.print("<h1>Not Found</h1>\r\n");
  client.stop();
}

void serveur_page(WiFiClient client, String myString)
{
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Access-Control-Allow-Origin: *\r\n");
  client.print("Content-Type: text/html\r\n");
  client.print("Connection: close\r\n");
  client.print("\r\n");
  client.print( myString );
  client.stop();
}

void serveur_page(WiFiClient client, const uint8_t* data, size_t len)
{
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Access-Control-Allow-Origin: *\r\n");
  client.print("Content-Type: text/html\r\n");
  client.print("Content-Encoding: gzip\r\n"); // très important !
  client.print("Connection: close\r\n");
  client.print("Cache-Control: max-age=86400\r\n"); // optionnel : mise en cache 1 jour
  client.print("Content-Length: ");
  client.print(len);
  client.print("\r\n\r\n");

  client.write(data, len); // envoyer les données binaires gzip
  client.stop();
}

void serveur_json(WiFiClient client, String myString)
{
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Access-Control-Allow-Origin: *\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Content-Length: ");
  client.print(myString.length());
  client.print("\r\n");
  client.print("Connection: keep-alive\r\n");
  client.print("\r\n");
  client.print(myString);
  client.stop();
}

void serveur_restart(WiFiClient client)
{
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: text/html\r\n");
  client.print("\r\n");
  client.print("<h1>Red&eacute;marrage</h1>\r\n");
  client.flush();
  client.stop();
  vTaskDelay(1000);
  ESP.restart();
}

void stream_header(WiFiClient client)
{
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: multipart/x-mixed-replace; boundary=frame\r\n");
  client.print("\r\n");
}

void stream_loop(WiFiClient client, camera_fb_t fb)
{
  client.print("--frame\r\n");
  client.print("Content-Type: image/jpeg\r\n");
  client.print("Content-Length: ");
  client.print(fb.len);
  client.print("\r\n");
  client.print("Connection: keep-alive\r\n");
  client.print("\r\n");
  client.write(fb.buf, fb.len);
  client.print("\r\n");
  client.flush();
}

void photo(WiFiClient client)
{
  camera_fb_t fb = newCameraFrame();

  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: image/jpeg\r\n");
  client.print("Content-Disposition: inline; filename=photo.jpg\r\n");
  client.print("Connection: close\r\n\r\n");
  client.write(fb.buf, fb.len);
  client.flush();
  client.stop();

  purge_fb(fb);
}

char *urlDecode(const char *str)
{
  int d = 0; /* whether or not the string is decoded */

  char *dStr = new char[strlen(str) + 1];
  char eStr[] = "00"; /* for a hex code */

  strcpy(dStr, str);

  while(!d) {
    d = 1;
    int i; /* the counter for the string */

    for(i=0;i<strlen(dStr);++i) {

      if(dStr[i] == '%') {
        if(dStr[i+1] == 0)
          return dStr;

        if(isxdigit(dStr[i+1]) && isxdigit(dStr[i+2])) {

          d = 0;

          /* combine the next to numbers into one */
          eStr[0] = dStr[i+1];
          eStr[1] = dStr[i+2];

          /* convert it to decimal */
          long int x = strtol(eStr, NULL, 16);

          /* remove the hex */
          memmove(&dStr[i+1], &dStr[i+3], strlen(&dStr[i+3])+1);

          dStr[i] = x;
        }
      }
    }
  }

  return dStr;
}

char *toChar(String Data, String key)
{
  int keyStart = Data.indexOf(key + "=");
  if (keyStart == -1) {
    return ""; // Clé non trouvée
  }
  int valueStart = keyStart + key.length() + 1; // Début de la valeur
  int valueEnd = Data.indexOf("&", valueStart);
  if (valueEnd == -1) {
    valueEnd = Data.length(); // Si pas de '&', prendre jusqu'à la fin
  }
  Data = Data.substring(valueStart, valueEnd);
//  Serial.printf( "Data: %s\n", Data);

  char *dStr = new char[ Data.length() ];

  strncpy( dStr, Data.c_str(), Data.length() );
  dStr[ Data.length() ] = '\0';

  return dStr;
}

String status_vers()
{
  JsonDocument doc;

  sensor_t * s = esp_camera_sensor_get();
  
  doc["xclk"] = s->xclk_freq_hz / 1000000;
  doc["gainceiling"] = s->status.gainceiling;
  doc["zoom"] = zoom;
  doc["crop_x"] = crop_x;
  doc["crop_y"] = crop_y;
  doc["luminosite"] = lum_moy();
  doc["fb_fps"] = String(fb_fps, 1); // + " fps (" + String(1000 / fb_fps, 0) + " ms)";
  doc["ram"] = ESP.getFreeHeap();
  doc["psram"] = ESP.getFreePsram();
  doc["pix_detected"] = pix_detected;

  doc["temp_restant"] = info;

  doc["surv"] = surv;
  doc["couleur_seuil"] = couleur_seuil;
  doc["couleur_box_x"] = couleur_box_x;
  doc["couleur_box_y"] = couleur_box_y;
  doc["taille_couleur_box"] = taille_couleur_box;

#ifdef SD
  doc["sd_text"] = sd_text();
  doc["video_sd"] = task_capture_sd_active;
  doc["vid_vers_telegram"] = vid_vers_telegram;
#endif
  
//  doc[""] = ;

  String json;
  serializeJson(doc, json);

  return json;
}

String status_var()
{
  JsonDocument doc;

  sensor_t * s = esp_camera_sensor_get();

  doc["compil"] = __DATE__ " " __TIME__;
  doc["hote"] = hote;
  doc["local_ip"] = WiFi.localIP();
  doc["MAC_STA"] = WiFi.macAddress();
  doc["Channel"] = WiFi.channel();
  doc["RSSI"] = WiFi.RSSI();
  
  doc["ssid"] = ssid;
  doc["ssid2"] = ssid2;
  doc["password"] = password;
  doc["chatID"] = chatID;
  doc["botToken"] = botToken;
    
  doc["envoi_photo"] = envoi_photo;
  doc["envoi_photo_tablette"] = envoi_photo_tablette;
  doc["surveillance"] = surveillance;
  doc["bbox_couleur"] = bbox_couleur;
  doc["zones_x"] = zones.x;
  doc["zones_y"] = zones.y;
  doc["zones_w"] = zones.w;
  doc["zones_h"] = zones.h;
  doc["couleur_min"] = couleur_min;
  doc["couleur_max"] = couleur_max;
  doc["seuil_1"] = seuil_1;
  doc["min_pixel_detection"] = min_pixel_detection;
  doc["temps_photo"] = temps_photo;

  doc["framesize"] = s->status.framesize;
  doc["gainceiling"] = s->status.gainceiling;
  doc["hmirror"] = s->status.hmirror;
  doc["vflip"] = s->status.vflip;
  doc["quality"] = s->status.quality;

  doc["led_intensity"] = etat_flash_led;

  doc["ip_tablette"] = ip_tablette;

  doc["reset_cpu_0"] = reset_cpu_0;
  doc["reset_cpu_1"] = reset_cpu_1;

#ifdef SD
  doc["video_sd_h"] = video_sd_h;
  doc["video_sd_m"] = video_sd_m;
#endif

  doc["Runtime"] = runtime();

//  doc[""] = ;

  String json;
  serializeJson(doc, json);

//  size_t jsonSize = measureJson(doc);
//  Serial.printf("La taille du document JSON est: %u octets\n", jsonSize);

  return json;
}

void handleOptionsRequest(WiFiClient client)
{
  client.println("HTTP/1.1 200 OK");
  client.println("Access-Control-Allow-Origin: *"); // Autorise toutes les origines, vous pouvez spécifier une origine spécifique
  client.println("Access-Control-Allow-Methods: POST, GET, OPTIONS"); // Autorise les méthodes POST, GET et OPTIONS
  client.println("Access-Control-Allow-Headers: Content-Type"); // Autorise les en-têtes spécifiés
  client.println("Connection: close");
  client.println();
}

void handleControlRequest(const char *variable, const char *vall, WiFiClient client)
{
  int val = atoi(vall);
  sensor_t *s = esp_camera_sensor_get();
  int res = 0;

  if (!strcmp(variable, "framesize"))
  {
    s->set_framesize(s, (framesize_t)val);
    zoom = 0;
    xOffset = 0;
    yOffset = 0;
    crop_x = 0;
    crop_y = 0;
  }

  else if (!strcmp(variable, "quality"))
  {
    s->set_reg(s,0xff,0xff,0x00);
    s->set_quality(s, val);
  }
  else if (!strcmp(variable, "envoi_photo_tablette")) envoi_photo_tablette = val;
  else if (!strcmp(variable, "gainceiling")) s->set_gainceiling(s, (gainceiling_t)val);
  else if (!strcmp(variable, "hmirror")) s->set_hmirror(s, val);
  else if (!strcmp(variable, "xclk"))
  {
    if (val <= 20) s->set_xclk(s, LEDC_TIMER_0, val);
  }
  
  else if (!strcmp(variable, "mode_nuit")) mode_nuit_auto_fps();
  else if (!strcmp(variable, "max_fps")) mode_max_fps();
  else if (!strcmp(variable, "sombre")) mode_sombre();

  else if (!strcmp(variable, "envoi_photo")) envoi_photo = val;
  else if (!strcmp(variable, "surveillance")) surveillance = val;
  else if (!strcmp(variable, "zoom")) set_zoom(val);
  else if (!strcmp(variable, "bouton_h")) window_up();
  else if (!strcmp(variable, "bouton_g")) window_left();
  else if (!strcmp(variable, "bouton_z")) window_center();
  else if (!strcmp(variable, "bouton_d")) window_right();
  else if (!strcmp(variable, "bouton_b")) window_down();
  else if (!strcmp(variable, "crop_x")) set_crop_x(val);
  else if (!strcmp(variable, "crop_y")) set_crop_y(val);
  
  else if (!strcmp(variable, "couleur_min")) couleur_min = val;
  else if (!strcmp(variable, "couleur_max")) couleur_max = val;
  else if (!strcmp(variable, "couleur_prev")) couleur_prev = val;
  else if (!strcmp(variable, "seuil_1")) seuil_1 = val;
  else if (!strcmp(variable, "min_pixel_detection")) min_pixel_detection = val;
  else if (!strcmp(variable, "couleur_seuil")) couleur_seuil = val;
  else if (!strcmp(variable, "bbox_couleur")) bbox_couleur = val;
  else if (!strcmp(variable, "zones_x")) zones.x = val;
  else if (!strcmp(variable, "zones_y")) zones.y = val;
  else if (!strcmp(variable, "zones_w")) zones.w = val;
  else if (!strcmp(variable, "zones_h")) zones.h = val;
  else if (!strcmp(variable, "taille_couleur_box")) taille_couleur_box = val;
  else if (!strcmp(variable, "couleur_box_x")) couleur_box_x = val;
  else if (!strcmp(variable, "couleur_box_y")) couleur_box_y = val;
  else if (!strcmp(variable, "temps_photo")) temps_photo = val;
  else if (!strcmp(variable, "surv")) surv = val;

  else if (!strcmp(variable, "photo_telegram")) photo_telegram = val;
  else if (!strcmp(variable, "capture_psram_25")) capture_psram_25();
  else if (!strcmp(variable, "capture_psram_max")) capture_psram_max();

  else if (!strcmp(variable, "led_intensity")) flash(val);

#ifdef SD
  else if (!strcmp(variable, "video_sd_h")) video_sd_h = val;
  else if (!strcmp(variable, "video_sd_m")) video_sd_m = val;
  else if (!strcmp(variable, "photo_sd")) enregistrer_photo();
  else if (!strcmp(variable, "vid_vers_telegram")) vid_vers_telegram = val;
  else if (!strcmp(variable, "video_sd")) start_stop_task_capture_sd();
#endif


//  Serial.printf( "%s: %i\n", variable, val );
  client.println("HTTP/1.1 200 OK");
}

void task_stream_1(void *t)
{
  stream_header(client_stream_1);
  for(;;)
  {
    if (!client_stream_1.connected())
    {
      client_stream_1.stop();
      vTaskDelete(NULL);
    }

    if (!surv)
    {
      camera_fb_t fb = newCameraFrame();
      stream_loop(client_stream_1, fb);
      purge_fb(fb);
      vTaskDelay(10);
    }
    else
    {
      to_stream_demande = true;
      
      while (!to_stream_pret && surv) vTaskDelay(5);
      
      if (surv)
      {
        to_stream_pret = false;
        stream_loop(client_stream_1, to_stream);
        purge_fb(to_stream);
      }
      else
      {
        Serial.printf("surv: %u\n", surv);
        to_stream_demande = false;
        to_stream_pret = false;
        purge_fb(to_stream);
      }

      vTaskDelay(50);
    }
  }
}

void task_stream_2(void *t)
{
  stream_header(client_stream_2);
  for(;;)
  {
    if( !client_stream_2.connected())
    {
      client_stream_2.stop();
      vTaskDelay(10);
      vTaskDelete(NULL);
    }

    if(surv && surveillance == 0)
    {
      control_stream = true;
      while (!control && surv) vTaskDelay(50);

      if (surv)
      {
        control = false;
        stream_loop(client_stream_2, fb_control);
        purge_fb(fb_control);
      }
      else
      {
        control_stream = false;
        control = false;
        purge_fb(fb_control);
      }
    }
    else
    {
      camera_fb_t fb;
      switch(surveillance)
      {
        case 0:
          fb = newCameraFrame();
          break;

        case 1:
          fb = conv_to_jpg(couleur_zone_select(conv_jpeg_to(4, false)));
          break;

        case 2:
          fb = conv_to_jpg(couleur(couleur_zone(conv_jpeg_to(4, false))));
          break;
          
        case 3:
          fb = conv_to_jpg(detecteur_zone_select(conv_jpeg_to(4, false)));
          break;
      }

      stream_loop(client_stream_2, fb);
      purge_fb(fb);
    }

    vTaskDelay(20);
  }
}

void task_web(void *t)
{
  WiFiClient client;

  uint16_t task_delay;
  uint32_t debut, fin;
  for (;;)
  {
    task_delay = 200;
    debut = millis();
    
    client = server.available();
           
    if (client.connected())
    {
      String req = "";
      while(client.available())
      {
        req += (char)client.read();
      }
  
      if( req.indexOf("GET") != -1)
      {
        int addr_start = req.indexOf("GET") + strlen("GET");
        int addr_end = req.indexOf("HTTP", addr_start);
        req = req.substring(addr_start, addr_end);
        req.trim();
  
        if (req == "/")
        {
          serveur_page(client, index_html_gz, index_html_gz_len);
        }
        
        else if (req == "/video_stream")
        {
          client_stream_1 = client;
          xTaskCreatePinnedToCore(task_stream_1, "task_stream_1", 4000, NULL, 1, NULL, 1);
        }
        
        else if (req == "/video_stream_2")
        {
          client_stream_2 = client;
          xTaskCreatePinnedToCore(task_stream_2, "task_stream_2", 5000, NULL, 1, NULL, 1);
        }

        else if (req.indexOf("/capture") != -1)
        {
          photo(client);
        }
        else if (req.indexOf("/last_ph") != -1)
        {
          last_photo(client);
        }
        else if (req == "/status")
        {
          serveur_json(client, status_var());
        }
        else if (req == "/status_vers")
        {
          serveur_json(client, status_vers());
        }
        else if (req == "/restart")
        {
          serveur_restart(client);
        }
        else if (req == "/wifi_reset")
        {
          init_littlefs();
          deleteFile("/ssid.txt");
          serveur_restart(client);
        }
        else
        {
          not_found(client);
        }
      }
      else if (req.indexOf("POST /control") != -1)
      {
        int add_start = req.indexOf("{\"id\":");
        int add_end = req.indexOf("\"}", add_start);
        req = req.substring(add_start, add_end + 3);

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, req);
        
        if (error)
        {
          Serial.print("deserializeJson() failed: ");
          Serial.println(error.f_str());
          continue;
        }
      
        const char *variable = doc["id"];
        const char *val = doc["value"];
        Serial.printf( "%s: %s\n", variable, val);
        handleControlRequest( variable, val, client);
      }
      else if (req.indexOf("POST /setwifi") != -1)
      {
        int contentLengthStart = req.indexOf("Content-Length:") + strlen("Content-Length:");
        int contentLengthEnd = req.indexOf("\r\n", contentLengthStart);
        int contentLength = req.substring(contentLengthStart, contentLengthEnd).toInt();
        
        // Rechercher le double saut de ligne indiquant la fin de l'en-tête de la requête
        int doubleNewlineIndex = req.indexOf("\r\n\r\n");
        if (doubleNewlineIndex == -1) return;
      
        // Analyser la charge utile pour récupérer les données du formulaire
        req = req.substring(doubleNewlineIndex + strlen("\r\n\r\n"));
        req = urlDecode(req.c_str());

        writeFile("/ssid.txt", toChar( req, "ssid"));
        writeFile("/ssid2.txt", toChar( req, "ssid2"));
        writeFile("/password.txt", toChar( req, "password"));
        writeFile("/hote.txt", toChar( req, "hote"));
        writeFile("/local_ip.txt", toChar( req, "local_ip"));
        writeFile("/chatID.txt", toChar( req, "chatID"));
        writeFile("/botToken.txt", toChar( req, "botToken"));
        writeFile("/ip_tablette.txt", toChar( req, "ip_tablette"));
        
        ip_tablette.fromString(toChar( req, "ip_tablette"));

        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/plain");
        client.println("Connection: close");
        client.println();
        client.println("OK");
      }
      else if (req.indexOf("OPTIONS") != -1)
      {
        handleOptionsRequest(client);
      }
    }

    fin = millis() - debut;

    if (fin >= task_delay) task_delay = 10;
    else                   task_delay -= fin;
    
//    Serial.printf("task_delay: %u ms, task av: %u ms\n", task_delay, millis() - debut);

    vTaskDelay(pdMS_TO_TICKS(task_delay));
//    uint16_t t = uxTaskGetStackHighWaterMark(nullptr);
//    Serial.printf("task_web: %u octets\n", t);
  }
}

void init_web()
{
  server.begin();
  xTaskCreatePinnedToCore(task_web, "task_web", 4500, NULL, 1, NULL, 1);
}
