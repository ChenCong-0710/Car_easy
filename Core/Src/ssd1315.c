#include "ssd1315.h"
#include "i2c.h"
#include <string.h>

#define SSD1315_ADDR_0          (0x3CU << 1)
#define SSD1315_ADDR_1          (0x3DU << 1)
#define SSD1315_PAGE_COUNT      (SSD1315_HEIGHT / 8U)
#define SSD1315_COMMAND_TIMEOUT 50U
#define SSD1315_DATA_TIMEOUT    50U

static uint8_t framebuffer[SSD1315_WIDTH * SSD1315_PAGE_COUNT];
static uint8_t oled_address;
static uint8_t cursor_x;
static uint8_t cursor_y;
static uint8_t oled_ready;

static const uint8_t font_digits[10][5] =
{
  {0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU},
  {0x00U, 0x42U, 0x7FU, 0x40U, 0x00U},
  {0x42U, 0x61U, 0x51U, 0x49U, 0x46U},
  {0x21U, 0x41U, 0x45U, 0x4BU, 0x31U},
  {0x18U, 0x14U, 0x12U, 0x7FU, 0x10U},
  {0x27U, 0x45U, 0x45U, 0x45U, 0x39U},
  {0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U},
  {0x01U, 0x71U, 0x09U, 0x05U, 0x03U},
  {0x36U, 0x49U, 0x49U, 0x49U, 0x36U},
  {0x06U, 0x49U, 0x49U, 0x29U, 0x1EU}
};

static const uint8_t font_letters[26][5] =
{
  {0x7EU, 0x11U, 0x11U, 0x11U, 0x7EU},
  {0x7FU, 0x49U, 0x49U, 0x49U, 0x36U},
  {0x3EU, 0x41U, 0x41U, 0x41U, 0x22U},
  {0x7FU, 0x41U, 0x41U, 0x22U, 0x1CU},
  {0x7FU, 0x49U, 0x49U, 0x49U, 0x41U},
  {0x7FU, 0x09U, 0x09U, 0x09U, 0x01U},
  {0x3EU, 0x41U, 0x49U, 0x49U, 0x7AU},
  {0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU},
  {0x00U, 0x41U, 0x7FU, 0x41U, 0x00U},
  {0x20U, 0x40U, 0x41U, 0x3FU, 0x01U},
  {0x7FU, 0x08U, 0x14U, 0x22U, 0x41U},
  {0x7FU, 0x40U, 0x40U, 0x40U, 0x40U},
  {0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU},
  {0x7FU, 0x04U, 0x08U, 0x10U, 0x7FU},
  {0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU},
  {0x7FU, 0x09U, 0x09U, 0x09U, 0x06U},
  {0x3EU, 0x41U, 0x51U, 0x21U, 0x5EU},
  {0x7FU, 0x09U, 0x19U, 0x29U, 0x46U},
  {0x46U, 0x49U, 0x49U, 0x49U, 0x31U},
  {0x01U, 0x01U, 0x7FU, 0x01U, 0x01U},
  {0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU},
  {0x1FU, 0x20U, 0x40U, 0x20U, 0x1FU},
  {0x3FU, 0x40U, 0x38U, 0x40U, 0x3FU},
  {0x63U, 0x14U, 0x08U, 0x14U, 0x63U},
  {0x07U, 0x08U, 0x70U, 0x08U, 0x07U},
  {0x61U, 0x51U, 0x49U, 0x45U, 0x43U}
};

static HAL_StatusTypeDef SSD1315_WriteCommand(uint8_t command)
{
  return HAL_I2C_Mem_Write(&hi2c3, oled_address, 0x00U,
                           I2C_MEMADD_SIZE_8BIT, &command, 1U,
                           SSD1315_COMMAND_TIMEOUT);
}

static void SSD1315_GetGlyph(char character, uint8_t glyph[5])
{
  const uint8_t *source = NULL;

  if ((character >= 'a') && (character <= 'z'))
  {
    character = (char)(character - ('a' - 'A'));
  }

  if ((character >= '0') && (character <= '9'))
  {
    source = font_digits[(uint8_t)(character - '0')];
  }
  else if ((character >= 'A') && (character <= 'Z'))
  {
    source = font_letters[(uint8_t)(character - 'A')];
  }

  if (source != NULL)
  {
    memcpy(glyph, source, 5U);
  }
  else
  {
    memset(glyph, 0, 5U);
    if (character == ':')
    {
      glyph[2] = 0x36U;
    }
    else if (character == '.')
    {
      glyph[2] = 0x60U;
    }
    else if (character == '-')
    {
      glyph[1] = 0x08U;
      glyph[2] = 0x08U;
      glyph[3] = 0x08U;
    }
    else if (character != ' ')
    {
      const uint8_t question[5] = {0x02U, 0x01U, 0x51U, 0x09U, 0x06U};
      memcpy(glyph, question, 5U);
    }
  }
}

uint8_t SSD1315_Init(void)
{
  static const uint8_t init_commands[] =
  {
    0xAEU, 0xD5U, 0x80U, 0xA8U, 0x3FU, 0xD3U, 0x00U, 0x40U,
    0x8DU, 0x14U, 0x20U, 0x02U, 0xA1U, 0xC8U, 0xDAU, 0x12U,
    0x81U, 0x7FU, 0xD9U, 0xF1U, 0xDBU, 0x40U, 0xA4U, 0xA6U,
    0x2EU, 0xAFU
  };

  oled_ready = 0U;
  oled_address = 0U;
  HAL_Delay(100U);

  if (HAL_I2C_IsDeviceReady(&hi2c3, SSD1315_ADDR_0, 2U, 20U) == HAL_OK)
  {
    oled_address = SSD1315_ADDR_0;
  }
  else if (HAL_I2C_IsDeviceReady(&hi2c3, SSD1315_ADDR_1, 2U, 20U) == HAL_OK)
  {
    oled_address = SSD1315_ADDR_1;
  }
  else
  {
    return 0U;
  }

  for (uint32_t index = 0U; index < sizeof(init_commands); index++)
  {
    if (SSD1315_WriteCommand(init_commands[index]) != HAL_OK)
    {
      return 0U;
    }
  }

  SSD1315_Clear();
  oled_ready = 1U;
  if (SSD1315_Update() == 0U)
  {
    oled_ready = 0U;
  }

  return oled_ready;
}

uint8_t SSD1315_IsReady(void)
{
  return oled_ready;
}

uint8_t SSD1315_GetAddress(void)
{
  return (uint8_t)(oled_address >> 1);
}

void SSD1315_Clear(void)
{
  memset(framebuffer, 0, sizeof(framebuffer));
  cursor_x = 0U;
  cursor_y = 0U;
}

void SSD1315_SetCursor(uint8_t x, uint8_t y)
{
  cursor_x = x;
  cursor_y = y;
}

void SSD1315_WriteChar(char character)
{
  uint8_t glyph[5];

  if ((cursor_x > (SSD1315_WIDTH - 6U)) || (cursor_y > (SSD1315_HEIGHT - 8U)))
  {
    return;
  }

  SSD1315_GetGlyph(character, glyph);
  for (uint8_t column = 0U; column < 5U; column++)
  {
    for (uint8_t row = 0U; row < 7U; row++)
    {
      const uint32_t pixel_index = (uint32_t)(cursor_y + row) / 8U * SSD1315_WIDTH +
                                   cursor_x + column;
      const uint8_t pixel_mask = (uint8_t)(1U << ((cursor_y + row) & 0x07U));
      if ((glyph[column] & (1U << row)) != 0U)
      {
        framebuffer[pixel_index] |= pixel_mask;
      }
      else
      {
        framebuffer[pixel_index] &= (uint8_t)~pixel_mask;
      }
    }
  }

  cursor_x = (uint8_t)(cursor_x + 6U);
}

void SSD1315_WriteString(const char *text)
{
  if (text == NULL)
  {
    return;
  }

  while (*text != '\0')
  {
    SSD1315_WriteChar(*text);
    text++;
  }
}

uint8_t SSD1315_Update(void)
{
  if (oled_ready == 0U)
  {
    return 0U;
  }

  for (uint8_t page = 0U; page < SSD1315_PAGE_COUNT; page++)
  {
    if ((SSD1315_WriteCommand((uint8_t)(0xB0U + page)) != HAL_OK) ||
        (SSD1315_WriteCommand(0x00U) != HAL_OK) ||
        (SSD1315_WriteCommand(0x10U) != HAL_OK) ||
        (HAL_I2C_Mem_Write(&hi2c3, oled_address, 0x40U,
                           I2C_MEMADD_SIZE_8BIT,
                           &framebuffer[(uint32_t)page * SSD1315_WIDTH],
                           SSD1315_WIDTH, SSD1315_DATA_TIMEOUT) != HAL_OK))
    {
      oled_ready = 0U;
      return 0U;
    }
  }

  return 1U;
}
