#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "fast_functions.h"

void lcd_send(uint8_t value, uint8_t mode)
{
    Wire.beginTransmission(0x27);
    Wire.write((value & 0xf0 | mode | En) | LCD_BACKLIGHT);
    Wire.write((value & 0xf0 | mode & ~En) | LCD_BACKLIGHT);
    Wire.write(((value << 4) & 0xf0 | mode | En) | LCD_BACKLIGHT);
    Wire.write(((value << 4) & 0xf0 | mode & ~En) | LCD_BACKLIGHT);
    Wire.endTransmission();
}

inline void lcd_command(uint8_t value)
{
    lcd_send(value, 0);
}

inline void lcd_printChar(char ch)
{
    lcd_send(ch, Rs);
}

void lcd_setCursor(uint8_t col, uint8_t row)
{
    lcd_command(LCD_SETDDRAMADDR | (col + row * 0x40));
}

void lcd_print(char *str)
{
    while (*str)
    {
        lcd_printChar(*str);
        ++str;
    }
}
/************************************
       Digital I/O functions
************************************/

void pinMod(uint8_t pin, uint8_t mode)
{
    switch (mode)
    {
    case INPUT:
        if (pin < 8)
        {
            bitClear(DDRD, pin);
            bitClear(PORTD, pin);
        }
        else if (pin < 14)
        {
            bitClear(DDRB, (pin - 8));
            bitClear(PORTB, (pin - 8));
        }
        else if (pin < 20)
        {
            bitClear(DDRC, (pin - 14)); // Mode: INPUT
            bitClear(PORTC, (pin - 8)); // State: LOW
        }
        return;
    case OUTPUT:
        if (pin < 8)
        {
            bitSet(DDRD, pin);
            bitClear(PORTD, pin);
        }
        else if (pin < 14)
        {
            bitSet(DDRB, (pin - 8));
            bitClear(PORTB, (pin - 8));
        }
        else if (pin < 20)
        {
            bitSet(DDRC, (pin - 14));   // Mode: OUTPUT
            bitClear(PORTC, (pin - 8)); // State: LOW
        }
        return;
    case INPUT_PULLUP:
        if (pin < 8)
        {
            bitClear(DDRD, pin);
            bitSet(PORTD, pin);
        }
        else if (pin < 14)
        {
            bitClear(DDRB, (pin - 8));
            bitSet(PORTB, (pin - 8));
        }
        else if (pin < 20)
        {
            bitClear(DDRC, (pin - 14)); // Mode: OUTPUT
            bitSet(PORTC, (pin - 14));  // State: HIGH
        }
        return;
    }
}

void pinWrite(uint8_t pin, bool x)
{
    switch (pin)
    {
    case 3:
        bitClear(TCCR2A, COM2B1);
        break;
    case 5:
        bitClear(TCCR0A, COM0B1);
        break;
    case 6:
        bitClear(TCCR0A, COM0A1);
        break;
    case 9:
        bitClear(TCCR1A, COM1A1);
        break;
    case 10:
        bitClear(TCCR1A, COM1B1);
        break;
    case 11:
        bitClear(TCCR2A, COM2A1); // PWM disable
        break;
    }
    if (pin < 8)
    {
        bitWrite(PORTD, pin, x);
    }
    else if (pin < 14)
    {
        bitWrite(PORTB, (pin - 8), x);
    }
    else if (pin < 20)
    {
        bitWrite(PORTC, (pin - 14), x); // Set pin to HIGH / LOW
    }
}

bool pinRead(uint8_t pin)
{
    if (pin < 8)
    {
        return bitRead(PIND, pin);
    }
    else if (pin < 14)
    {
        return bitRead(PINB, pin - 8);
    }
    else if (pin < 20)
    {
        return bitRead(PINC, pin - 14); // Return pin state
    }
}

void pinToggle(uint8_t pin)
{
    if (pin < 8)
    {
        bitSet(PIND, pin);
    }
    else if (pin < 14)
    {
        bitSet(PINB, (pin - 8));
    }
    else if (pin < 20)
    {
        bitSet(PINC, (pin - 14)); // Toggle pin state (for 'tone()')
    }
}