#include "LPC17xx.H"                         /* LPC17xx definitions           */
#include "ADC.h"
#include "glcd.h"

uint16_t AD_last;                            /* Last converted value          */
uint8_t  AD_done = 0;                        /* AD conversion done flag       */

/*----------------------------------------------------------------------------
  Function that initializes ADC
 *----------------------------------------------------------------------------*/
void ADC_Init (void) {

  LPC_SC->PCONP |= ((1 << 12) | (1 << 15));  /* enable power to ADC & IOCON   */

  LPC_PINCON->PINSEL1  &= ~( 3 << 18);
  LPC_PINCON->PINSEL1  |=  ( 1 << 18);       /* P0.25 is AD0.2                */
  LPC_PINCON->PINMODE1 &= ~( 3 << 18);
  LPC_PINCON->PINMODE1 |=  ( 2 << 18);       /* P0.25 no pull up/down         */

  LPC_ADC->ADCR        =  ( 1 <<  2) |       /* select AD0.2 pin              */
                          ( 4 <<  8) |       /* ADC clock is 25MHz/5          */
                          ( 1 << 21);        /* enable ADC                    */

  LPC_ADC->ADINTEN     =  ( 1 <<  8);        /* global enable interrupt       */

  NVIC_EnableIRQ(ADC_IRQn);                  /* enable ADC Interrupt          */
}


/*----------------------------------------------------------------------------
  start AD Conversion
 *----------------------------------------------------------------------------*/
void ADC_StartCnv (void) {
  LPC_ADC->ADCR &= ~( 7 << 24);              /* stop conversion               */
  LPC_ADC->ADCR |=  ( 1 << 24);              /* start conversion              */
}


/*----------------------------------------------------------------------------
  stop AD Conversion
 *----------------------------------------------------------------------------*/
void ADC_StopCnv (void) {

  LPC_ADC->ADCR &= ~( 7 << 24);              /* stop conversion               */
}


/*----------------------------------------------------------------------------
  get converted AD value
 *----------------------------------------------------------------------------*/
uint16_t ADC_GetCnv (void) {

  while (!(LPC_ADC->ADGDR & ( 1UL << 31)));  /* Wait for Conversion end       */
  AD_last = (LPC_ADC->ADGDR >> 4) & ADC_VALUE_MAX; /* Store converted value   */

  return(AD_last);
}


/*----------------------------------------------------------------------------
  A/D IRQ: Executed when A/D Conversion is done
 *----------------------------------------------------------------------------*/
void ADC_IRQHandler(void) {
  volatile uint32_t adstat;

  adstat = LPC_ADC->ADSTAT;		             /* Read ADC clears interrupt     */

  AD_last = (LPC_ADC->ADGDR >> 4) & ADC_VALUE_MAX; /* Store converted value   */

  AD_done = 1;
}

