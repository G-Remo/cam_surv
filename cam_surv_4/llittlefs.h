#include <LittleFS.h>

void deleteFile(const char *path)
{
  Serial.printf("Deleting file: %s\n", path);
  if(LittleFS.remove(path)) Serial.println("File deleted");
  else                      Serial.println("Delete failed");
}

void writeFile( const char *path, String message)
{
  if (message.length() == 0)
  {
    Serial.printf("Ecriture annulée : message vide pour le fichier %s\n", path);
    return;
  }

  Serial.printf("Écriture de : %s -> ", path);
  Serial.printf("message : %s\n", message.c_str());
  File file = LittleFS.open(path, FILE_WRITE);
  if(!file)
  {
    Serial.println("erreur d'acces");
    return;
  }
  if(file.print(message)) Serial.println(message);
  else Serial.println("erreur d'ecriture");
  file.close();
}

const char* readFile(const char* path)
{
  static char buffer[256];

  Serial.printf("Lecture de: %s -> ", path);

  File file = LittleFS.open(path, "r");
  
  if (file.isDirectory())
  {
    Serial.println("Erreur d'accès au fichier");
    return "0";
  }

  if (!file)
  {
    writeFile(path, "0");
    Serial.println("Pas de fichier, cretion");
    return "0";
  }

  size_t len = file.readBytesUntil('\n', buffer, sizeof(buffer) - 1);  // Lire jusqu'à '\n'
  buffer[len] = '\0';  // Terminer la chaîne par un caractère nul

  file.close();
  Serial.println(buffer);

  // Retourner une nouvelle copie de la chaîne
  return strdup(buffer);  // Allouer dynamiquement la mémoire pour éviter l'écrasement
}

void init_mdp()
{
  hote = readFile("/hote.txt");
  if (!strcmp(hote, "0")) hote = "ESP32";
  
  ssid = readFile("/ssid.txt");
  ssid2 = readFile("/ssid2.txt");
  password = readFile("/password.txt");
  local_ip.fromString(readFile("/local_ip.txt"));
  ip_tablette.fromString(readFile("/ip_tablette.txt"));
  chatID = String(readFile("/chatID.txt")).toInt(); 
  botToken = readFile( "/botToken.txt");
}

void deinit_littlefs()
{
  LittleFS.end();
  Serial.println("Deinit littlefs");
}

void init_littlefs()
{
  if( !LittleFS.begin(true))
  {
    Serial.println("Pas de systeme de fichier\nFormatage et redemarrage\n");
    ESP.restart();
  }
  Serial.println("Init Littlefs");
}

void init_littlefs_mdp()
{
  init_littlefs();

  init_mdp();

//  deinit_littlefs();
}
