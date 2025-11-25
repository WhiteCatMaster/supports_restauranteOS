#include <mqueue.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

pid_t pid_sala, pid_cocina;
sem_t sem_ingredientes_listos;
mqd_t mq_cocina_fd;
sem_t sem_preparacion;
sem_t sem_cocina;
sem_t sem_emplatar;

int tiempo_aleatorio(int min, int max) {
  return rand() % (max - min + 1) + min;
}
void cerrar(int senial) {
  // Logica que ejecuta al detectar el crtl + c
  signal(SIGINT, cerrar);
}

#define MAX_SIZE 128
#define MSG_BUFFER_SIZE (MAX_SIZE + 1)

void *preparar_ingredientes(void *args) {
  char buffer[MSG_BUFFER_SIZE];
  ssize_t bytes_read;

  while (1) {
    printf("[Preparación] Esperando comanda de la Sala...\n");

    bytes_read = mq_receive(mq_cocina_fd, buffer, MSG_BUFFER_SIZE, NULL);

    // El -1 indica que la funcion ha fallado y que no ha podido recibir el
    // mensaje
    if (bytes_read == -1) {
      perror("[Preparación] Error al recibir mensaje de la cola");
      continue;
    }

    // Al parecer se asegura de que el mensaje recibido sea tratado como un
    // string
    buffer[bytes_read] = '\0';

    printf("[Preparación] Recibida comanda: %s. Preparando ingredientes...\n",
           buffer);

    sleep(tiempo_aleatorio(3, 6));
    printf("[Preparación] Ingredientes listos para cocinar.\n");

     if (sem_post(&sem_ingredientes_listos) == -1) {
         perror("[Preparación] Error al hacer sem_post");
     }

  }

  // pthread_exit(NULL); // Aunque inalcanzable en un bucle infinito
}

void *cocinar(void *arg) {

  while (1) {
    sem_wait(&sem_ingredientes_listos);
    printf("[Cocina] Cocinando plato...\n");
    sleep(tiempo_aleatorio(4, 8));
    printf("[Cocina] Plato cocinado.\n");
    sem_post(&sem_cocina);
  }
}

// Hilo para el emplatado
void *emplatar(void *arg) {

  while (1) {
    // Tengo que esperar a que haya comida cocinada para emplatar
    sem_wait(&sem_emplatar);
    printf("[Emplatado] Emplatando el plato...\n");
    sleep(tiempo_aleatorio(2, 4));
    printf("[Emplatado] Plato listo y emplatado.\n");
    kill(pid_sala, SIGALRM);
  }
}

int main(int argc, char *argv[]) {
  int estado;
  int fin_cocina = 0, fin_sala = 0;
  int pidFin;
  pid_sala = fork();

  if (pid_sala != 0) {
    pid_cocina = fork();
    if (pid_cocina != 0) {
      /* Proceso padre */
      // signal(SIGINT, cerrar);
      do {
        pidFin = wait(&estado);
        if (pidFin == pid_sala) {
          fin_sala = 1;
        } else if (pidFin == pid_cocina) {
          fin_cocina = 1;
        }
      } while (!fin_cocina || !fin_sala);

    } else {
      /* Proceso Cocina */
      printf("[Cocina] Comienzo de la preparación de platos...\n");

      if (sem_init(&sem_ingredientes_listos, 0, 0) == -1) {
        perror("Error al inicializar semáforo sem_ingredientes_listos");
        exit(EXIT_FAILURE);
}
    }
  } else {
    /* Proceso Sala */
    printf("[Sala] Inicio de la gestión de comandas...\n");

    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;      
    attr.mq_msgsize = MAX_SIZE;
    attr.mq_curmsgs = 0;

    
    mq_cocina_fd = mq_open(NOMBRE_COLA, O_CREAT | O_WRONLY, 0644, &attr);
    char mensaje_comanda[MAX_SIZE];

    int num_comanda = 0;

    while (1) {

      sleep(tiempo_aleatorio(5, 10));
      
      printf("[Sala] Recibida comanda de cliente. Solicitando plato de la "
             "comanda nº %d a la cocina...\n",
             num_comanda);

      sprintf(mensaje_comanda, "Mesa_%d_Plato_del_dia", num_comanda);       

      mq_send(mq_cocina_fd, mensaje_comanda, strlen(mensaje_comanda) + 1, 1);
    
      num_comanda++;
    }
  }

  exit(0);
}