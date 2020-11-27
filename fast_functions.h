#ifndef __FAST_FUNCTIONS_H
#define __FAST_FUNCTIONS_H
#include <Arduino.h>

void lcd_print(char *str);
inline void lcd_printChar(char ch);
void lcd_send(uint8_t value, uint8_t mode);
inline void lcd_command(uint8_t value);
inline void lcd_printChar(char ch);
void lcd_setCursor(uint8_t col, uint8_t row);
void pinMod(uint8_t pin, uint8_t mode);
void pinWrite(uint8_t pin, bool x);
bool pinRead(uint8_t pin);
void pinToggle(uint8_t pin);

#endif