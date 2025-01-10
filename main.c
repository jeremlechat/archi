#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "minirisc.h"
#include "uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "harvey_platform.h"
#include "xprintf.h"
#include <math.h>
#include "semphr.h"
#include <pthread.h>           /* produit.c */
#include "font.h"
#include <time.h>

volatile int mouse_x = 0;
volatile int mouse_y = 0;
volatile int deplace_x_mouse = 0;
volatile int deplace_y_mouse = 0;
volatile int brush_radius = 3;
volatile int plate_pos_x;   //position gauche du plateau
volatile int plate_pos_y;	//position supérieure du plateau
volatile int plate_height = 10;
volatile int plate_width = 60;
volatile int plate_y_shift = 30;
volatile int dep_x = 10;
volatile int marble_pos_x;
volatile int marble_pos_y;
volatile int marble_radius = 8;
volatile float marble_speed = 5;
volatile double marble_angle = 65*M_PI/180;		//L'angle est en radian
volatile int left_button_is_pressed = 0;
volatile int bric_height = 80;	
volatile int bric_width = 60;
volatile int ecart = 1;
SemaphoreHandle_t ksem;
//volatile pthread_mutex_t mutex;
volatile int nb_bric_changed = 0;
volatile int is_game_over = 1;
volatile int delay = 20;
volatile int is_moving_right = 0;
volatile int is_moving_left = 0;
volatile int long_move_right = 0;
volatile int long_move_left = 0;
volatile int long_press = 0;
volatile double time_pressed = 0;
volatile clock_t start = 0;



typedef struct brique {
	int pos_x;
	int pos_y;
	int is_active;	
} brique;

#define nb_bric 20					//Contient le nombre initial de briques
volatile brique brics[nb_bric];		//Contient la liste de toutes les briques 


void echo_task(void *arg)
{
	(void)arg;
	char buf[128];

	while (1) {
		int i = 0;
		char c;
		while ((c = getchar()) != '\r' && i != (sizeof(buf) - 1)) {
			buf[i++] = c;
		}
		buf[i] = '\0';
		printf("Received: \"%s\"\n", buf);
		if (strcmp(buf, "quit") == 0) {
			minirisc_halt();
		}
	}
}

void angle_task() {
	while(1) {
		printf("L'angle vaut %.2f\n", marble_angle);
		vTaskDelay(MS2TICKS(100));
	}
}

#define SCREEN_WIDTH  600
#define SCREEN_HEIGHT 800

static uint32_t frame_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];
volatile uint32_t color = 0x00ff0000;


void init_video() {
	memset(frame_buffer, 0, sizeof(frame_buffer)); // clear frame buffer to black
	VIDEO->WIDTH  = SCREEN_WIDTH;
	VIDEO->HEIGHT = SCREEN_HEIGHT;
	VIDEO->DMA_ADDR = frame_buffer;
	VIDEO->CR = VIDEO_CR_IE | VIDEO_CR_EN;
	minirisc_enable_interrupt(VIDEO_INTERRUPT);
}

/**
 * Dessine un rectangle dont le coin supérieur gauche est en x/y ou 0/0 sinon
 * @param x int 
 * @param y
 * @param width
 */
void draw_square(int x, int y, int width, int height, uint32_t color) {
	if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) {
		return;
	}
	int i, j;
	int x_start = x < 0 ? 0 : x;
	int y_start = y < 0 ? 0 : y;
	int x_end = x + width;
	int y_end = y + height;
	if (x_end > SCREEN_WIDTH) {
		x_end = SCREEN_WIDTH;
	}
	if (y_end > SCREEN_HEIGHT) {
		y_end = SCREEN_HEIGHT;
	}
	for (j = y_start; j < y_end; j++) {
		for (i = x_start; i < x_end; i++) {
			frame_buffer[j*SCREEN_WIDTH + i] = color;
		}
	}
}



void draw_disk(int x, int y, int diam, uint32_t color) {
	if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) {
		return;
	}
	int i, j;
	int x_start = x < 0 ? 0 : x;
	int y_start = y < 0 ? 0 : y;
	int x_end = x + diam;
	int y_end = y + diam;
	int rad2 = diam*diam/4;
	int xc = x + diam / 2;
	int yc = y + diam / 2;
	if (x_end > SCREEN_WIDTH) {
		x_end = SCREEN_WIDTH;
	}
	if (y_end > SCREEN_HEIGHT) {
		y_end = SCREEN_HEIGHT;
	}
	for (j = y_start; j < y_end; j++) {
		int j2 = (yc - j)*(yc - j);
		for (i = x_start; i < x_end; i++) {
			int i2 = (xc - i)*(xc - i);
			if (j2 + i2 <= rad2) {
				frame_buffer[j*SCREEN_WIDTH + i] = color;
			}
		}
	}
}
/**
 * Rebondie sur une surface horizontale
 * @return la prochaine position en y
 */
int rebondi_horizontal() {	
	//printf("O est dans le rebond angle = %.2f\n", marble_angle);	
	marble_angle = - marble_angle;
	int next_y = marble_pos_y + marble_speed * sin(marble_angle);
	//printf("ON est apres le rebond angle = %.2f\n", marble_angle);
	return next_y;
}



/**
 * Rebondie sur une surface verticale
 * @return la prochaine position en x
 */
int rebondi_vertical() {
	marble_angle = M_PI - marble_angle;
	int next_x = marble_pos_x + marble_speed * cos(marble_angle);
	//printf("cos %.2f = %.2f\n", marble_angle, cos(marble_angle));
	return next_x;
}

void move_marble(){
	while (1) {
		if (!is_game_over) {
			float prochain_x = marble_pos_x + marble_speed * cos(marble_angle);
			float prochain_y = marble_pos_y + marble_speed * sin(marble_angle);
			int next_x = prochain_x;
			int next_y = prochain_y;
			if (next_x < 0 || next_x > SCREEN_WIDTH) {
				next_x = rebondi_vertical();
			}
			if (next_y < 0) {
				next_y = rebondi_horizontal();
			}
			draw_disk(marble_pos_x, marble_pos_y, marble_radius, 0x00000000);
			marble_pos_x = next_x;
			marble_pos_y = next_y;
			draw_disk(marble_pos_x, marble_pos_y, marble_radius, 0x00ff0000);
			vTaskDelay(MS2TICKS(delay));
			//printf("la position en x/y vaut : %d/%d\n", next_x, next_y);	
		}
	}
}

void game_over() {
	is_game_over = 1;
	marble_angle = 0;
	left_button_is_pressed = 0;
	marble_speed = 0;
	marble_pos_x = 0;
	marble_pos_y = 0;
	plate_pos_x = 0;
	plate_pos_y = 0;
	vTaskDelay(MS2TICKS(5));
	draw_square(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,0x0000000000);
}

void collision_plate() {
	int local_ecart = 17;
	while (1) { 	
		if (marble_pos_y > SCREEN_HEIGHT - plate_height - plate_y_shift) {
			game_over();
		}
		if (marble_pos_y > SCREEN_HEIGHT-plate_height-plate_y_shift - local_ecart && marble_pos_y < SCREEN_HEIGHT - plate_y_shift) {
			if (marble_pos_x > plate_pos_x && marble_pos_x < plate_pos_x + plate_width) {
				printf("collision avec le plateau\n");
//				printf("ON est avant le rebond angle = %.2f\n", marble_angle);
				rebondi_horizontal();
				vTaskDelay(MS2TICKS(2*delay));

			}
		}
	}
}

void collision_brics() {
	int marge = 1;
	while (1) { 	
		if (marble_pos_y < SCREEN_HEIGHT-plate_height-2*plate_y_shift) {
			for (int i=0; i < nb_bric; i++) {
				if (brics[i].is_active) {
					//Collision horizontale 
					if (marble_pos_x + marble_radius <= brics[i].pos_x + bric_width && marble_pos_x - marble_radius >= brics[i].pos_x) {
						if ( (marble_pos_y + marble_radius <= brics[i].pos_y + bric_height && marble_pos_y + marble_radius >= brics[i].pos_y + bric_height - marge) 
						|| (marble_pos_y - marble_radius <= brics[i].pos_y  && marble_pos_y - marble_radius>= brics[i].pos_y - marge ) ) {
							rebondi_horizontal();
							printf("collision horizontale avec brique %d\n", i);
							nb_bric_changed ++;
							brics[i].is_active=0;
							draw_square(brics[i].pos_x, brics[i].pos_y, 60 - ecart, 80 - ecart, 0x00000000);
						}
					}
					//Colision verticale
					if (marble_pos_y + marble_radius < brics[i].pos_y + bric_height && marble_pos_y - marble_radius> brics[i].pos_y) {
						if ( (marble_pos_x - marble_radius <= brics[i].pos_x + bric_width && marble_pos_x - marble_radius>= brics[i].pos_x + bric_width - marge)
						 || (marble_pos_x + marble_radius <= brics[i].pos_x && marble_pos_x + marble_radius >= brics[i].pos_x - marge)) {
							rebondi_vertical();
							printf("collision verticale avec brique %d\n", i);
							//printf("marble pos y = %d et marble pos x = %d\n", marble_pos_y, marble_pos_x);
							//printf("brics[i].pos_y + bric_height = %d\n", brics[i].pos_y + bric_height);
							brics[i].is_active=0;
							nb_bric_changed ++;
							draw_square(brics[i].pos_x, brics[i].pos_y, 60 - ecart, 80 - ecart, 0x00000000);
							}
					}
				}
			}
		}
	}
}

void video_interrupt_handler() {
	VIDEO->SR = 0;
}


void mouse_interrupt_handler() {
	mouse_data_t mouse_event;
	while (MOUSE->SR & MOUSE_SR_FIFO_NOT_EMPTY) {
		mouse_event = MOUSE->DATA;
		switch (mouse_event.type) {
			case MOUSE_MOTION:
				if (left_button_is_pressed) {
					deplace_x_mouse = mouse_event.x-mouse_x;
//					mouse_x = mouse_event.x;
//					mouse_y = mouse_event.y;
//					xprintf("x mouse = %d , x event = %d\n", mouse_x,mouse_event.x);
//					xprintf("deplace_x_mouse : %d\n", deplace_x_mouse);
				}
				break;
			case MOUSE_BUTTON_LEFT_DOWN:
				left_button_is_pressed = 1;
				mouse_x = mouse_event.x;
				mouse_y = mouse_event.y;
//				xprintf("left_button_is_pressed : %d\n", left_button_is_pressed);
				break;
			case MOUSE_BUTTON_LEFT_UP:
				left_button_is_pressed = 0;
				break;
		}
	}
}

void init_plate() {
	plate_pos_x = SCREEN_WIDTH/2-plate_width/2;
	plate_pos_y = SCREEN_HEIGHT-plate_height-plate_y_shift;
	draw_square(plate_pos_x, plate_pos_y, plate_width, plate_height, 0x00ffffff);
	printf("position plate %d;%d -> %d;%d\n", plate_pos_x,plate_pos_y, plate_pos_x+plate_width, plate_pos_y+plate_height);


}

void init_marble() {
	printf("angle inital = %.2f\n", marble_angle);
	draw_disk(plate_pos_x + plate_width/2 - marble_radius/2, plate_pos_y - 300, marble_radius, 0x00ff0000);
	marble_pos_x = plate_pos_x + plate_width/2 - marble_radius/2;
	marble_pos_y = plate_pos_y - 300;
}

void move_plate() {
	int facteur = 20;
	while(1) {
		if (!is_game_over) {
			if (long_move_left && long_press == 0) {
				start = clock();
				long_press = 1;
			}
			if (long_move_left && long_press == 0) {
				start = clock();
				long_press = 1;
			}

			double time = (double)(clock() - start)/CLOCKS_PER_SEC;
			printf("Temps de presse = %.2f\n" , time);
			printf("le plateau est en %d\n", plate_pos_x);
			if (long_move_right) {
				printf("appuie long droite ::::::::::::::::::::: \n");
				draw_square(plate_pos_x, plate_pos_y, plate_width, plate_height, 0x00000000);
				plate_pos_x = (int)(( 1 + time/3) * facteur) + plate_pos_x;
				if (plate_pos_x+plate_width>SCREEN_WIDTH) {
					plate_pos_x = SCREEN_WIDTH - plate_width;
				}
				draw_square(plate_pos_x, plate_pos_y, plate_width, plate_height, 0x00ffffff);
				long_move_right = 0;
			} else if (long_move_left) {
				printf("appuie long gache ::::::::::::::::::::: \n");
				draw_square(plate_pos_x, plate_pos_y, plate_width, plate_height, 0x00000000);
				plate_pos_x = - (int)(( 1 + time/3) * facteur) + plate_pos_x;
				if (plate_pos_x < 0) {
					plate_pos_x = 0;
				}
				draw_square(plate_pos_x, plate_pos_y, plate_width, plate_height, 0x00ffffff);
				long_move_left = 0;
			} else {
				printf("on annule le t e m p s   d e   p r e s s e    ! ! !  ! !  ! ! ! !");
				long_press = 0;
			}
			if (is_moving_right) {
				draw_square(plate_pos_x, plate_pos_y, plate_width, plate_height, 0x00000000);
				plate_pos_x = facteur + plate_pos_x;
				if (plate_pos_x+plate_width>SCREEN_WIDTH) {
					plate_pos_x = SCREEN_WIDTH - plate_width;
				}
				draw_square(plate_pos_x, plate_pos_y, plate_width, plate_height, 0x00ffffff);
				is_moving_right = 0;
			} else if (is_moving_left) {
				draw_square(plate_pos_x, plate_pos_y, plate_width, plate_height, 0x00000000);
				plate_pos_x = -facteur + plate_pos_x;
				if (plate_pos_x < 0) {
					plate_pos_x = 0;
				}
				draw_square(plate_pos_x, plate_pos_y, plate_width, plate_height, 0x00ffffff);
				is_moving_left = 0;
			}

			printf("le plateau est désormais en %d\n", plate_pos_x);

//Gere la double touche 
		}
		vTaskDelay(MS2TICKS(delay));
	}
}


void move_plate1() {
	while(1) {
		if (!is_game_over) {
	//		xSemaphoreTake(ksem, portMAX_DELAY);
	//		printf("move_plate\n");
			if (left_button_is_pressed == 1) {
	//			printf("left is pressed\n");
	//			printf("mouse x = %d et y = %d \n", mouse_x, mouse_y);
	//			printf("depalce x = %d\n", deplace_x_mouse);
				draw_square(plate_pos_x, plate_pos_y, deplace_x_mouse, plate_height, 0x00000000);
				if (plate_pos_x <= mouse_x && mouse_x <= plate_pos_x+plate_width && plate_pos_y <= mouse_y && mouse_y <= plate_pos_y+plate_height) {
					printf("On a cliqué sur le plateau\n");
					if (plate_pos_x+plate_width+deplace_x_mouse>SCREEN_WIDTH) {			//Dans le cas ou on sort de l'écran à droite
						plate_pos_x=SCREEN_WIDTH-plate_width;
					} else {
						plate_pos_x += deplace_x_mouse;
					}
					if (plate_pos_x-deplace_x_mouse<0) {								//Dans le cas ou on sort de l'écran à gauche
						plate_pos_x=0;
					} else {
						plate_pos_x += deplace_x_mouse;
					}
				printf("le plateau a bougé de %d\n", deplace_x_mouse);
				}
			} 
			draw_square(plate_pos_x, plate_pos_y, plate_width, plate_height, 0x00ffffff);
		}
	}
}

void keyboard_interrupt_handler() {
	uint32_t kdata;
	while (KEYBOARD->SR & KEYBOARD_SR_FIFO_NOT_EMPTY) {
		kdata = KEYBOARD->DATA;
		if (kdata & KEYBOARD_DATA_PRESSED) {
			xprintf("key code: %d\n", KEYBOARD_KEY_CODE(kdata));
			switch (KEYBOARD_KEY_CODE(kdata)) {
				case 113: // Q
					minirisc_halt();
					break;
				case 32: // space
					memset(frame_buffer, 0, sizeof(frame_buffer)); // clear frame buffer
					break;
				case 79 :
					is_moving_right = 1;
					break;
				case 80 :
					is_moving_left = 1;
					break;
				default:
//					xSemaphoreGiveFromISR(ksem, NULL);
			}
		} 
		if (kdata & KEYBOARD_DATA_REPEAT) {
			switch (KEYBOARD_KEY_CODE(kdata)) {
			case 79 :
				long_move_right = 1;
				break;
			case 80 :
				long_move_left = 1;
			default:
				break;
			}
		} 
	}
}


void mur() {
	init_video();
	MOUSE->CR |= MOUSE_CR_IE;
	KEYBOARD->CR |= KEYBOARD_CR_IE;

	minirisc_enable_interrupt(VIDEO_INTERRUPT | MOUSE_INTERRUPT | KEYBOARD_INTERRUPT);

	minirisc_enable_global_interrupts();
	


	for (int i = 0; i < 10; i++) {
		for (int j = 0; j < 2; j++) {
			brics[i+10*j].pos_x = i * bric_width;
			brics[i+10*j].pos_y = j * bric_height;
			brics[i+10*j].is_active = 1;
			draw_square(i * bric_width, j * bric_height, bric_width - ecart, bric_height - ecart, 0x000000ee);
		}
	}
}

int casse_brique() {
	init_video();

	MOUSE->CR |= MOUSE_CR_IE;
	KEYBOARD->CR |= KEYBOARD_CR_IE;

	minirisc_enable_interrupt(VIDEO_INTERRUPT | MOUSE_INTERRUPT | KEYBOARD_INTERRUPT);

	minirisc_enable_global_interrupts();
	


	for (int i =10; i < SCREEN_WIDTH; i+=100) {
		for (int j = 10; j < SCREEN_HEIGHT-200; j+=100) {
			draw_square(i, j, 60, 80, 0x000000ee);
		}	
	}
	return 0;
}

void init_game(int mode) {
	is_game_over = 0;
	init_uart();
	init_video();
	if (mode == 0) {
		mur();
	}


	init_plate();
	init_marble();


}

int main() {


	ksem = xSemaphoreCreateBinary();
    if (ksem == NULL) {
        // Handle error
        printf("Failed to create mutex\n");
    }
	init_game(0);
//	printf("cos 0 = %.2f\n", cos(0));
//	printf("cos 45 = %.2F\n", cos(45));
//	printf("cos 90 = %.2F\n", cos(90));
//	printf("cos 135 = %.2F\n", cos(1350));
//	printf("cos 180 = %.2F\n", cos(180));
//	printf("cos -45 = %.2F\n", cos(-45));
//	printf("cos pi = %.2F\n", cos(M_PI));
//	printf("cos pi/2 = %.2F\n", cos(M_PI/2));

//crer sémaphore

	//xTaskCreate(angle_task,"angle",1024,NULL,1,NULL);

	xTaskCreate(collision_plate, "collision_plate", 8192, NULL, 1, NULL);
	xTaskCreate(collision_brics, "collision_brics", 8192, NULL, 1, NULL);
	xTaskCreate(move_marble,  "move",  8192, NULL, 1, NULL);
	xTaskCreate(move_plate, "moveplate", 8192, NULL, 1, NULL);
	vTaskStartScheduler();

	return 0;
}

