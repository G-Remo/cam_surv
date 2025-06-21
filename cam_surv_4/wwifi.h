#include <WiFi.h>
#include <esp_wifi.h>
#include <ArduinoOTA.h>

const char *ssid, *ssid2, *password, *hote;
IPAddress local_ip, gateway(192, 168, 1, 1), subnet(255, 255, 255, 0), dns(8, 8, 8, 8), zero(0, 0, 0, 0);

bool isConnectedToPrimary = false;
bool isSoftAP = false;
bool wifi_power_max;

void set_wifi_max_pow(bool val)
{
  wifi_power_max = val;
  if(!val)
  {
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    Serial.println("Normal WiFi power");
  }
  else
  {
    esp_wifi_set_ps(WIFI_PS_NONE);
    Serial.println("Max WiFi power");
  }
}

bool wifi_ok()
{
  if (WiFi.status() != WL_CONNECTED) return false;
  return true;
}

bool wifi_check()
{
  if (!isConnectedToPrimary) return false;
  if (isSoftAP) return false;
  
  WiFiClient client;
  
  if (client.connect(gateway, 80))
  {
    client.println("GET / HTTP/1.1");
    client.println("Host: example.com");
    client.println("Connection: close");
    client.println();
    client.stop();
    return true;
  }

  return false;
}

void stop_access_point()
{
  if (!isSoftAP) return;
  
  WiFi.softAPdisconnect(true);
  isSoftAP = false;
}

void startAccessPoint()
{
  if (isSoftAP) return;
  
  WiFi.disconnect();
  while (WiFi.status() == WL_CONNECTED) vTaskDelay(1);
  WiFi.mode(WIFI_AP);
  Serial.println("Creation d'un point de connexion WiFi sans mdp");
  WiFi.softAP(hote);
  Serial.print("Access Point: ");
  Serial.print(hote);
  Serial.print(" démarré avec l'IP: ");
  Serial.println(WiFi.softAPIP());
  isSoftAP = true;
}

bool connectToAP(const char *_ssid,
                 const char *_password,
                 IPAddress _local_ip,
                 IPAddress _gateway, 
                 IPAddress _subnet,
                 IPAddress _dns)
{
  WiFi.disconnect();
  stop_access_point();
  while (WiFi.status() == WL_CONNECTED) vTaskDelay(1);
  WiFi.mode(WIFI_STA);
  WiFi.config(_local_ip, _gateway, _subnet, _dns);
  WiFi.begin(_ssid, _password);
  
  Serial.print("Connexion a: " + String(_ssid));
  uint8_t attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20)
  {
    vTaskDelay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nIP: " + WiFi.localIP().toString());
    return true;
  }

  Serial.println("\nÉchec de connexion.");
  return false;
}

void wifi_start()
{
  // Déconnecter du WiFi et réinitialiser l'état de l'AP
  WiFi.disconnect();
//  stop_access_point();

  // Attendre la déconnexion complète
  while (WiFi.status() == WL_CONNECTED)  vTaskDelay(2);

  // Scanner les réseaux disponibles
  bool ssid_found = false;
  bool ssid2_found = false;
  int network_count = WiFi.scanNetworks();

  for (int i = 0; i < network_count; i++)
  {
    String currentSSID = WiFi.SSID(i);

    if (currentSSID == ssid)
    {
      Serial.println("Réseau trouvé : " + currentSSID);
      ssid_found = true;
    }
    else if (currentSSID == ssid2)
    {
      Serial.println("Réseau trouvé : " + currentSSID);
      ssid2_found = true;
    }
  }

  // Supprimer les résultats du scan pour libérer de la mémoire
  WiFi.scanDelete();

  // Logique de connexion en fonction des réseaux disponibles
  if (ssid_found)
  {
    isConnectedToPrimary = connectToAP(ssid, password, local_ip, gateway, subnet, dns);
    if (isConnectedToPrimary) return;
  }
  
  if (ssid2_found)
  {
    if (connectToAP(ssid2, password, zero, zero, zero, zero)) return;
  }

  startAccessPoint();
}

void test_connexion()
{
  static uint32_t _time;
  if (millis() - _time < 1 * 60 * 1000) return;
  _time = millis();
    
  if (WiFi.status() != WL_CONNECTED || !wifi_check())
  {
    isConnectedToPrimary = false;
    Serial.println("Perte de connexion. Tentative de reconnexion...");
    
    wifi_start();
  }
  else
  {
    if (!isConnectedToPrimary)
    {
      Serial.println("Vérification périodique du routeur principal...");
      wifi_start();
    }
    else
    {
      Serial.println("Connecté au réseau principal.");
    }
  }
}

void task_wifi_ota(void *t)
{
  // Configuration de l'OTA
  ArduinoOTA.onStart([]() {
    Serial.println("Début de la mise à jour OTA...");
    info = "OTA: 00%";
    delete_task_compare = true;
    vTaskDelay(200);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nFin de la mise à jour OTA");
  });
  
  static uint8_t lastProgress = 0;  // Variable pour stocker le dernier pourcentage affiché
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    uint8_t currentProgress = (progress * 100) / total;

    // Afficher uniquement tous les 10%
    if (currentProgress >= lastProgress + 10) {
      Serial.printf("Progression: %u%%\n", currentProgress);
      info = "OTA: " + String(currentProgress) + " %";
      lastProgress = currentProgress;
    }
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Erreur [%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Erreur d'authentification");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Erreur de début");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Erreur de connexion");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Erreur de réception");
    else if (error == OTA_END_ERROR) Serial.println("Erreur de fin");
  });
  
  ArduinoOTA.setHostname(hote);
  ArduinoOTA.begin();
  
  Serial.println("OTA demarre");

  for(;;)
  {
    ArduinoOTA.handle();
    test_connexion();

    vTaskDelay(pdMS_TO_TICKS(2000));

//   uint32_t temp1 = uxTaskGetStackHighWaterMark(nullptr);
//   Serial.print("task_ota: "); Serial.println(temp1);
  }
}

void init_wifi()
{
//  set_wifi_max_pow(true);
  WiFi.setSleep(false);
  WiFi.setHostname(hote);
  wifi_start();

  xTaskCreatePinnedToCore(task_wifi_ota, "task_wifi_ota", 4000, NULL, 1, NULL, 1);
}
