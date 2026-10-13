#ifndef __SSD1315_H__
#define __SSD1315_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define SSD1315_WIDTH   128U
#define SSD1315_HEIGHT   64U

uint8_t SSD1315_Init(void);
uint8_t SSD1315_IsReady(void);
uint8_t SSD1315_GetAddress(void);
void SSD1315_Clear(void);
void SSD1315_SetCursor(uint8_t x, uint8_t y);
void SSD1315_WriteChar(char character);
void SSD1315_WriteString(const char *text);
uint8_t SSD1315_Update(void);

#ifdef __cplusplus
}
#endif

#endif /* __SSD1315_H__ */
