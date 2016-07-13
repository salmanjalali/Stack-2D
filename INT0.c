#include <LPC17xx.h>
#include "glcd.h"
#include "INT0.h"
#include <stdio.h>

int random = 0;

void INT0_Init(void){
	LPC_PINCON->PINSEL4 &= ~(3<<20);
	LPC_GPIO2->FIODIR &= ~(1<<10);
	LPC_GPIOINT->IO2IntEnF |= (1<<10);
	
	NVIC_EnableIRQ(EINT3_IRQn);
}

void EINT3_IRQHandler(void) {
	random++;
	LPC_GPIOINT->IO2IntClr |=(1<<10);
}
