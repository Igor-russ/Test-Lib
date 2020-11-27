#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <GyverButton.h>
#include <avr/pgmspace.h>
#include <GyverStepper.h>
#include <GyverTimers.h>
//******************* Распределение пинов и определение констант ********************************
enum
{
    BUTTON_PIN = 3,
    PLUS_PIN = 4,
    MINUS_PIN = 5,
    STEP_PIN = 6,
    DIR_PIN = 7,
    ENABLE_PIN = 8,
    START_PIN = 9,
    STOP_PIN = 12,
    BACKLIGHT_PIN = 10
};
enum
{
    MOTOR_STEPS = 200, //Количество шагов двигателя за один оборот.
    MICROSTEPS = 16,   //Количество микрошагов на один шаг.
    MAX_LITERS_PER_HOUR = 30
};
enum
{
    ENDLESS,
    DOSATOR,
    CARBONIZE,
    CALIBRATE
};                                                       //Режимы работы насоса.
const int16_t STEPS_PER_TURN = MOTOR_STEPS * MICROSTEPS; // Количество шагов для одного оборота двигателя.
const int16_t ONE_SECOND = 1000;                         //Одна секунда в млсек.
const int16_t RPM_TO_SMOOTH = 120;                       //RPM при котором будет плавный разгон двигателя.
const int16_t MAX_RPM = 300;
const int16_t LCD_ADDR = 0x27;
byte TURNS = 10; //Количество оборотов для калибровки насоса.
//**************** Прототипы функций ************************************************************
int8_t readEncoder();               // Чтение показаний энкодера.
void checkEvents();                 // Проверка разных событий.
void staticDataOnLcd();             // Выполняется одно из: "Непрерывно", "Дозатор", "По времени", "Калибровка".
void dynamicDataOnLcd();            //Динамичеки меняющиеся данные на LCD.
void startButtonHandler();          //Обработка нажатия кнопки СТАРТ
void stopButtonHandle();            //Обработка нажатия кнопки СТОП
void holdingEncoderButtonHandler(); //Обработка удержания кнопки энкодера
void blinkLcd(byte count, bool change_TURNS);
bool countdownTime();               //Обратный отсчет установленного времени углевания.
void startMotor();                  //Запуск двигателя в зависимости от требуемой скорости.
void verifyEndOfRunningModes();     //Проверка окончания дозировки, калибровки, углевания.
void verifyButtonsPress();          //Проверка нажатия кнопок СТАРТ, СТОП и кнопки энкодера.
void intToString(int x, char *str); //Преобразование целого числа в строку
void intToString(int x, char *str, int d);
void invertString(char *str, int len);
//****************Конец прототипов функций*******************************************************
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
GButton encoder_button(BUTTON_PIN);
GButton start_button(START_PIN);
GButton stop_button(STOP_PIN);
GStepper<STEPPER2WIRE> stepper(STEPS_PER_TURN, STEP_PIN, DIR_PIN, ENABLE_PIN);
//************* Символы в памяти программ (Flash) **************
const byte P[8] PROGMEM = {0x1F, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x00};  //П
const byte l[8] PROGMEM = {0x00, 0x00, 0x07, 0x09, 0x11, 0x11, 0x11, 0x00};  //л
const byte ch[8] PROGMEM = {0x00, 0x00, 0x11, 0x11, 0x11, 0x0F, 0x01, 0x00}; //ч
const byte m[8] PROGMEM = {0x00, 0x00, 0x11, 0x1B, 0x15, 0x11, 0x11, 0x00};  //м
const byte b[8] PROGMEM = {0x07, 0x08, 0x04, 0x0E, 0x11, 0x11, 0x0E, 0x00};  //б
const byte z[8] PROGMEM = {0x00, 0x00, 0x06, 0x09, 0x02, 0x01, 0x09, 0x06};  //з
const byte i[8] PROGMEM = {0x00, 0x00, 0x11, 0x13, 0x15, 0x19, 0x11, 0x00};  //и
const byte n[8] PROGMEM = {0x00, 0x00, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x00};  //н
//************ ГЛОБАЛЬНЫЕ переменные *************************************************
//Переменная периода (в микросекундах) импульсов на выходе STEP_PIN в зависимости
//unsigned long step_pulse = 0;//от желаемой скорости вращения двигателя RPM (оборотов в минуту).
bool toggle_dir = 0;                       // Направление вращения
float ml_per_Nturns = 0;                   //Производительность насоса в мл. за TURNS оборотов.
byte ml_per_Nturns_EEPROM = ml_per_Nturns; //Для записи в EEPROM.
int16_t hours = 0;                         // Переменные для
int16_t minutes = 0;                       //обратного отсчета
int16_t secundes = 0;                      // в режиме Углевания.
int16_t steps_per_sec;                     //скорость мотора в шагах/сек.
int16_t Minutes = 0;                       //Для хранения времени в минутах в режиме Углевания (с обратным отсчетом).
int16_t turns_to_dose = 0;                 //Количество необходимых оборотов в режиме Розлив.
float ml_per_turn = ml_per_Nturns / TURNS; //Производительность насоса в мл. за один оборот.
float liters_per_hour = 10;                //Требуемая производительность в литрах за час.
float fl_turns_to_dose = 0;
int16_t rpm = 0;                      //Обороты в минуту.
int16_t ml_to_dose = 500;             // Объём для режима ДОЗИРОВКА
bool carbonize_running = 0;           //Процесс  углевания.
bool start_pressed = 0;               //Фиксация события нажатой кнопки СТАРТ
bool stop_pressed = 0;                //Фиксация события нажатой кнопки СТОП
bool double_click = 0;                //Фиксация события двойного нажатия на кнопку энкодера.
bool regim_changed = 0;               //Фиксация смены режима.
bool smooth_start = false;            //Фиксация режима разгона.
bool change_TURNS = 0;                //Флаг разрешения изменения переменной TURNS.
long step_pulse = 0;                  //Период импульсов STEP в микросекундах. Только для интереса.
long step_frequency = 0;              //Частота импульсов STEP в Герцах.Только для интереса.
long move_to_position = 0;            //Установка целевой позиции в шагах
byte regim = 0;                       //Режимы работы ("Непрерывно", "Дозатор", "Углевание", "Калибровка").
const char dir_cw[] PROGMEM = ">>>";  // Символ вращения по часовой стрелке.
const char dir_ccw[] PROGMEM = "<<<"; // Символ вращения против часовой стрелки.
const char colon[] PROGMEM = ":";
//*************************************************************************************