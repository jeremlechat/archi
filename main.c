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


volatile int mouse_x = 0;          // Position X actuelle de la souris
volatile int mouse_y = 0;          // Position Y actuelle de la souris
volatile int deplace_x_mouse = 0;  // Déplacement horizontal de la souris
volatile int left_button_is_pressed = 0;  // Indicateur d'appui sur le bouton gauche

volatile int plate_pos_x;          // Position X du plateau (gauche)
volatile int plate_pos_y;          // Position Y du plateau (haut)
volatile int plate_height = 10;    // Hauteur du plateau
volatile int plate_width = 60;     // Largeur du plateau
volatile int plate_y_shift = 30;   // Décalage vertical du plateau par rapport au bas de l'écran

volatile int marble_pos_x;         // Position X de la bille
volatile int marble_pos_y;         // Position Y de la bille
volatile int marble_radius = 8;    // Rayon de la bille
volatile float marble_speed = 4;   // Vitesse de la bille
volatile double marble_angle = 65 * M_PI / 180; // Angle initial de la bille (en radians)

volatile int bric_height = 80;     // Hauteur d'une brique
volatile int bric_width = 60;      // Largeur d'une brique
volatile int ecart = 1;            // Écart entre les briques
volatile int nb_bric_changed = 0;  // Nombre de briques détruites

typedef struct brique {
    int pos_x;                     // Position X de la brique
    int pos_y;                     // Position Y de la brique
    int is_active;                 // Statut actif/inactif de la brique
} brique;

#define nb_bric 20                 // Nombre total de briques
volatile brique brics[nb_bric];    // Tableau contenant les briques

volatile int is_game_over = 1;     // Indicateur de fin de jeu
volatile int delay = 20;           // Délai entre les itérations des tâches
volatile clock_t start = 0;        // Temps de départ pour certains calculs
volatile int collision = -1;       // Type de collision (-1 = aucune, 0 = horizontale, 1 = verticale)

volatile int is_moving_right = 0;  // Déplacement court vers la droite
volatile int is_moving_left = 0;   // Déplacement court vers la gauche
volatile int long_move_right = 0;  // Déplacement long vers la droite
volatile int long_move_left = 0;   // Déplacement long vers la gauche
volatile int long_press = 0;       // Indicateur d'appui prolongé
volatile int has_move_right = 0;   // Compteur de mouvement vers la droite
volatile int has_move_left = 0;    // Compteur de mouvement vers la gauche

volatile int marble_right;         // Bord droit de la bille
volatile int marble_left;          // Bord gauche de la bille
volatile int marble_top;           // Bord supérieur de la bille
volatile int marble_bottom;        // Bord inférieur de la bille


// Clamp angle between 0 and 2π
double clamp_angle(double angle) {
    while (angle < 0) angle += 2 * M_PI;
    while (angle >= 2 * M_PI) angle -= 2 * M_PI;
    return angle;
}



#define SCREEN_WIDTH  600
#define SCREEN_HEIGHT 800

static uint32_t frame_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];


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
	marble_angle = - marble_angle;
    marble_angle = clamp_angle(marble_angle);
    return marble_pos_y + (int)(marble_speed * sin(marble_angle));
}



/**
 * Rebondie sur une surface verticale
 * @return la prochaine position en x
 */
int rebondi_vertical() {
    marble_angle = M_PI - marble_angle;
    marble_angle = clamp_angle(marble_angle);
    return marble_pos_x + (int)(marble_speed * cos(marble_angle));
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
	draw_square(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,0x00aa33ee);
}

void collision_plate() {
	int local_ecart = 14;
    double max_angle = M_PI / 3; // Angle maximal de déviation (60 degrés)
    double small_shift = M_PI / 60;  // Petit décalage (3 degrés)
    double large_shift = M_PI / 20;  // Grand décalage (9 degrés)

	while (1) { 	
		if (marble_pos_y > SCREEN_HEIGHT - plate_height - plate_y_shift) {
			game_over();
		}
		if (marble_pos_y > SCREEN_HEIGHT-plate_height-plate_y_shift - local_ecart && marble_pos_y < SCREEN_HEIGHT - plate_y_shift) {
			if (marble_pos_x > plate_pos_x && marble_pos_x < plate_pos_x + plate_width) {
				if (long_move_right) {
					marble_angle += large_shift;
					//printf("long move irfgt \n");
				} else if (long_move_left) {
					//printf("long move left \n");
					marble_angle -= large_shift;
				} else if (has_move_right > 0) {
					//printf(" move irfgt \n");
					marble_angle += small_shift;
				} else if (has_move_left > 0) {
					//printf("move left \n");
					marble_angle -= small_shift;
				}
				

				rebondi_horizontal();
				//printf("long_move_left %d, long_move_right %d, has left %d, has right %d \n", long_move_left, long_move_right, has_move_left, has_move_right);
				vTaskDelay(MS2TICKS(2*delay));
			}
		}

	}
}

int collision_brics(int next_x,int next_y) {
	int marge = 1;
	marble_right = next_x + marble_radius;
	marble_left = next_x - marble_radius;
	marble_top = next_y - marble_radius;
	marble_bottom = next_y + marble_radius;
		// clock_t debut = clock();
	if (next_y < SCREEN_HEIGHT-plate_height-2*plate_y_shift) {
		for (int i=nb_bric - 1; i > -1; i--) {
			if (brics[i].is_active) {
                int bric_right = brics[i].pos_x + bric_width;
                int bric_left = brics[i].pos_x;
                int bric_top = brics[i].pos_y;
                int bric_bottom = brics[i].pos_y + bric_height;

                // Collision horizontale (bille touchant le haut ou le bas de la brique)
                if (marble_right > bric_left && marble_left < bric_right) {
                    if ((marble_bottom >= bric_top && marble_bottom <= bric_top + marge)   // Bas de la bille touchant le haut de la brique
                        || (marble_top <= bric_bottom && marble_top >= bric_bottom - marge)) { // Haut de la bille touchant le bas de la brique
                        collision = 0;  // Collision horizontale
                        return i;
                    }
                }

                // Collision verticale (bille touchant les côtés de la brique)
                if (marble_bottom > bric_top && marble_top < bric_bottom) {
                    if ((marble_left <= bric_right && marble_left >= bric_right - marge)   // Gauche de la bille touchant le côté droit de la brique
                        || (marble_right >= bric_left && marble_right <= bric_left + marge)) { // Droite de la bille touchant le côté gauche de la brique
                        collision = 1;  // Collision verticale
                        return i;
                    }
                }
			}
		}
	}
	return(-1);
}

void erase (int num_bric) {
	brics[num_bric].is_active=0;
	nb_bric_changed ++;
	draw_square(brics[num_bric].pos_x, brics[num_bric].pos_y, bric_width - ecart,bric_height - ecart, 0x00000000);
}

void move_marble(){
	while (1) {
		if (!is_game_over) {
			float prochain_x = marble_pos_x + marble_speed * cos(marble_angle);
			float prochain_y = marble_pos_y + marble_speed * sin(marble_angle);
			int next_x = prochain_x;
			int next_y = prochain_y;
			if (next_x - marble_radius < 0 || next_x + marble_radius> SCREEN_WIDTH) {
				next_x = rebondi_vertical();
			}
			if (next_y < 0) {
				next_y = rebondi_horizontal();
			}
			int retour = collision_brics(next_x,next_y);
			int col = collision;
			if (retour == - 1) {
				draw_disk(marble_pos_x, marble_pos_y, marble_radius, 0x00000000);
				marble_pos_x = next_x;
				marble_pos_y = next_y;
				draw_disk(marble_pos_x, marble_pos_y, marble_radius, 0x00ff0000);
			} else if (retour < nb_bric) {
				if (col == 0) {
					erase(retour);
					rebondi_horizontal();
					collision = -1;
				} else if (col == 1 ) {
					erase(retour);
					rebondi_vertical();
					collision = -1;
				}
			} else {
				xprintf("Il y a eu un probleme de collision");
			}
			vTaskDelay(MS2TICKS(delay));
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
				}
				break;
			case MOUSE_BUTTON_LEFT_DOWN:
				left_button_is_pressed = 1;
				mouse_x = mouse_event.x;
				mouse_y = mouse_event.y;
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
			if (long_move_right) {
				draw_square(plate_pos_x, plate_pos_y, plate_width, plate_height, 0x00000000);
				plate_pos_x = (int)(( 1 + time/3) * facteur) + plate_pos_x;
				if (plate_pos_x+plate_width>SCREEN_WIDTH) {
					plate_pos_x = SCREEN_WIDTH - plate_width;
				}
				draw_square(plate_pos_x, plate_pos_y, plate_width, plate_height, 0x00ffffff);
				long_move_right = 0;
			} else if (long_move_left) {
				draw_square(plate_pos_x, plate_pos_y, plate_width, plate_height, 0x00000000);
				plate_pos_x = - (int)(( 1 + time/3) * facteur) + plate_pos_x;
				if (plate_pos_x < 0) {
					plate_pos_x = 0;
				}
				draw_square(plate_pos_x, plate_pos_y, plate_width, plate_height, 0x00ffffff);
				long_move_left = 0;
			} else {
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
		}
		has_move_left --;
		has_move_right --;
		vTaskDelay(MS2TICKS(delay));
	}
}


void keyboard_interrupt_handler() {
	uint32_t kdata;
	while (KEYBOARD->SR & KEYBOARD_SR_FIFO_NOT_EMPTY) {
		kdata = KEYBOARD->DATA;
		if (kdata & KEYBOARD_DATA_PRESSED) {
			//xprintf("key code: %d\n", KEYBOARD_KEY_CODE(kdata));
			switch (KEYBOARD_KEY_CODE(kdata)) {
				case 113: // Q
					minirisc_halt();
					break;
				case 32: // space
					memset(frame_buffer, 0, sizeof(frame_buffer)); // clear frame buffer
					break;
				case 79 :
					is_moving_right = 1;
					if (has_move_right <= 0) {
						has_move_right = 6;
					}
					break;
				case 80 :
					is_moving_left = 1;
					if (has_move_left <= 0) {
						has_move_left = 6;
					}
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
	int nb_ligne_bric = nb_bric /10; 

	for (int i = 0; i < 10; i++) {
		for (int j = 0; j < nb_ligne_bric; j++) {
        brics[i + 10 * j].pos_x = i * (bric_width + ecart);
        brics[i + 10 * j].pos_y = j * (bric_height + ecart);
			brics[i+10*j].is_active = 1;
			draw_square(brics[i + 10 * j].pos_x, brics[i + 10 * j].pos_y, bric_width - ecart, bric_height - ecart, 0x000000ee);
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

	printf("Speed: %.2f, Angle: %.2f radians\n", marble_speed, marble_angle);

}

int main() {
	init_game(0);


	xTaskCreate(collision_plate, "collision_plate", 8192, NULL, 1, NULL);
	xTaskCreate(move_marble,  "move",  8192, NULL, 1, NULL);
	xTaskCreate(move_plate, "moveplate", 8192, NULL, 1, NULL);
	vTaskStartScheduler();

	return 0;
}

