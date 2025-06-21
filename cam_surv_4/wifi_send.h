#include <WiFi.h>
#include <WiFiClient.h>

IPAddress ip_tablette;
const IPAddress ip_pont_wifi(192, 168, 4, 1);
const uint16_t serverPort = 5000;
bool envoi_photo_tablette = true;

void wifi_send(uint8_t *buf, size_t len)
{
  WiFiClient client;

  // Connexion au serveur
  IPAddress ip = ip_tablette;
  
  if (!isConnectedToPrimary)
  {
    ip = ip_pont_wifi;
  }

  Serial.print("wifi_send, ip: ");
  Serial.println(ip);
  if (client.connect(ip, serverPort, 5000))
  {
    String texte = String(hote);
    size_t texteSize = texte.length();

    // Envoyer la taille du texte
    client.write((uint8_t*)&texteSize, sizeof(texteSize));
    client.write(texte.c_str(), texteSize);

    // Envoyer la taille de l'image
    client.write((uint8_t*)&len, sizeof(len));

    uint16_t count = 0;
    const size_t bufferSize = 1024;
    size_t totalSent = 0;
    while (totalSent < len)
    {
      size_t chunkSize = min(bufferSize, len - totalSent);
      client.write(buf + totalSent, chunkSize);
      totalSent += chunkSize;
//      if (++count % 20 == 0) vTaskDelay(1);
    }

    client.clear();
    client.stop();
    Serial.println("wifi_send: Envoyé");
  }
}
