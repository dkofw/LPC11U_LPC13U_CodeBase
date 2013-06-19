/**************************************************************************/
/*!
    @file     sensorpoll.c
    @author   K. Townsend (microBuilder.eu)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2013, K. Townsend (microBuilder.eu)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**************************************************************************/
#include "sensorpoll.h"

volatile uint32_t sensorpoll_counter = 0;
volatile uint32_t sensorpoll_capture[4] = {0,0,0,0};

/**************************************************************************/
/*!
    @brief Interrupt handler for 16-bit timer 1

    @desc  This interrupt handler can be used to schedule sensor reads at
           a specific interval.  By default, the timer is setup for a 5 ms
           delay, which should give enough time to read more slow I2C
           sensors safely, but care should always be taken when doing any
           extended task inside an interrupt handler.

           By default, this interrupt handler will be set to the lowest
           priority to avoid any problems with other interrupt-based
           functions like 'delay' which may be used by the sensor
           drivers!

           This code is not *safe* and shouldn't be used carelessly, but
           when properly implemented can provide regularly scheduled
           sensor data without the need for a complex RTOS and task
           scheduler.

    @note  Use this interupt handler with care and caution!
*/
/**************************************************************************/
#if defined CFG_MCU_FAMILY_LPC11UXX
void TIMER16_1_IRQHandler(void)
#elif defined CFG_MCU_FAMILY_LPC13UXX
void CT16B1_IRQHandler(void)
#else
  #error "sensorpoll.c: No MCU defined"
#endif
{
  /* Handle match event (only MAT0 is handled here!) */
  if (LPC_CT16B1->IR & (0x01 << 0))
  {
    LPC_CT16B1->IR = 0x1 << 0;
    sensorpoll_counter++;
    /* ToDo: capture your sensor data here, or setup a callback that
     * will capture the data for you, keeping in mind the 5ms limit */
  }

  /* Handle capture events, which you might want to use to capture
   * sensor data only when a specific CAP event/pin is triggered */
  if (LPC_CT16B1->IR & (0x1 << 4))
  {
    LPC_CT16B1->IR = 0x1 << 4;
    sensorpoll_capture[0]++;
  }
  if (LPC_CT16B1->IR & (0x1 << 5))
  {
    LPC_CT16B1->IR = 0x1 << 5;
    sensorpoll_capture[1]++;
  }
  if (LPC_CT16B1->IR & (0x1 << 6))
  {
    LPC_CT16B1->IR = 0x1 << 6;
    sensorpoll_capture[2]++;
  }
  if (LPC_CT16B1->IR & (0x1 << 7))
  {
    LPC_CT16B1->IR = 0x1 << 7;
    sensorpoll_capture[3]++;
  }

  return;
}

/**************************************************************************/
/*!
    @brief Initialises the timer with a default 5ms tick rate

    @code
    extern volatile uint32_t sensorpoll_counter = 0;

    // Initialise the sensor poller (setup the timer, etc.)
    sensorpollInit();

    // Enable the timer (start polling for data!)
    sensorpollEnable();

    while(1)
    {
      // If everything is setup properly this should increment by ~200
      // every second (sensorpoll.c has a 5 ms default tick rate)
      printf("%d\r\n", sensorpoll_counter);
      delay(1000);
    }
    @endcode
*/
/**************************************************************************/
void sensorpollInit()
{
  uint32_t i;

  /* Initialise 16-bit timer 1 */
  LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 8);

  /* Reset the counter variables */
  sensorpoll_counter = 0;
  for ( i = 0; i < 4; i++ )
  {
    sensorpoll_capture[i] = 0;
  }

  LPC_CT16B1->TCR  = 0x02;            /* Reset the timer               */
  LPC_CT16B1->PR   = 0x07;            /* Set prescaler to eight        */
  LPC_CT16B1->IR   = 0xff;            /* Reset all interrrupts         */
  LPC_CT16B1->PWMC = 0x00;            /* Disable PWM mode              */
  LPC_CT16B1->MR0  = (SystemCoreClock / 200) >> 3; /* 5 ms w/prescalar */
  LPC_CT16B1->MCR  = (0x3<<0);        /* Interrupt and Reset on MR0    */

  /* Set this IRQ to lowest priority to avoid problems with delay.c    */
  #if defined CFG_MCU_FAMILY_LPC11UXX
    NVIC_SetPriority(TIMER_16_1_IRQn, (1<<__NVIC_PRIO_BITS) - 1);
  #elif defined CFG_MCU_FAMILY_LPC13UXX
    NVIC_SetPriority(CT16B1_IRQn, (1<<__NVIC_PRIO_BITS) - 1);
  #endif

  /* Enable the IRQ */
  #if defined CFG_MCU_FAMILY_LPC11UXX
    NVIC_EnableIRQ(TIMER_16_1_IRQn);
  #elif defined CFG_MCU_FAMILY_LPC13UXX
    NVIC_EnableIRQ(CT16B1_IRQn);
  #endif

  return;
}

/**************************************************************************/
/*!
    @brief Enables the timer
*/
/**************************************************************************/
void sensorpollEnable(void)
{
  LPC_CT16B1->TCR = 1;
  return;
}

/**************************************************************************/
/*!
    @brief Disables the timer
*/
/**************************************************************************/
void sensorpollDisable(void)
{
  LPC_CT16B1->TCR = 0;
  return;
}

/**************************************************************************/
/*!
    @brief Configures the match register for the timer, which will set
           the 'delay' between each tick on the timer.

    @param[in]  value
                The value to assign to the match register

    @code
    // Set the timer to trigger every 12000 ticks, which is equal to 96000
    // ticks of the system clock since the timer is set to run at 1/8 the
    // speed of the system clock.
    sensorpollSetMatch(12000);
    @endcode

    @note Note that by default the 16-bit timer is setup with a prescalar
          value of 8, meaning that the timer clock is 1/8 the speed of the
          system clock.  For example, a 48MHz system clock = a 6MHz
          peripheral clock.  Each 'tick' for the match register is based
          on this 1/8 peripheral clock, not the faster system clock.
*/
/**************************************************************************/
void sensorpollSetMatch(uint16_t value)
{
  LPC_CT16B1->MR0 = value;
  return;
}

/**************************************************************************/
/*!
    @brief Gets the match register value for the timer, which controls
           the 'delay' between each tick on the timer.

    @note Note that by default the 16-bit timer is setup with a prescalar
          value of 8, meaning that the timer clock is 1/8 the speed of the
          system clock.  For example, a 48MHz system clock = a 6MHz
          peripheral clock.  Each 'tick' for the match register is based
          on this 1/8 peripheral clock, not the faster system clock.
*/
/**************************************************************************/
uint16_t sensorpollGetMatch(void)
{
  return LPC_CT16B1->MR0;
}