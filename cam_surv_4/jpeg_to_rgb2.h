#include "JPEGDEC.h"

uint8_t reduction_factor = 1;
uint8_t reduction_compteur = 0;
uint8_t resize_control = 16; // {128, 64, 32, 16, 8, 4, 2};

struct JpegContext {
  JPEGDEC decoder;
  camera_fb_t output;
  uint8_t reductionFactor;
  bool grayscale;
};

inline uint8_t rgb565_to_gray(uint16_t pixel) {
  uint8_t r = (pixel >> 11) & 0x1F;
  uint8_t g = (pixel >> 5) & 0x3F;
  uint8_t b = pixel & 0x1F;
  return (r * 299 + g * 587 + b * 114) / 1000;
}

int drawMCU(JPEGDRAW *pDraw) {
  JpegContext *ctx = (JpegContext *)pDraw->pUser;
  camera_fb_t out = ctx->output;
  uint8_t f = ctx->reductionFactor;

  int x0 = pDraw->x / f;
  int y0 = pDraw->y / f;
  int outW = pDraw->iWidth / f;
  int outH = pDraw->iHeight / f;

  uint16_t *pixels = pDraw->pPixels;

  for (int y = 0; y < outH; y++) {
    for (int x = 0; x < outW; x++) {
      int srcX = x * f;
      int srcY = y * f;
      int srcIdx = srcY * pDraw->iWidth + srcX;
      int dstIdx = (y0 + y) * out.width + (x0 + x);

      if ((x0 + x) >= out.width || (y0 + y) >= out.height) continue;

      if (ctx->grayscale) {
        uint16_t pixel = pixels[srcIdx];
        uint8_t r = (pixel >> 11) & 0x1F;
        uint8_t g = (pixel >> 5) & 0x3F;
        uint8_t b = pixel & 0x1F;
        out.buf[dstIdx] = (r * 299 + g * 587 + b * 114) / 1000;
      } else {
        dstIdx *= 2;
        uint16_t pixel = pixels[srcIdx];
        out.buf[dstIdx] = (pixel >> 8) & 0xFF;
        out.buf[dstIdx + 1] = pixel & 0xFF;
      }
    }
  }

  return 1;
}

camera_fb_t conv_jpeg_to(camera_fb_t input, uint8_t scale = 8, bool gray = true)
{
  static bool isrunning = false;
  while (isrunning) vTaskDelay(1);
  isrunning = true;

  JpegContext ctx;
  ctx.reductionFactor = scale > 8 ? scale / 8 : 1;
  uint8_t jpegScale = scale > 8 ? 8 : scale;
  ctx.grayscale = gray;

  int outW = input.width / (jpegScale * ctx.reductionFactor);
  int outH = input.height / (jpegScale * ctx.reductionFactor);
  int outLen = gray ? outW * outH : outW * outH * 2;

  ctx.output.width = outW;
  ctx.output.height = outH;
  ctx.output.format = gray ? PIXFORMAT_GRAYSCALE : PIXFORMAT_RGB565;
  ctx.output.buf = (uint8_t *)malloc(outLen);
  ctx.output.len = 0;

  if (!ctx.output.buf) {
    Serial.println("Erreur malloc");
    isrunning = false;
    return ctx.output;
  }

  if (ctx.decoder.openRAM(input.buf, input.len, drawMCU)) {
    ctx.decoder.setUserPointer(&ctx);
    ctx.decoder.setPixelType(RGB565_LITTLE_ENDIAN);
    ctx.decoder.decode(0, 0, jpegScale);
    ctx.decoder.close();
    ctx.output.len = outLen;
  } else {
    free(ctx.output.buf);
    ctx.output.buf = nullptr;
    ctx.output.len = 0;
    Serial.println("Erreur JPEG decode");
  }

  isrunning = false;
  return ctx.output;
}

camera_fb_t conv_jpeg_to(uint8_t scale = 8, bool gray = true)
{
  static bool isrunning = false;
  while (isrunning) vTaskDelay(1);
  isrunning = true;

  JpegContext ctx;
  ctx.reductionFactor = scale > 8 ? scale / 8 : 1;
  uint8_t jpegScale = scale > 8 ? 8 : scale;
  ctx.grayscale = gray;

  camera_fb_t input = newCameraFrame();
  int outW = input.width / (jpegScale * ctx.reductionFactor);
  int outH = input.height / (jpegScale * ctx.reductionFactor);
  int outLen = gray ? outW * outH : outW * outH * 2;

  ctx.output.width = outW;
  ctx.output.height = outH;
  ctx.output.format = gray ? PIXFORMAT_GRAYSCALE : PIXFORMAT_RGB565;
  ctx.output.buf = (uint8_t *)malloc(outLen);
  ctx.output.len = 0;

  if (!ctx.output.buf) {
    Serial.println("Erreur malloc");
    isrunning = false;
    return ctx.output;
  }

  if (ctx.decoder.openRAM(input.buf, input.len, drawMCU)) {
    ctx.decoder.setUserPointer(&ctx);
    ctx.decoder.setPixelType(RGB565_LITTLE_ENDIAN);
    ctx.decoder.decode(0, 0, jpegScale);
    ctx.decoder.close();
    ctx.output.len = outLen;
  } else {
    free(ctx.output.buf);
    ctx.output.buf = nullptr;
    ctx.output.len = 0;
    Serial.println("Erreur JPEG decode");
  }

  isrunning = false;
  return ctx.output;
}
