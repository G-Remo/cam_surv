#include <SD_MMC.h>
#include <vector>
#include <queue>

struct FileInfo {
  String name;
  size_t size;
  String lastWrite;

  bool operator<(const FileInfo &other) const {
    // Comparer les fichiers par date (les plus anciens d'abord)
    return lastWrite > other.lastWrite;
  }
};

bool createDir(const char *path)
{
  if (!SD_MMC.exists(path))
  {
    if (!SD_MMC.mkdir(path))
    {
      Serial.printf("Erreur creation: %s\n", path);
      return false;
    }
  }
  return true;
}

void nettoyage(const char *dirname, size_t maxSizeInBytes)
{
  Serial.printf("Ouverture de %s\n", dirname);

  File root = SD_MMC.open(dirname);
  if (!root)
  {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  std::priority_queue<FileInfo> files;
  size_t totalSize = 0;

  while (file)
  {
    if ( !file.isDirectory())
    {
      FileInfo fileInfo;
      fileInfo.name = file.name();
      fileInfo.size = file.size();
      fileInfo.lastWrite = file.getLastWrite();
      files.push(fileInfo);
      totalSize += file.size();
    }
    file = root.openNextFile();
  }
  root.close();
  Serial.printf("Size of %s: %u Mo\n", dirname, totalSize / (1024 * 1024));

  // Calculer la taille totale et supprimer les fichiers au besoin
  size_t currentSize = 0;
  while (!files.empty() && totalSize - currentSize > maxSizeInBytes) {
    const auto &oldestFile = files.top();
    Serial.printf("Deleting oldest file: %s\n", oldestFile.name.c_str());
    char tmp_name[30];
    sprintf( tmp_name, "%s/%s", dirname, oldestFile.name.c_str());
    Serial.printf("tmp_name: %s\n", tmp_name);
    size_t tmp_size = oldestFile.size;
    if ( SD_MMC.remove(tmp_name))
    {
      currentSize += tmp_size;
      files.pop();
    }
    else
    {
      Serial.println("Erreur suppression");
      return;
    }
  }
}

void enregistrer_photo()
{
  if ( !SD_MMC.begin("/sdcard", true ))
  {
    Serial.println("SD Mount Failed");
    return;
  }

  char adresse[30] = "";
  camera_fb_t fb = newCameraFrame();

  createDir("/Photos");

  sprintf( adresse, "/Photos/%s.jpg", horodatage());
  Serial.println(adresse);
  File file = SD_MMC.open(adresse, FILE_WRITE);

  if ( !file)
  {
    Serial.println("Echec lors de la creation du fichier.");
  }
  else
  {
    file.write( fb.buf, fb.len); // req (image), req length
    Serial.printf("Fichier enregistre: %s\n", adresse);

  }
  file.close();
  purge_fb(fb);
}

uint16_t sd_libre()
{
  return (SD_MMC.totalBytes() - SD_MMC.usedBytes()) / (1024 * 1024);
}

uint16_t sd_total()
{
  return SD_MMC.totalBytes() / (1024 * 1024);
}

String sd_text()
{
  uint8_t cardType = SD_MMC.cardType();

  if (cardType == CARD_NONE) return "Pas de carte SD";

  String sd_text = "SD libre: ";
  if (sd_libre() > 1024) sd_text += String(sd_libre() / 1024) + " Go";
  else                   sd_text += String(sd_libre()) + " Mo";

  return sd_text;
}

void delete_old_stuff()
{
  Serial.printf( "SD libre: %u%%\n", 100 * sd_libre() / sd_total());
  if ( 100 * sd_libre() / sd_total() > 20 ) return;
  nettoyage("/Videos", sd_total() / 2);
  nettoyage("/Photos", sd_total() / 3);
}

void deinit_sd()
{
  SD_MMC.end();
}

void init_sd()
{
  if ( !SD_MMC.begin("/sdcard", true) )
  {
    Serial.println("Erreur init sd");
    return;
  }
  if ( sd_text() == "SD UNKNOWN")
  {
    Serial.println("Erreur type sd");
    return;
  }
  
  delete_old_stuff();
  Serial.println(sd_text());
}

File myFile(String my_file)
{
  File file = SD_MMC.open(my_file, FILE_READ);

  // Vérifie si le fichier a été ouvert correctement
  if (!file)
  {
    Serial.println("Erreur d'ouverture du fichier");
    return File(); // Retourne un objet File invalide
  }

  return file;
}
