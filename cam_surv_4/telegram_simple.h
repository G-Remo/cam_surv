#include <NetworkClientSecure.h>

const char *botToken;
int64_t chatID;
bool photo_telegram;

bool sp(camera_fb_t fb)
{
  NetworkClientSecure client_secure;
  client_secure.setInsecure();
  
  if (!client_secure.connect("api.telegram.org", 443))
  {
    Serial.println("Erreur de connexion à Telegram");
    client_secure.stop();
    return false;
  }

  String tail = "\r\n--boundary--\r\n";
  String head = "--boundary\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + String(chatID) + "\r\n--boundary\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";

  client_secure.print(String("POST /bot") + botToken + "/sendPhoto HTTP/1.1\r\n");
  client_secure.print("Host: api.telegram.org\r\n");
  client_secure.print("Content-Type: multipart/form-data; boundary=boundary\r\n");
  client_secure.print("Content-Length: " + String(head.length() + fb.len + tail.length()) + "\r\n");
  client_secure.print("\r\n");
  client_secure.print(head);

  size_t bufferSize = 1024;
  uint8_t *buffer = fb.buf;
  size_t len = fb.len;
  uint16_t count = 0;
  while (len > 0)
  {
    uint32_t debut_while = millis();
    size_t chunkSize = len > bufferSize ? bufferSize : len;
    client_secure.write(buffer, chunkSize);
    buffer += chunkSize;
    len -= chunkSize;
    if (++count % 10 == 0) vTaskDelay(10);

    if (millis() - debut_while > 1000)
    {  
      info = "Time Out";
      Serial.println(info);
      client_secure.stop();
      return false;
    }
  }

  client_secure.print(tail);
  client_secure.stop();

  return true;
}

void send_photo(camera_fb_t fb)
{
//  uint32_t debut = millis();
  uint8_t tentatives = 0;
  bool reussite = true;
  wifi_send(fb.buf, fb.len);
  while (!sp(fb))
  {
    tentatives++;
    if (tentatives > 5)
    {
      reussite = false;
      Serial.println("Echec");
      break;
    }
    vTaskDelay(2);
    reussite = true;
  }
  
  
  purge_fb(fb);

  if (reussite)
  {
    info = "Photo Envoyé";
    Serial.println(info);
  }
}

bool sp(uint8_t *buf, size_t len)
{
  NetworkClientSecure client_secure;
  client_secure.setInsecure();
  
  if (!client_secure.connect("api.telegram.org", 443))
  {
    Serial.println("Erreur de connexion à Telegram");
    client_secure.stop();
    return false;
  }

  String tail = "\r\n--boundary--\r\n";
  String head = "--boundary\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + String(chatID) + "\r\n--boundary\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";

  client_secure.print(String("POST /bot") + botToken + "/sendPhoto HTTP/1.1\r\n");
  client_secure.print("Host: api.telegram.org\r\n");
  client_secure.print("Content-Type: multipart/form-data; boundary=boundary\r\n");
  client_secure.print("Content-Length: " + String(head.length() + len + tail.length()) + "\r\n");
  client_secure.print("\r\n");
  client_secure.print(head);

  size_t bufferSize = 4096;
  uint8_t *buffer = buf;
  uint16_t count = 0;
  while (len > 0)
  {
    uint32_t debut_while = millis();
    size_t chunkSize = len > bufferSize ? bufferSize : len;
    client_secure.write(buffer, chunkSize);
    buffer += chunkSize;
    len -= chunkSize;
    if (++count % 10 == 0) vTaskDelay(2);

    if (millis() - debut_while > 1000)
    {  
      info = "Time Out";
      Serial.println(info);
      client_secure.stop();
      return false;
    }
  }

  client_secure.print(tail);
  client_secure.stop();

  return true;
}

void send_photo(uint8_t *buf, size_t len)
{
  uint8_t tentatives = 0;
  bool reussite = true;
  while (!sp(buf, len))
  {
    tentatives++;
    if (tentatives > 5)
    {
      reussite = false;
      Serial.println("Echec");
      break;
    }
    vTaskDelay(2);
    reussite = true;
  }

  if (reussite)
  {
    info = "Photo Envoyé";
    Serial.println(info);
  }
}

bool sv(uint8_t *buf, size_t buf_size)
{
  NetworkClientSecure client_secure;
  client_secure.setInsecure();
  
  if (!client_secure.connect("api.telegram.org", 443)) return false;
  
  String tail = "\r\n--boundary--\r\n";
  String head = "--boundary\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + String(chatID) + "\r\n--boundary\r\nContent-Disposition: form-data; name=\"video\"; filename=\"video.avi\"\r\nContent-Type: video/avi\r\n\r\n";

  client_secure.print(String("POST /bot") + botToken + "/sendVideo HTTP/1.1\r\n");
  client_secure.print("Host: api.telegram.org\r\n");
  client_secure.print("Content-Type: multipart/form-data; boundary=boundary\r\n");
  client_secure.print("Content-Length: " + String(head.length() + buf_size + tail.length()) + "\r\n");
  client_secure.print("\r\n");
  client_secure.print(head);

  size_t bufferSize = 2048;
  size_t fileSize = buf_size;
  uint8_t *buffer = buf;
  uint16_t count = 0;
  while (buf_size > 0)
  {
    uint32_t debut_while = millis();
    size_t chunkSize = buf_size > bufferSize ? bufferSize : buf_size;
    client_secure.write(buffer, chunkSize);
    buffer += chunkSize;
    buf_size -= chunkSize;
    info = "Envoie: " + String(100 - (100 * buf_size / fileSize)) + "%";
    if (++count % 10 == 0 ) vTaskDelay(10);
        
    if (millis() - debut_while > 1000)
    {
      info = "Envoie video Time Out";
      Serial.println(info);
      client_secure.stop();
      return false;
    }
  }
  client_secure.print(tail);
  client_secure.stop();
  
  return true;
}

void send_video(uint8_t *buf, size_t buf_size)
{
//  uint32_t debut = millis();
  uint8_t tentatives = 0;
  bool reussite = true;
  while (!sv(buf, buf_size))
  {
    tentatives++;
    if (tentatives > 5)
    {
      reussite = false;
      Serial.println("Echec");
      break;
    }
    vTaskDelay(2);
  }
  free(buf);
  buf = nullptr;

  if (reussite) info = "Video envoyé";
  else          info = "Video pas envoyé";
}

#ifdef SD
bool sv(File file)
{
  if (!file)
  {
    Serial.println("Le fichier est invalide.");
    return false;
  }

  size_t fileSize = file.size();

  NetworkClientSecure client_secure;
  client_secure.setInsecure();

  if (!client_secure.connect("api.telegram.org", 443))
  {
    Serial.println("Erreur de connexion à Telegram");
    file.close();
    return false;
  }
  Serial.println("Connexion à Telegram réussie");

  // Préparation des parties de la requête
  String tail = "\r\n--boundary--\r\n";
  String head = "--boundary\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + String(chatID) +
                "\r\n--boundary\r\nContent-Disposition: form-data; name=\"video\"; filename=\"video.avi\"\r\nContent-Type: video/avi\r\n\r\n";
  
  client_secure.print(String("POST /bot") + botToken + "/sendVideo HTTP/1.1\r\n");
  client_secure.print("Host: api.telegram.org\r\n");
  client_secure.print("Content-Type: multipart/form-data; boundary=boundary\r\n");
  client_secure.print("Content-Length: " + String(head.length() + fileSize + tail.length()) + "\r\n");
  client_secure.print("\r\n");
  client_secure.print(head);

  // Buffer pour lire les données du fichier
  size_t bufferSize = 2048; // Réduit pour éviter la surcharge mémoire
  uint8_t buf[bufferSize];
  size_t totalSent = 0;
  uint16_t count = 0;
  while (file.available())
  {
    uint32_t debut_while = millis();
    size_t len = file.read(buf, bufferSize);
    client_secure.write(buf, len);
    totalSent += len;

    info = "Envoie: " + String(100 * totalSent / fileSize) + "%";
    if (++count % 10 == 0 ) vTaskDelay(10);

    if (millis() - debut_while > 1000)
    {  
      info = "Envoie Video SD Time Out";
      Serial.println(info);
      client_secure.stop();
      file.close();
      return false;
    }
  }
  client_secure.print(tail);
  client_secure.stop();
  file.close();
  return true;
}

void send_video(File file)
{
//  uint32_t debut = millis();
  uint8_t tentatives = 0;
  bool reussite = false;
  while (!sv(file))
  {
    reussite = true;
    tentatives++;
    if (tentatives > 5)
    {
      reussite = false;
      Serial.println("Echec");
      break;
    }
    vTaskDelay(2);
  }


  Serial.println("Envoi du fichier terminé.");

  info = "Envoyé";
}
#endif

void loop_telegram()
{
  if (photo_telegram)
  {
    Serial.println("photo_telegram");
    send_photo(newCameraFrame());
    photo_telegram = false;
  }

#ifdef SD
  if (envoi_video_sd)
  {
    Serial.println("Telegram video");
    send_video(myFile(avi_file_name));
    envoi_video_sd = false;
  }
#endif

  if (envoi_video_psram)
  {
    Serial.println("envoi video psram");
    send_video(videoBuffer, bufPos);
    envoi_video_psram = false;
  }
}
