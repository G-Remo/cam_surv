#include <fb_gfx.h>
#include "jpeg_to_rgb.h"
//#include "jpeg_to_rgb2.h"

#define TASK_COMPARE_DELAY 200

bool delete_task_compare = false;
camera_fb_t fb_control;
bool control, control_stream;

struct Zone {
    uint8_t x, y, w, h;
};

Zone zones = {0, 0, 100, 100};

bool surv, envoi_photo;
uint8_t seuil_1 = 25;
volatile uint16_t pix_detected;
uint8_t temps_photo = 5;
uint8_t min_pixel_detection = 15;
uint8_t couleur_box_x, couleur_box_y;
uint8_t taille_couleur_box = 1;
uint8_t couleur_seuil = 50;
uint16_t bbox_couleur = 65535, couleur_min = 65504, couleur_max = 65534;

camera_fb_t resize( camera_fb_t input, uint16_t width, uint16_t height)
{
  camera_fb_t out;
  
  // Gestion du format RGB565
  if (input.format == PIXFORMAT_RGB565) {
    // Si la taille est déjà la même, retourner l'image d'entrée
    if (input.width == width && input.height == height) return input;
    
    // Définir les dimensions et allouer le buffer pour la sortie
    out.width = width;
    out.height = height;
    out.len = out.width * out.height * 2;  // RGB565 -> 2 octets par pixel
    out.format = PIXFORMAT_RGB565;
    out.buf = (uint8_t*) malloc(out.len);

    // Redimensionnement de l'image en conservant le rapport des pixels
    for (size_t y = 0; y < out.height; y++) {
      for (size_t x = 0; x < out.width; x++) {
        size_t input_x = x * input.width / out.width;
        size_t input_y = y * input.height / out.height;

        // Copie des deux octets pour chaque pixel (format RGB565)
        out.buf[(y * out.width + x) * 2]     = input.buf[(input_y * input.width + input_x) * 2]; 
        out.buf[(y * out.width + x) * 2 + 1] = input.buf[(input_y * input.width + input_x) * 2 + 1];
      }
    }
    
    // Libérer le buffer de l'entrée
    purge_fb(input);
    return out;
  }

  // Gestion du format GRAYSCALE
  else if (input.format == PIXFORMAT_GRAYSCALE) {
    // Si la taille est déjà la même, retourner l'image d'entrée
    if (input.width == width && input.height == height) return input;
    
    // Définir les dimensions et allouer le buffer pour la sortie
    out.width = width;
    out.height = height;
    out.len = out.width * out.height;  // GRAYSCALE -> 1 octet par pixel
    out.format = PIXFORMAT_GRAYSCALE;
    out.buf = (uint8_t*) malloc(out.len);

    // Redimensionnement de l'image en conservant le rapport des pixels
    for (size_t y = 0; y < out.height; y++) {
      for (size_t x = 0; x < out.width; x++) {
        size_t input_x = x * input.width / out.width;
        size_t input_y = y * input.height / out.height;

        // Copie d'un seul octet pour chaque pixel (format GRAYSCALE)
        out.buf[y * out.width + x] = input.buf[input_y * input.width + input_x];
      }
    }
    
    // Libérer le buffer de l'entrée
    purge_fb(input);
    return out;
  }

  // Si le format n'est ni RGB565 ni GRAYSCALE, retourner un framebuffer vide
  return out;
}

camera_fb_t crop_square( camera_fb_t input, size_t startX, size_t startY, size_t box)
{
  camera_fb_t out;

  // Vérification des limites du crop pour éviter les dépassements
  if (startX + box > input.width || startY + box > input.height) {
    return out; // Retourner un framebuffer vide si la zone de crop est hors des limites
  }

  // Gestion du format RGB565
  if (input.format == PIXFORMAT_RGB565) {
    // Définir les dimensions et allouer le buffer pour la sortie
    out.width = box;
    out.height = box;
    out.format = PIXFORMAT_RGB565;
    out.len = out.width * out.height * 2;  // RGB565 -> 2 octets par pixel
    out.buf = (uint8_t*) malloc(out.len);

    // Découpe de l'image d'entrée
    for (int y = 0; y < box; ++y) {
      for (int x = 0; x < box; ++x) {
        int inputIndex = ((startY + y) * input.width + (startX + x)) * 2;
        int outputIndex = (y * box + x) * 2;

        // Copie des deux octets pour chaque pixel (format RGB565)
        out.buf[outputIndex]     = input.buf[inputIndex];
        out.buf[outputIndex + 1] = input.buf[inputIndex + 1];
      }
    }
  }
  // Gestion du format GRAYSCALE
  else if (input.format == PIXFORMAT_GRAYSCALE) {
    // Définir les dimensions et allouer le buffer pour la sortie
    out.width = box;
    out.height = box;
    out.format = PIXFORMAT_GRAYSCALE;
    out.len = out.width * out.height;  // GRAYSCALE -> 1 octet par pixel
    out.buf = (uint8_t*) malloc(out.len);

    // Découpe de l'image d'entrée
    for (int y = 0; y < box; ++y) {
      for (int x = 0; x < box; ++x) {
        int inputIndex = (startY + y) * input.width + (startX + x);
        int outputIndex = y * box + x;

        // Copie d'un seul octet pour chaque pixel (format GRAYSCALE)
        out.buf[outputIndex] = input.buf[inputIndex];
      }
    }
  } else {
    return out;  // Si le format n'est pas reconnu, retourner un framebuffer vide
  }

  // Libérer le buffer de l'entrée
  purge_fb(input);

  return out;
}

camera_fb_t conv_to_jpg(camera_fb_t input)
{
  if (input.format == PIXFORMAT_JPEG) return input;

  camera_fb_t out;
  out.width = input.width;
  out.height = input.height;
  out.format = PIXFORMAT_JPEG;

  fmt2jpg(input.buf, input.len, input.width, input.height, input.format, 90, &out.buf, &out.len);

  purge_fb(input);

  return out;
}

camera_fb_t conv_to_jpg(uint8_t *buf, uint8_t width, uint8_t height)
{
  camera_fb_t out;
  out.width = width;
  out.height = height;
  out.format = PIXFORMAT_JPEG;

  fmt2jpg(buf, width * height, width, height, PIXFORMAT_GRAYSCALE, 100, &out.buf, &out.len);
  
  return out;
}

camera_fb_t detecteur_zone_select(camera_fb_t input)
{
  fb_data_t rfb;
  rfb.width = input.width;
  rfb.height = input.height;
  rfb.data = input.buf;
  rfb.bytes_per_pixel = 2;
  rfb.format = FB_RGB565;

    uint16_t x, y, w, h;

  x = zones.x * input.width / 100;
  y = zones.y * input.height / 100;
  w = zones.w * (input.width - ( zones.x * input.width / 100)) / 100;
  h = zones.h * (input.height - ( zones.y * input.height / 100)) / 100;


  fb_gfx_drawFastHLine(&rfb, x, y, w, bbox_couleur);
  fb_gfx_drawFastHLine(&rfb, x, y + h - 1, w, bbox_couleur);
  fb_gfx_drawFastVLine(&rfb, x, y, h, bbox_couleur);
  fb_gfx_drawFastVLine(&rfb, x + w - 1, y, h, bbox_couleur);

  return input;
}

camera_fb_t couleur_zone_select(camera_fb_t input)
{
  uint16_t box;

  while(true)
  {
    box = 16 * pow( 2, taille_couleur_box);
    if( box <= input.width && box <= input.height) break;
    taille_couleur_box--;
  }

  uint16_t bbox_x_max = (input.width - box) * 100 / input.width;
  uint16_t bbox_y_max = (input.height - box) * 100 / input.height;

  if (couleur_box_x >= bbox_x_max) couleur_box_x = bbox_x_max;
  if (couleur_box_y >= bbox_y_max) couleur_box_y = bbox_y_max;
  
  uint16_t x = couleur_box_x * input.width / 100;
  uint16_t y = couleur_box_y * input.height / 100;

  fb_data_t rfb;
  rfb.width = input.width;
  rfb.height = input.height;
  rfb.data = input.buf;
  rfb.bytes_per_pixel = 2;
  rfb.format = FB_RGB565;

  fb_gfx_drawFastHLine(&rfb, x, y, box, bbox_couleur);
  fb_gfx_drawFastHLine(&rfb, x, y + box - 1, box, bbox_couleur);
  fb_gfx_drawFastVLine(&rfb, x, y, box, bbox_couleur);
  fb_gfx_drawFastVLine(&rfb, x + box - 1, y, box, bbox_couleur);

  return input;
}

camera_fb_t couleur(camera_fb_t input , bool text = false)
{
  camera_fb_t out;
  out.width = input.width;
  out.height = input.height;
  out.len = out.width * out.height * 2;
  out.format = PIXFORMAT_RGB565;
  out.buf = (uint8_t*) malloc(out.len);

  uint32_t nb_couleur = 0;

  for (uint32_t i = 0; i < out.len/2; i++) {
    uint16_t myPix = ( input.buf[i * 2] << 8 | input.buf[i * 2 + 1]);
    
    if( myPix >= couleur_min && myPix <= couleur_max)
    {
      out.buf[i * 2 + 1] = input.buf[i * 2 + 1];
      out.buf[i * 2] = input.buf[i * 2];
      nb_couleur++;
    }
    else
    {
      out.buf[i * 2 + 1] = 0;
      out.buf[i * 2] = 0;
    }
  }
    
  purge_fb(input);

  if (text)
  {
    fb_data_t rfb;
    rfb.width = out.width;
    rfb.height = out.height;
    rfb.data = out.buf;
    rfb.bytes_per_pixel = 2;
    rfb.format = FB_RGB565;

    uint16_t color = (couleur_min + couleur_max) / 2;
    char str[4];
    uint8_t val = nb_couleur * 100 / (input.len / 2);
    sprintf( str, "%u", val);

    fb_gfx_print(&rfb, 2, 2, color, str);
  }

  return out;
}

camera_fb_t couleur_zone(camera_fb_t input)
{
  uint16_t box;
  while(true)
  {
    box = 16 * pow(2, taille_couleur_box);
    if( box <= input.width && box <= input.height) break;
    taille_couleur_box--;
  }

  uint16_t bbox_x_max = (input.width - box) * 100 / input.width;
  uint16_t bbox_y_max = (input.height - box) * 100 / input.height;

  if(couleur_box_x >= bbox_x_max) couleur_box_x = bbox_x_max;
  if( couleur_box_y >= bbox_y_max ) couleur_box_y = bbox_y_max;
  
  uint16_t x = couleur_box_x * input.width / 100;
  uint16_t y = couleur_box_y * input.height / 100;

  return crop_square(input, x, y, box);
}

bool detect_couleur()
{
  camera_fb_t input = couleur( couleur_zone( conv_jpeg_to(4, false) ) );

  uint32_t nb_couleur = 0;

  for (uint16_t i = 0; i < input.len/2; i++) {
    uint16_t myPix = ( input.buf[i * 2] << 8 | input.buf[i * 2 + 1]);
    
    if( myPix >= couleur_min && myPix <= couleur_max)
    {
      nb_couleur++;
    }
  }
  purge_fb(input);

  uint8_t val = nb_couleur * 100 / ( input.len / 2 );
  
  if( val >= couleur_seuil ) return true;
  return false;
}

/*
 * tache detection mouvements
 */
void task_compare(void *t)
{
  Serial.println("task_compare");
  surv = true;
  envoi_photo = true;
  uint16_t task_delay;
  uint32_t TimePhoto;
  uint32_t debut;
  camera_fb_t frame_original = newCameraFrame();
  camera_fb_t frame_old = conv_jpeg_to(frame_original, resize_control);
  purge_fb(frame_original);
  camera_fb_t frame_new;
  uint8_t *nb_buf = NULL;
  for(;;)
  {
    while (!surv) vTaskDelay(500);

    if (delete_task_compare) vTaskDelete(NULL);
    
    task_delay = TASK_COMPARE_DELAY;
    debut = millis();

    frame_original = newCameraFrame();
    frame_new = conv_jpeg_to(frame_original, resize_control);

    if (frame_new.width != frame_old.width ||
        frame_new.height != frame_old.height ||
        frame_new.buf == nullptr)
    {
//      if (frame_new.buf != nullptr) purge_fb(frame_new);
//      if (frame_old.buf != nullptr) purge_fb(frame_old);
      purge_fb(frame_new);
      purge_fb(frame_old);
      frame_old = conv_jpeg_to(resize_control);

      vTaskDelay(10);
      continue;
    }
    
    nb_buf = (uint8_t *)malloc(frame_new.width * frame_new.height * sizeof(uint8_t));
    if (nb_buf == NULL)
    {
      Serial.println("Erreur d'allocation mémoire");
      continue;
    }
    
    // Initialisation du buffer à zéro
    memset(nb_buf, 0, frame_new.width * frame_new.height * sizeof(uint8_t));
    
    uint16_t moveX = 0;
    uint16_t moveY = 0;
    uint16_t numPixels = 0;  // Tableau pour stocker les pixels détectés par zone
    
    for (uint8_t y = 0; y < frame_new.height; y++) {
      for (uint8_t x = 0; x < frame_new.width; x++) {
        if (abs(frame_old.buf[y * frame_new.width + x] - frame_new.buf[y * frame_new.width + x]) > seuil_1) {
          moveX += x;
          moveY += y;

          uint16_t xx = zones.x * frame_new.width / 100;
          uint16_t yy = zones.y * frame_new.height / 100;
          uint16_t ww = xx + (zones.w * (frame_new.width - xx) / 100);
          uint16_t hh = yy + (zones.h * (frame_new.height - yy) / 100);

          if ((x >= xx) && (x <= ww) && (y >= yy) && (y <= hh)) {
            nb_buf[y * frame_new.width + x] = 255;
            numPixels++;
          }

        }
      }
    }
    
    purge_fb(frame_old);

    pix_detected = 0;
    // Affichage du nombre de pixels détectés par zone

//      Serial.printf("Zone %d: %u pixels détectés\n", i + 1, numPixels);
    pix_detected += numPixels;

    if( numPixels > min_pixel_detection)
    {
      if( millis() - TimePhoto  > temps_photo * 1000)
      {
        TimePhoto = millis();
        Serial.println("photo");

        if (envoi_photo)
        {
          camera_fb_t photo = copyCameraFrame(frame_original);
          sendToQueue(photo);
        }
      }
    }

    purge_fb(frame_original);
    
    if (control_stream)
    {
      fb_control = conv_to_jpg(nb_buf, frame_new.width, frame_new.height);
      control_stream = false;
      control = true;
    }

    free(nb_buf);
    frame_old = copyCameraFrame(frame_new);
    purge_fb(frame_new);
           
    uint16_t fin = millis() - debut;
    
    if (fin >= task_delay) task_delay =  200;
    else                   task_delay -= fin;

//    Serial.printf("task av: %u ms\n", millis() - debut);

//    Serial.printf("task_compare: %u octets\n", uxTaskGetStackHighWaterMark(nullptr));
//    Serial.printf("task_delay: %u ms\n", task_delay);

    vTaskDelay(task_delay);

//    Serial.printf("task ap: %u ms\n\n", millis() - debut);
  }
}

void start_task_compare()
{
  xTaskCreatePinnedToCore(task_compare, "task_compare", 4000, NULL, 1, NULL, 0);
}
