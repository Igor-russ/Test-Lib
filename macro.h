//****************************** M A C R O   B E G I N ************************************************************************************************
//#define DEBUG                                     // Для режима отладки нужно раскомментировать эту строку
#ifdef DEBUG                      // В случае существования DEBUG
#define DPRINT(x) Serial.print(x) // Создаем "переадресацию" на стандартную функцию
#define DPRINTLN(x) Serial.println(x)
#else // Если DEBUG не существует - игнорируем упоминания функций
#define DPRINT(x)
#define DPRINTLN(x)
#endif
#if defined(ARDUINO) && ARDUINO >= 100
#define printByte(args) write(args);
#else
#define printByte(args) print(args, BYTE);
#endif
#define lcd_print_P(x) lcd.print((const __FlashStringHelper *)(x))
//#define bitToggle(value, bit) ((value) ^= (1UL << bit))
#ifndef _NOP
#define _NOP()                   \
    do                           \
    {                            \
        __asm__ volatile("nop"); \
    } while (0)
#endif
//===========================
#define EVERY_MS(x)                    \
    static uint32_t tmr;               \
    bool flag = millis() - tmr >= (x); \
    if (flag)                          \
        tmr += (x);                    \
    if (flag)
//===========================
//****************************** M A C R O   E N D *****************************************************************************************************