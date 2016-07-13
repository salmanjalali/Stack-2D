#include <LPC17xx.h>
#include <RTL.h>
#include <stdio.h>
#include <stdbool.h>
#include "glcd.h"
#include "INT0.h"
#include "ADC.h"
#include "LED.h"

// Constants
#define MAX_X 240
#define MAX_Y 320
#define HEIGHT 20
#define INITIAL_X 0 
#define INITIAL_Y 300
#define INITIAL_WIDTH 100
#define JUMPS 1
#define WATCHDOG_TIME 19 << 17
#define WATCHDOG_LED 19 << 12
#define ERASE_TIME 19 << 12

// Global variables
int colors[14] = {Black, Navy, DarkGreen, DarkCyan, Maroon, Purple, Olive, DarkGrey, Blue, Green, Cyan, Red, Magenta, Yellow};
volatile double time = 0, watchdog_timer = 0;
extern volatile int random;
extern volatile uint8_t AD_done;
extern volatile uint16_t AD_last;
OS_SEM next_block, change_difficulty, watchdog_sem;
volatile int difficulty;

int x_offsets[15];
int widths[15];
int count = 0, diff;

// Block characteristics
typedef struct block_info {
	int x_offset, y_offset, width, color;
} block_info_t;

volatile block_info_t current_block;

//reset the game
__inline void reset(){
		memset(x_offsets, 0, sizeof(x_offsets));
		memset(widths, 0, sizeof(widths));
		count = 0;
		current_block.y_offset = INITIAL_Y;
		current_block.width = INITIAL_WIDTH;
		GLCD_Clear(White);
}

__inline void destroy_right(){
	double ticks;
	GLCD_SetTextColor(LightGrey);
	GLCD_Bargraph (x_offsets[count - 1] + widths[count - 1], current_block.y_offset, diff, HEIGHT, 1024);
	
	ticks = os_time_get();
	while(os_time_get() - ticks < ERASE_TIME);
	
	GLCD_SetTextColor(White);
	GLCD_Bargraph (x_offsets[count - 1] + widths[count - 1], current_block.y_offset, diff, HEIGHT, 1024);
}

__inline void destroy_left(){
	double ticks;
	GLCD_SetTextColor(LightGrey);
	GLCD_Bargraph (current_block.x_offset, current_block.y_offset, abs(diff), HEIGHT, 1024);
				
	ticks = os_time_get();
	while(os_time_get() - ticks < ERASE_TIME);

	GLCD_SetTextColor(White);
	GLCD_Bargraph (current_block.x_offset, current_block.y_offset, abs(diff), HEIGHT, 1024);
}

__task void stop_block(void *void_ptr) {	
	while(1) {
		os_sem_wait(&next_block, 0xffff);
		
		watchdog_timer = os_time_get();
		
		x_offsets[count] = current_block.x_offset;
		if(count != 0){
			diff = x_offsets[count] - x_offsets[count - 1];
			// If the block completely missed the stack, game over
			if (abs(diff) >= current_block.width - 1 ){
				GLCD_DisplayString(1, 1, 1, "  GAME OVER ");

				os_sem_wait(&next_block, 0xffff);
				reset();
				continue;
			}
			// If the block extends to the right of the stack
			else if(diff < 0){
				destroy_left();
				
				x_offsets[count] -= diff;
				current_block.width += diff;
			}
			// If the block extends to the left of the stack
			else if (diff > 0){
				destroy_right();
				
				current_block.width -= diff;
			}
			// If the block is perfectly stacked, do nothing
		}
		current_block.y_offset -= HEIGHT;
		current_block.x_offset = INITIAL_X;
		current_block.color++;
		widths[count] = current_block.width;
		
		count++;
	}
}	

__task void set_difficulty(void *void_ptr){
	uint32_t ad_avg = 0;
  uint16_t ad_val = 0, ad_val_ = 0xFFFF;
	
	while (1) {                                /* Loop forever                  */
		os_sem_wait(&change_difficulty, 0xffff);
		
		ad_avg += AD_last << 8;                /* Add AD value to averaging     */
		ad_avg ++;
		if ((ad_avg & 0xFF) == 0x10) {         /* average over 16 values        */
			ad_val = (ad_avg >> 8) >> 4;         /* average devided by 16         */
			ad_avg = 0;
		}

    if (ad_val ^ ad_val_) {                  /* AD value changed              */
      ad_val_ = ad_val;
		}
    difficulty = ad_val;
	}
}

__task void watch_dog(void *void_ptr){
	int i;
	double ticks;
	
	while(1){
		// Turn on all the lights initially
		LED_Out(255);
		
		os_sem_wait(&watchdog_sem, 0xffff);
		
		GLCD_DisplayString(5, 1, 1, " Press Button");
		
		for (i = 0; i <= 8; i++){
			LED_Out(255>>i);
			
			ticks = os_time_get();
			while(os_time_get() - ticks < (WATCHDOG_LED) ){
				if(random > 0)
				{
					random = 0;
					GLCD_DisplayString(5, 1, 1, "             ");
					os_tsk_delete_self();
				}
			}
		}
		
		GLCD_DisplayString(5, 1, 1, "   Timed Out     ");
		while(random < 1);
		random = 0;
		reset();
		os_tsk_delete_self();
	}
}

__task void base_task(void) {
	bool moving_right;
	
	// Set the priority for the base task to lowest
	os_tsk_prio_self( 1 );
	
	// Initialize the semaphores
	os_sem_init(&next_block, 0);
	os_sem_init(&change_difficulty, 0);
	os_sem_init(&watchdog_sem, 0);
	
	current_block.x_offset = INITIAL_X;	
	current_block.y_offset = INITIAL_Y;
	current_block.width = INITIAL_WIDTH;
	current_block.color = 0;
	moving_right = 1;
	
	GLCD_Init();
	GLCD_Clear(White);
	
	os_tsk_create_ex(stop_block, 2, 0);
	os_tsk_create_ex(set_difficulty, 2, 0);
	os_tsk_create_ex(watch_dog, 3, 0);
	
	while(1) {
		ADC_StartCnv();
		GLCD_SetTextColor(White);
		if(moving_right) {
			GLCD_Bargraph (current_block.x_offset, current_block.y_offset, JUMPS, HEIGHT, 1024);
			current_block.x_offset += JUMPS;
		}
		else {
			GLCD_Bargraph (current_block.x_offset + current_block.width - JUMPS, current_block.y_offset, JUMPS, HEIGHT, 1024);
			current_block.x_offset -= JUMPS;
		}
		
		if(current_block.x_offset + current_block.width >= MAX_X) {
			moving_right = 0;
			current_block.x_offset = MAX_X - current_block.width;
		}
		else if(current_block.x_offset <= 0) {
			moving_right = 1;
			current_block.x_offset = 0;
		}
		
		if (current_block.width < 10)
			while(os_time_get() - time < difficulty);
		else
			while(os_time_get() - time < difficulty);
		
		GLCD_SetTextColor(colors[current_block.color]);
		GLCD_Bargraph (current_block.x_offset, current_block.y_offset, current_block.width, HEIGHT, 1024);

		time = os_time_get();
		
		// If the button is pressed, fire the appropriate interrupt and call the task
		if(random > 0){
			os_sem_send(&next_block);
			random = 0;
		}
		
		// If the potentiometer is moved, fire the appropriate interrupt and call the task
		if(AD_done == 1){
			AD_done = 0;
			os_sem_send(&change_difficulty);
		}
		
		if(os_time_get() - watchdog_timer > WATCHDOG_TIME) {
			os_sem_send(&watchdog_sem);
			os_tsk_create_ex(watch_dog, 3, 0);
			watchdog_timer = os_time_get();
		}
	}
}

int main( void ) {
	// Initialization to get the system clock
	SystemInit();
	SystemCoreClockUpdate();
	
	// Initialize interrupts
	INT0_Init();
	ADC_Init(); 
  LED_Init();
	
	// Call the base task to draw the moving block
	os_sys_init( base_task );
	
	while ( 1 ) {
		// Endless loop
	}
}
