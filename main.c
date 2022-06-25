/***********************************************************************************
* Implementering av flertrådning via biblioteket pthread.h. I detta program
* implementeras fyra trådar, varav tre används för att styra tre lysdioder och
* en används för att läsa in heltal inmatade från tangentbordet för att styra
* lysdioderna. Lysdioderna blinkar antingen med given blinkhastighet, som varierar
* mellan respektive lysdiod, eller hålls släckta. För att toggla en given lysdiod
* kan ett heltal 1 - antalet lysdioder matas in från tangentbordet. En statisk
* array används så att fler lysdioder enkelt kan läggas till, där den enda
* modifikation som krävs är att lägga till de nya lysdioderna i arrayen.
*
* Kompilera programmet och skapa en körbar fil döpt main via följande kommando:
* gcc main.c -o main -Wall -l gpiod -l pthread
*
* Kör sedan programmet via följande kommando:
* ./main
***********************************************************************************/

/* Inkluderingsdirektiv: */
#include <stdio.h> /* Funktioner printf, fgets med mera. */
#include <stdlib.h> /* Funktionen atoi för typomvandling mellan text och tal. */
#include <stdbool.h> /* Datatypen bool. */
#include <gpiod.h> /* Linux officiella drivrutiner för GPIO-implementering. */
#include <unistd.h> /* Funktioner sleep och usleep för fördröjning. */
#include <pthread.h> /* Flertrådning. */

/***********************************************************************************
* led_controller: Strukt för enkel styrning av en lysdiod med en enable-signal,
*                 där denna lysdiod antingen blinkar med given blinkhastighet
*                 eller hålls släckt.
***********************************************************************************/
struct led_controller
{
   struct gpiod_line** led; /* Pekare till lysdiodens GPIO-linjepekare. */
   const size_t delay_time; /* Blinkhastighet mätt i millisekunder. */
   bool enabled; /* Indikerar ifall lysdioden är tänd eller släckt. */
};

/***********************************************************************************
* led_controller_vector: Strukt för lagring av led-controllers i en statisk array,
*                        där antalet controllers registreras.
***********************************************************************************/
struct led_controller_vector
{
   struct led_controller** data; /* Array med led-controllers. */
   const size_t size; /* Antalet led-controllers. */
};

/***********************************************************************************
* led_blink: Blinkar lysdiode med valbar blinkhastighet.
***********************************************************************************/
static void led_blink(struct gpiod_line* restrict self, const size_t delay_time)
{
   gpiod_line_set_value(self, 1);
   usleep(delay_time * 1000);
   gpiod_line_set_value(self, 0);
   usleep(delay_time * 1000);
   return;
}

/***********************************************************************************
* led_process: Process för blinkning av lysdiod via enable-signal.
***********************************************************************************/
static void* led_process(void* led)
{
   struct led_controller* self = (struct led_controller*)led;

   while (1)
   {
      if (self->enabled)
      {
         led_blink(*self->led, self->delay_time);
      }
      else
      {
         gpiod_line_set_value(*self->led, 0);
      }
   }

   return 0;
}

/***********************************************************************************
* read_process: Läser kontinuerligt av inmatade tecken från tangentbordet för
*               att därigenom styra vilka lysdioder som blinkar eller inte.
*               För att toggla en given lysdiod skall användaren mata in ett tal
*               1 upp till och med antalet lysdioder lagrade i en statisk array
*               innehållande led-controllers.
***********************************************************************************/
static void* read_process(void* arg)
{
   struct led_controller_vector* self = (struct led_controller_vector*)arg;
   char s[50] = { '\0' };

   while (1)
   {
      fgets(s, sizeof(s), stdin);
      const size_t number = atoi(s);
      
      if (number <= self->size)
      {
         self->data[number - 1]->enabled = !self->data[number - 1]->enabled;
      }
   }

   return 0;
}

/***********************************************************************************
* main: Ansluter tre lysdioder till PIN 17, 22 och 23. Lysdioderna implementeras
*       i var sin led-controller med olika fördröjningstid, som via var sin tråd
*       möjliggör att dessa kan blinka separat samt med olika blinkhastigheter.
*       Varje led-controller lagras i sin tur i en vektor, som möjliggör att
*       användaren kan mata in ett tal 1 - antalet lysdioder för att toggla en
*       given lysdiod mellan att vara släckt och blinka, vilket åstadkommes via
*       en separat process. I detta fall när tre lysdioder har anslutits så kan
*       dessa togglas via inmatning av heltal 1 - 3.
***********************************************************************************/
int main(void)
{
   struct gpiod_chip* chip0 = gpiod_chip_open("/dev/gpiochip0");
   struct gpiod_line* led1 = gpiod_chip_get_line(chip0, 17);
   struct gpiod_line* led2 = gpiod_chip_get_line(chip0, 22);
   struct gpiod_line* led3 = gpiod_chip_get_line(chip0, 23);

   gpiod_line_request_output(led1, "led1", 0);
   gpiod_line_request_output(led2, "led2", 0);
   gpiod_line_request_output(led3, "led3", 0);

   struct led_controller l1 = 
   { 
      .led = &led1, 
      .delay_time = 500, 
      .enabled = false 
   };
   
   struct led_controller l2 = 
   { 
      .led = &led2, 
      .delay_time = 100, 
      .enabled = false 
   };
   
   struct led_controller l3 = 
   { 
      .led = &led3, 
      .delay_time = 1000, 
      .enabled = false 
   };

   struct led_controller* leds[] = { &l1, &l2, &l3 };
   const size_t size = sizeof(leds) / sizeof(struct led_controller*);

   struct led_controller_vector led_vector = 
   { 
      .data = leds, 
      .size = size 
   };

   pthread_t t1, t2, t3, t4;
   pthread_create(&t1, 0, &led_process, &l1);
   pthread_create(&t2, 0, &led_process, &l2);
   pthread_create(&t3, 0, &led_process, &l3);
   pthread_create(&t4, 0, &read_process, &led_vector);

   pthread_join(t1, 0);
   pthread_join(t2, 0);
   pthread_join(t3, 0);
   pthread_join(t4, 0);
   return 0;
}
