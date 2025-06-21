#include "JPEGDEC.h"

JPEGDEC jpeg;
camera_fb_t reduced;

uint8_t reduction_factor = 1;
uint8_t reduction_compteur = 0;
uint8_t resize_control = 16; // {128, 64, 32, 16, 8, 4, 2};

inline uint8_t pix_565_to_gray(uint8_t color1, uint8_t color2)
{
  // Extraire les composants R, G, B du format RGB565
  uint8_t r1 = (color1 & 0xF8) >> 3;
  uint8_t g1 = ((color1 & 0x07) << 3) | ((color2 & 0xE0) >> 5);
  uint8_t b1 = color2 & 0x1F;

  // Formule de conversion en niveaux de gris avec des coefficients ajustés
  double gray = (0.299 * r1 + 0.587 * g1 + 0.114 * b1) * 7;

  // Normaliser à la plage de 0 à 255
  gray = constrain(gray, 0.0, 255.0);

  return gray;
}

inline int drawMCU(JPEGDRAW *pDraw)
{
  uint16_t x_origine = pDraw->x;
  uint16_t y_origine = pDraw->y / reduction_factor;
  uint16_t out_w = pDraw->iWidth / reduction_factor;
  uint16_t out_h = pDraw->iHeight / reduction_factor;

  if (out_h == 0) out_h = 1;

  // Correction : vérifier les limites des boucles et les indices
  if (reduction_factor > 1)
  {
    reduction_compteur++;
    if (reduction_compteur % reduction_factor != 0) return 1;
  }

  if (reduced.format == PIXFORMAT_GRAYSCALE)
  {
    for (int y = 0; y < out_h; y++) {
      for (int x = 0; x < out_w; x++) {
        uint16_t input_y = y * reduction_factor;
        uint16_t input_x = x * reduction_factor;

        // Calcul de la position dans le buffer de sortie
        uint32_t output_index = ((y_origine + y) * reduced.width) + (x_origine + x);

        // Correction des indices de l'image d'entrée
        if (input_y < pDraw->iHeight && input_x < pDraw->iWidth &&
            (y_origine + y) < reduced.height && (x_origine + x) < reduced.width) {
          reduced.buf[output_index] = 
              pix_565_to_gray((pDraw->pPixels[input_y * pDraw->iWidth + input_x] >> 8) & 0xFF,
                              pDraw->pPixels[input_y * pDraw->iWidth + input_x] & 0xFF);
        }
      }
    }
    reduced.len += out_h * out_w;
  }
  else
  {
    for (int y = 0; y < out_h; y++) {
      for (int x = 0; x < out_w; x++) {
        uint16_t input_y = y * reduction_factor;
        uint16_t input_x = x * reduction_factor;

        // Calcul de la position dans le buffer de sortie
        uint32_t output_index = (((y_origine + y) * reduced.width) + (x_origine + x)) * 2;

        // Correction des indices de l'image d'entrée
        if (input_y < pDraw->iHeight && input_x < pDraw->iWidth &&
            (y_origine + y) < reduced.height && (x_origine + x) < reduced.width) {
          reduced.buf[output_index] = (pDraw->pPixels[input_y * pDraw->iWidth + input_x] >> 8) & 0xFF;
          reduced.buf[output_index + 1] = pDraw->pPixels[input_y * pDraw->iWidth + input_x] & 0xFF;
        }
      }
    }
    reduced.len += out_h * out_w * 2;
  }

  return 1;
}

inline bool conv_jpeg(camera_fb_t in, uint8_t scale = 8)
{
  reduction_factor = 1;
  if (scale > 8)
  {
    reduction_factor = scale / 8;
    scale = 8;
  }

  bool jdec_ok = false;

  if (jpeg.openRAM(in.buf, in.len, drawMCU))
  {
    reduced.width = in.width / (scale * reduction_factor);
    reduced.height = in.height / (scale * reduction_factor);
    reduced.len = reduced.width * reduced.height;
    reduced.format = PIXFORMAT_GRAYSCALE;

    reduced.buf = (uint8_t*) malloc(reduced.len);
    reduction_compteur = 0;
    reduced.len = 0;
    
    if( jpeg.decode(0, 0, scale)) jdec_ok = true;
    jpeg.close();
    
  }

  if (!jdec_ok) return false;

  return true;
}

camera_fb_t conv_jpeg_to(uint8_t scale = 8, bool gray = true)
{
  static bool isrunning = false;
  while (isrunning) vTaskDelay(1);
  isrunning = true;
  
  reduction_factor = 1;
  if (scale > 8)
  {
    reduction_factor = scale / 8;
    scale = 8;
  }

  uint8_t nb_erreurs = 0;
  for(;;)
  {
    camera_fb_t input_image = newCameraFrame();
    bool jdec_ok = false;

    if (jpeg.openRAM(input_image.buf, input_image.len, drawMCU))
    {
      reduced.width = input_image.width / (scale * reduction_factor);
      reduced.height = input_image.height / (scale * reduction_factor);
      if (gray)
      {
        reduced.len = reduced.width * reduced.height;
        reduced.format = PIXFORMAT_GRAYSCALE;
      }
      else
      {
        reduced.len = reduced.width * reduced.height * 2;
        reduced.format = PIXFORMAT_RGB565;
      }

      reduced.buf = (uint8_t*) malloc(reduced.len);
      reduction_compteur = 0;
      reduced.len = 0;
      
      if( jpeg.decode(0, 0, scale)) jdec_ok = true;
      jpeg.close();
    }

    purge_fb(input_image);
    if(jdec_ok) break;
    
    purge_fb(reduced);
    Serial.println("Erreur resize_xx");
    nb_erreurs++;
    if (nb_erreurs > 3) ESP.restart(); 
  }

  isrunning = false;

  return reduced;
}

camera_fb_t conv_jpeg_to(camera_fb_t input_image = newCameraFrame(), uint8_t scale = 8, bool gray = true)
{
  static bool isrunning = false;
  while (isrunning) vTaskDelay(10);
  isrunning = true;
  
  reduction_factor = 1;
  if (scale > 8)
  {
    reduction_factor = scale / 8;
    scale = 8;
  }

  if (jpeg.openRAM(input_image.buf, input_image.len, drawMCU))
  {
    reduced.width = input_image.width / (scale * reduction_factor);
    reduced.height = input_image.height / (scale * reduction_factor);
    if (gray)
    {
      reduced.len = reduced.width * reduced.height;
      reduced.format = PIXFORMAT_GRAYSCALE;
    }
    else
    {
      reduced.len = reduced.width * reduced.height * 2;
      reduced.format = PIXFORMAT_RGB565;
    }

    reduced.buf = (uint8_t*) malloc(reduced.len);
    reduction_compteur = 0;
    reduced.len = 0;
    
    jpeg.decode(0, 0, scale);
    jpeg.close();
  }

//  purge_fb(reduced);
  isrunning = false;

  return reduced;
}
