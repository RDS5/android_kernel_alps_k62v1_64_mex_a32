
#ifndef _LCDKIT_H_
#define _LCDKIT_H_

#define LCD_PANEL_ID_LEN 30
char lcd_id[LCD_PANEL_ID_LEN];

#define TP_COLOR_LEN 50
char tpcolor_buf[TP_COLOR_LEN];

typedef enum
{
	LCD_HW_ID0 = 0x00,//ID0 low   , ID1  low
	LCD_HW_ID1 = 0x01,//ID0 high  , ID1  low
	LCD_HW_ID2 = 0x02,//ID0 float , ID1  low
	LCD_HW_ID4 = 0x04,//ID0 low   , ID1  high
	LCD_HW_ID5 = 0x05,//ID0 high  , ID1  high
	LCD_HW_ID6 = 0x06,//ID0 float , ID1  high
	LCD_HW_ID8 = 0x08,//ID1 float , ID0  low
	LCD_HW_ID9 = 0x09,//ID1 float , ID0  high
	LCD_HW_IDA = 0x0A,//ID0 float , ID1  float
	LCD_HW_ID_MAX = 0xFF,
} hw_lcd_id_index;

extern void hw_gpio_tlmm_config(unsigned int gpio, unsigned char func,
			  unsigned char dir, unsigned char pull, unsigned char drvstr);


#endif

