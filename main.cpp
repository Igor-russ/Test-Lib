//Огромное спасибо Саше aka Gyver & Co (https://alexgyver.ru/) за использованные мной его библиотек.
//Двигатель в режимах ENDLESS, DOSATOR и CALIBRATE работает по тику из основного цикла loop.
//В режиме CARBONIZE двигатель тактируется по прерыванию от Таймера2, так как вывод на LCD
//дисплей очень ресурсоёмок. В безтаймерном режиме двигатель подтормаживает/дёргается
//в момент вывода времени на дисплей. Вдобавок используются быстрые функции для работы
//с дисплеем: lcd_setCursor, lcd_print. Всю работу на таймере не стал делать, так как при тактировании
//из основного цикла loop двигатель работает мягче (тише).
#include <Arduino.h>
#include "macro.h"
#include "init.h"
#include "fast_functions.h"

ISR(TIMER2_A)
{
  stepper.tick();
}
//*******************************************************************
void setup()
{
  while (!Serial) 
    ;
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  encoder_button.setTickMode(AUTO);
  start_button.setTickMode(AUTO);
  stop_button.setTickMode(AUTO);
  encoder_button.setTimeout(1000);
  stepper.autoPower(true);
  stepper.setAcceleration(1000);
  stepper.setRunMode(KEEP_SPEED);
  stepper.disable();
  //************************** Инициализация портов *******************************************************
  pinMod(BUTTON_PIN, INPUT_PULLUP); //Порты энкодера на вход.
  pinMod(MINUS_PIN, INPUT_PULLUP);  // +
  pinMod(PLUS_PIN, INPUT_PULLUP);   // Включить подтягивающие резисторы.
  pinMod(START_PIN, INPUT_PULLUP);  // Порты кнопок на вход, включить подтягивающие резисторы для кнопок..
  pinMod(STOP_PIN, INPUT_PULLUP);   // Включить подтягивающие резисторы для кнопок.
  //********************************************************************************************************
  //************** Создание символов ***********************************************************************
  lcd.createChar_P(0, l);  //   л
  lcd.createChar_P(1, ch); //   ч
  lcd.createChar_P(2, m);  //   м
  lcd.createChar_P(3, P);  //   П
  lcd.createChar_P(4, i);  //   и
  lcd.createChar_P(5, b);  //   б
  lcd.createChar_P(6, z);  //   з
  lcd.createChar_P(7, n);  //   н
  //*******************************************************************************************************
  //EEPROM.write(0, ml_per_Nturns_EEPROM);//Выполнить один раз и закомментировать.
  //EEPROM.write(1, TURNS);//Выполнить один раз и закомментировать.
  analogWrite(BACKLIGHT_PIN, 30); //Установка уровня подсветки ЖКИ экрана.
  TURNS = EEPROM.read(1);
  ml_per_Nturns_EEPROM = EEPROM.read(0);
  ml_per_Nturns = ml_per_Nturns_EEPROM;
  ml_per_turn = ml_per_Nturns / TURNS;
  rpm = round((liters_per_hour * 1000L) / (60L * ml_per_turn));
#ifdef DEBUG
  Serial.print(F("ml_per_turn = "));
  Serial.println(ml_per_turn);
  Serial.print(F("liters/hour = "));
  Serial.println(liters_per_hour);
  Serial.print(F("RPM = "));
  Serial.println(rpm);
  Serial.print(F("step_pulse = "));
  Serial.print(60L * 1000L * 1000L / MOTOR_STEPS / MICROSTEPS / rpm);
  Serial.println(F("_us"));
  Serial.print(F("Step Frequency = "));
  DPRINT(1000000 / (60L * 1000L * 1000L / MOTOR_STEPS / MICROSTEPS / rpm));
  Serial.println(" Hz");
  Serial.println("");
#endif
  Timer2.setPeriod(35);
  Timer2.disableISR();
  //Скорость обмена по I2C в КГц.
  //TWBR = (F_CPU / (SPEED * 1000l) - 16) / 2;
  Wire.setClock(400000); //Скорость обмена по I2C в Гц.
  DPRINT("TWBR = ");
  DPRINTLN(TWBR);
  staticDataOnLcd();
}

void loop()
{
  for (;;)
  {
    if (regim != CARBONIZE)
    {
      stepper.tick();
      Timer2.disableISR();
    }
    else
    {
      Timer2.enableISR();
    }
    checkEvents();
  }
}
//************** Нажатие кнопки *****************************
void checkEvents()
{
  if (!stepper.getState())
  {
    EVERY_MS(3)
    { //Двигатель стоит.
      verifyButtonsPress();
    }
  }
  else
  { //Двигатель вращается.
    EVERY_MS(25)
    {
      verifyEndOfRunningModes();
      stopButtonHandle();
    }
  }
}
//**********************************************************
void verifyButtonsPress()
{
  bool start_enable = ((regim == DOSATOR && ml_to_dose) || //Не реагировать на СТАРТ если ml_to_dose = 0
                       (regim == CARBONIZE && Minutes) ||  //Не реагировать на СТАРТ если Minutes = 0
                       regim == ENDLESS ||                 //Во всех остальных режимах - реагировать.
                       regim == CALIBRATE);
  if (start_button.isClick() && start_enable && !start_pressed)
  {
    start_pressed = 1;
    DPRINTLN("startHandler");
    startButtonHandler();
  }
  else if (encoder_button.isSingle())
  {                                        //Нажатие кнопки менее 1 сек.
    DPRINTLN(F("ENCODER BUTTON pressed")); // Переключение режимов только когда мотор стоит.
    regim_changed = 1;
    if (++regim >= 4)
    {
      regim = 0;
    }
    staticDataOnLcd();
  }
  else if (encoder_button.isDouble() && (regim == CALIBRATE || regim == DOSATOR))
  {
    double_click = 1;
  }
  else if (encoder_button.isHolded())
  {                                // Переключение направления вращения
    holdingEncoderButtonHandler(); //только когда мотор стоит.
  }
  else if (stop_button.isHolded() && regim == CALIBRATE)
  {
    change_TURNS = !change_TURNS;
    blinkLcd(1, change_TURNS);
  }
  dynamicDataOnLcd();
}
//*******************************************************************************
void verifyEndOfRunningModes()
{
  if (carbonize_running && countdownTime())
  {
    (smooth_start == true) ? stepper.stop() : stepper.brake();
    carbonize_running = 0;
    smooth_start = false;
    if (secundes < 10)
    {
      lcd.setCursor(11, 1);
      lcd.print(F("0"));
    }
    lcd.print(secundes);
  }
  if (regim == ENDLESS && stepper.getState())
  {
    static int8_t val;
    if ((val = readEncoder()))
    {
      liters_per_hour += val;
      if (liters_per_hour <= 1)
      {
        liters_per_hour = 1;
      }
      else if (liters_per_hour >= MAX_LITERS_PER_HOUR)
      {
        liters_per_hour = MAX_LITERS_PER_HOUR;
      }
      rpm = round((liters_per_hour * 1000L) / (60 * ml_per_turn));
      if (rpm > MAX_RPM)
      {
        rpm = MAX_RPM;
      }
      staticDataOnLcd();
      steps_per_sec = round((long)rpm * STEPS_PER_TURN / 60); //Вычисление скорости в шагах/сек.
      stepper.setSpeed(steps_per_sec, SMOOTH);
    }
  }
}

//********************************************************************************
void dynamicDataOnLcd()
{
  switch (regim)
  {
  case ENDLESS:
    if (!stepper.getState())
    {
      static int8_t val;
      if ((val = readEncoder()))
      {
        liters_per_hour += val;
        if (liters_per_hour <= 1)
        {
          liters_per_hour = 1;
        }
        else if (liters_per_hour >= MAX_LITERS_PER_HOUR)
        {
          liters_per_hour = MAX_LITERS_PER_HOUR;
        }
        rpm = round((liters_per_hour * 1000L) / (60 * ml_per_turn));
        if (rpm > MAX_RPM)
        {
          rpm = MAX_RPM;
        }
#ifdef DEBUG
        step_pulse = 60L * 1000L * 1000L / MOTOR_STEPS / MICROSTEPS / rpm;
        step_frequency = 1000000L / step_pulse;
        Serial.print(F("liters/hour = "));
        Serial.println(liters_per_hour);
        Serial.print(F("RPM = "));
        Serial.println(rpm);
        Serial.print(F("step_pulse = "));
        Serial.print(step_pulse);
        Serial.println(F("_us"));
        Serial.print(F("Step Frequency = "));
        Serial.print(step_frequency);
        Serial.println(" Hz");
        Serial.print(F("ml_per_turn = "));
        Serial.println(ml_per_turn);
        Serial.println("");
#endif
        staticDataOnLcd();
      }
    }
    break;
  //**************************** Режим НАСОС ********************************
  case DOSATOR:
    static int8_t ml;
    static byte count = 0;
    if ((ml = readEncoder()))
    {
      ml_to_dose += 10 * ml;
      if (ml_to_dose < 0)
      {
        ml_to_dose = 0;
      }
      staticDataOnLcd();
    }
    if (double_click)
    {
      switch (count)
      {
      case 0:
        ml_to_dose = 700;
        count = 1;
        staticDataOnLcd();
        break;
      case 1:
        ml_to_dose = 1000;
        count = 2;
        staticDataOnLcd();
        break;
      case 2:
        ml_to_dose = 500;
        count = 0;
        staticDataOnLcd();
        break;
      }
      double_click = 0;
    }
    break;
  case CARBONIZE:
    if (!stepper.getState())
    { //Мотор стоит
      static int8_t val;
      if ((val = readEncoder()))
      {
        Minutes += 10 * val;
        if (Minutes <= 0)
        {
          Minutes = 0;
          secundes = 0;
        }
        hours = Minutes / 60;
        minutes = Minutes % 60;
        DPRINTLN(Minutes);
        DPRINTLN(hours);
        DPRINTLN(minutes);
        staticDataOnLcd();
      }
    }
    else
    { //Мотор крутится
      static char sc[] = "  ";
      static char mn[] = "  ";
      static char hr[] = "  ";
      countdownTime();
      static bool time_to_lcd = 0;
      static uint32_t currTime = millis(); 
      if (millis() - currTime >= ONE_SECOND)
      {
        hours = Minutes / 60;
        minutes = Minutes % 60;
        if (secundes == 60)
        {
          intToString(0, sc, 2);
        }
        else
        {
          intToString(secundes, sc, 2);
        }
        intToString(minutes, mn, 2);
        intToString(hours, hr, 2);
        time_to_lcd = 1;
        currTime = millis();
      }
      if (time_to_lcd)
      {
        EVERY_MS(1)
        {
          static byte cnt = 0;
          switch (cnt)
          {
          case 0:
            lcd_setCursor(11, 1);
            ++cnt;
            break;
          case 1:
            lcd_print(sc);
            ++cnt;
            break;
          case 2:
            lcd_setCursor(7, 1);
            ++cnt;
            break;
          case 3:
            lcd_print(mn);
            ++cnt;
            break;
          case 4:
            lcd_setCursor(3, 1);
            ++cnt;
            break;
          case 5:
            lcd_print(hr);
            cnt = 0;
            time_to_lcd = 0;
            break;
          }
        }
      }
    }
    break;
  case CALIBRATE:
    if (!stepper.getState())
    {
      static int8_t val;
      if (change_TURNS == 0)
      {
        if ((val = readEncoder()))
        {
          ml_per_Nturns += val;
          if (ml_per_Nturns < 1)
          {
            ml_per_Nturns = 1;
          }
          staticDataOnLcd();
        }
        if (double_click)
        {
          EEPROM.write(0, ml_per_Nturns);
          blinkLcd(3, change_TURNS);
          double_click = 0;
        }
      }
      else if (change_TURNS == 1)
      {
        if ((val = readEncoder()))
        {
          TURNS += val;
          if (TURNS < 10)
          {
            TURNS = 10;
          }
          else if (TURNS > 50)
          {
            TURNS = 50;
          }
          staticDataOnLcd();
        }
        if (double_click)
        {
          EEPROM.write(1, TURNS);
          blinkLcd(3, change_TURNS);
          double_click = 0;
        }
      }
    }
    break;
  }
}
//***************************************************************
void staticDataOnLcd()
{
  if (regim_changed == 1)
  {
    lcd.clear();
    regim_changed = 0;
    ml_per_Nturns_EEPROM = EEPROM.read(0);
    ml_per_Nturns = ml_per_Nturns_EEPROM;
    ml_per_turn = ml_per_Nturns / TURNS;
    rpm = round((liters_per_hour * 1000L) / (60 * ml_per_turn));
    if (rpm > MAX_RPM)
    {
      rpm = MAX_RPM;
    }
  }
  if (toggle_dir == 0)
  {
    lcd.setCursor(0, 0);
    lcd.print("   ");
    lcd.setCursor(13, 0);
    lcd_print_P(dir_cw); // вращения мотора <<< или >>>.
  }
  else
  {
    lcd.setCursor(13, 0);
    lcd.print("   ");
    lcd.setCursor(0, 0);
    lcd_print_P(dir_ccw);
  }
  //**************************** Режим НАСОС ********************************
  if (regim == ENDLESS)
  {
    lcd.setCursor(6, 0);
    lcd.print(F("Hacoc"));
    (liters_per_hour < 10) ? lcd.setCursor(2, 1) : lcd.setCursor(1, 1);
    if (liters_per_hour < 10)
    {
      lcd.setCursor(1, 1);
      lcd.print(F(" "));
    }
    lcd.print(liters_per_hour, 0); // Отображение "л/ч:"
    lcd.setCursor(4, 1);
    lcd.printByte(0);
    lcd.print(F("/"));
    lcd.printByte(1);
    lcd.setCursor(8, 1);
    lcd.print(F("rpm="));
    lcd.print(rpm);
    if (rpm < 100 && rpm >= 10)
    {
      lcd.setCursor(14, 1);
      lcd.print(F("  "));
    }
    else if (rpm < 10)
    {
      lcd.setCursor(13, 1);
      lcd.print(F("  "));
    }
    //******************* Режим РОЗЛИВ ************************************
  }
  else if (regim == DOSATOR)
  {
    char tmp[5]{};
    char lcdLine[18]{};
    lcd.setCursor(5, 0);
    lcd.print(F("Po"));
    lcd.printByte(6);
    lcd.printByte(0);
    lcd.printByte(4);
    lcd.print(F("B")); // Розлив
    lcd.setCursor(0, 1);
    intToString(ml_to_dose, tmp);
    if (ml_to_dose < 100)
    {
      strcpy(lcdLine, "       ");
      strcat(lcdLine, tmp);
      strcat(lcdLine, " ml.   ");
    }
    else if (ml_to_dose < 1000)
    {
      strcpy(lcdLine, "      ");
      strcat(lcdLine, tmp);
      strcat(lcdLine, " ml.   ");
    }
    else
    {
      strcpy(lcdLine, "      ");
      strcat(lcdLine, tmp);
      strcat(lcdLine, " ml.   ");
    }
    lcd.print(lcdLine);
    //******************* Режим УГЛЕВАНИЕ **********************************
  }
  else if (regim == CARBONIZE)
  {
    lcd.setCursor(3, 0);
    lcd.printByte(3);
    lcd.print(F("o Bpe"));
    lcd.printByte(2);
    lcd.print(F("e"));
    lcd.printByte(7);
    lcd.printByte(4); // По времени
    //Вторая строка.
    lcd.setCursor(5, 1);
    lcd.printByte(1);
    lcd.setCursor(6, 1);
    lcd.print(F(":"));
    lcd.setCursor(9, 1);
    lcd.printByte(2);
    lcd.setCursor(10, 1);
    lcd.print(F(":"));
    lcd.setCursor(13, 1);
    lcd.print(F("c"));
    (hours < 10) ? lcd.setCursor(4, 1) : lcd.setCursor(3, 1);
    lcd.print(hours);
    if (hours < 10)
    {
      lcd.setCursor(3, 1);
      lcd.print(F("0"));
    }
    (minutes < 10) ? lcd.setCursor(8, 1) : lcd.setCursor(7, 1);
    lcd.print(minutes);
    if (minutes < 10)
    {
      lcd.setCursor(7, 1);
      lcd.print(F("0"));
    }
    (secundes < 10) ? lcd.setCursor(12, 1) : lcd.setCursor(11, 1);
    if (secundes == 60)
    {
      lcd.print(F("00"));
    }
    else
    {
      if (secundes < 10)
      {
        lcd.setCursor(11, 1);
        lcd.print(F("0"));
      }
      lcd.print(secundes);
    }
    //******************* Режим КАЛИБРОВКА *********************************
  }
  else if (regim == CALIBRATE)
  {
    lcd.setCursor(3, 0);
    lcd.print(F("Ka"));
    lcd.printByte(0);
    lcd.printByte(4);
    lcd.printByte(5);
    lcd.print(F("poBka")); // Калибровка
    lcd.setCursor(0, 1);
    lcd.print(TURNS);
    lcd.print(F(" o"));
    lcd.printByte(5);
    lcd.print(F("op.:")); // обор.:
    lcd.setCursor(13, 1);
    lcd.printByte(2);
    lcd.printByte(0);
    lcd.print(F(".")); //мл.
    (ml_per_Nturns < 10) ? lcd.setCursor(11, 1) : lcd.setCursor(10, 1);
    lcd.print(ml_per_Nturns, 0);
    if (ml_per_Nturns < 10)
    {
      lcd.setCursor(10, 1);
      lcd.print(F("0"));
    }
  }
}
//******************************************************************************
void holdingEncoderButtonHandler()
{
  (toggle_dir == 1) ? stepper.reverse(false) : stepper.reverse(true);
  toggle_dir = !toggle_dir;
  staticDataOnLcd();
}
//******************************************************************************
void startButtonHandler()
{
  rpm = round((liters_per_hour * 1000L) / (60 * ml_per_turn));
  if (rpm > MAX_RPM)
  {
    rpm = MAX_RPM;
  }
  steps_per_sec = round((long)rpm * STEPS_PER_TURN / 60); //Вычисление скорости в шагах/сек.
  stepper.setAcceleration(steps_per_sec / 5);
  switch (regim)
  {
  //**************************** Режим НАСОС ********************************
  case ENDLESS:
#ifdef DEBUG
    step_pulse = 60L * 1000L * 1000L / MOTOR_STEPS / MICROSTEPS / rpm;
    step_frequency = 1000000L / step_pulse;
    Serial.print(F("liters/hour = "));
    Serial.println(liters_per_hour);
    Serial.print(F("RPM = "));
    Serial.println(rpm);
    Serial.print(F("step_pulse = "));
    Serial.print(step_pulse);
    Serial.println(F("_us"));
    Serial.print(F("Step Frequency = "));
    Serial.print(step_frequency);
    Serial.println(" Hz");
    Serial.print(F("ml_per_turn = "));
    Serial.println(ml_per_turn);
    Serial.println("");
#endif
    stepper.setRunMode(KEEP_SPEED);
    start_pressed = 0;
    startMotor();
    break;
  //******************* Режим РОЗЛИВ ************************************
  case DOSATOR:
    start_pressed = 0;
    fl_turns_to_dose = (float)ml_to_dose / ml_per_turn;
    turns_to_dose = (int)fl_turns_to_dose;
    stepper.setRunMode(FOLLOW_POS);
    stepper.setMaxSpeed(steps_per_sec);
    stepper.setCurrent(0);
    move_to_position = (long)STEPS_PER_TURN * (long)turns_to_dose;
    stepper.setTarget(move_to_position);
    DPRINT("steps_per_sec = ");
    DPRINTLN(steps_per_sec);
    DPRINT("RPM = ");
    DPRINTLN(rpm);
    DPRINT("turns_to_dose = ");
    DPRINTLN(turns_to_dose);
    DPRINT("move_to_position = ");
    DPRINTLN(stepper.getTarget());
    break;
  }
  //******************* Режим УГЛЕВАНИЕ **********************************
  if (regim == CARBONIZE)
  {
    DPRINTLN("Carbonize");
    stepper.setRunMode(KEEP_SPEED);
    start_pressed = 0;
    Timer2.enableISR();
    delay(20);
    startMotor();
    carbonize_running = 1;
    //******************* Режим КАЛИБРОВКА *********************************
  }
  else if (regim == CALIBRATE)
  {
    DPRINTLN("Calibrate");
    start_pressed = 0;
    stepper.setRunMode(FOLLOW_POS);
    stepper.setMaxSpeed(STEPS_PER_TURN * 2);
    move_to_position = (long)STEPS_PER_TURN * (long)TURNS;
    stepper.setCurrent(0);
    stepper.setTarget(move_to_position);
    DPRINT("steps_per_sec = ");
    DPRINTLN(steps_per_sec);
    DPRINT("RPM = ");
    DPRINTLN(rpm);
    DPRINT("move_to_position = ");
    DPRINTLN(stepper.getTarget());
  }
}
//******************************************************************************
void stopButtonHandle()
{
  if (stop_button.isClick())
  {
    (smooth_start == true) ? stepper.stop() : stepper.brake();
    smooth_start = false;
    carbonize_running = 0;
    start_pressed = 0;
    stop_pressed = 1;
  }
  dynamicDataOnLcd();
}
//******************************************************************************
void blinkLcd(byte count, bool change_TURNS)
{
  while (count)
  {
    if (change_TURNS == 0)
    {
      lcd.setCursor(10, 1);
      lcd.print("  ");
      delay(400);
      (ml_per_Nturns < 10) ? lcd.setCursor(11, 1) : lcd.setCursor(10, 1);
      lcd.print(ml_per_Nturns, 0);
      if (ml_per_Nturns < 10)
      {
        lcd.setCursor(10, 1);
        lcd.print(F("0"));
      }
      delay(400);
      --count;
    }
    else
    {
      lcd.setCursor(0, 1);
      lcd.print("  ");
      delay(400);
      lcd.setCursor(0, 1);
      lcd.print(TURNS);
      delay(400);
      --count;
    }
  }
}
//********************************************************************************
bool countdownTime()
{
  static uint32_t currTime = millis();
  if (millis() - currTime >= ONE_SECOND && (secundes > 0 && Minutes >= 0))
  {
    --secundes;
    currTime = millis();
  }
  if (secundes == 0 && Minutes > 0)
  {
    --Minutes;
    secundes = 60;
  }
  if (Minutes == 0 && secundes == 0)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}
//********************************************************************************
void startMotor()
{
  if (rpm > RPM_TO_SMOOTH)
  {
    stepper.setSpeed(steps_per_sec, SMOOTH);
    smooth_start = true;
  }
  else
  {
    stepper.setSpeed(steps_per_sec);
    smooth_start = false;
  }
}
//*****************ЧТЕНИЕ ПОКАЗАНИЙ ЭНКОДЕРА ПРИ ЕГО ВРАЩЕНИИ********************
int8_t readEncoder()
{
  static int8_t rot_enc_table[] = {0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0};
  static uint8_t prevNextCode = 0;
  static uint16_t store = 0;
  prevNextCode <<= 2;
  if (pinRead(MINUS_PIN))
  {
    prevNextCode |= 0x02;
  }
  if (pinRead(PLUS_PIN))
  {
    prevNextCode |= 0x01;
  }
  prevNextCode &= 0x0f;
  // If valid then store as 16 bit data.
  if (rot_enc_table[prevNextCode])
  {
    store <<= 4;
    store |= prevNextCode;
    if (store == 0xd42b)
    {
      return -1;
    }
    if (store == 0xe817)
    {
      return 1;
    }
    //if ((store & 0xff) == 0x2b) return -1;
    //if ((store & 0xff) == 0x17) return 1;
  }
  return 0;
}

//***************** Преобразование целого числа в строку ***************************
void intToString(int x, char *str)
{
  int i = 0;
  x = abs(x);
  do
  {
    str[i++] = (x % 10) + '0';
    x = x / 10;
  } while (x);
  invertString(str, i);
  str[i] = '\0';
}
//********* Добавлено: минимальное количество символов в строке = 'd' *************
void intToString(int x, char *str, int d)
{
  int i = 0;
  x = abs(x);
  do
  {
    str[i++] = (x % 10) + '0';
    x = x / 10;
  } while (x);
  // Если количество цифр нужно больше,
  // то перед числом добавляются нули.
  while (i < d)
  {
    str[i++] = '0';
  }
  invertString(str, i);
  str[i] = '\0';
}
//****************************************************
void invertString(char *str, int len)
{
  int i = 0, j = len - 1, temp;
  while (i < j)
  {
    temp = str[i];
    str[i] = str[j];
    str[j] = temp;
    ++i;
    --j;
  }
}
