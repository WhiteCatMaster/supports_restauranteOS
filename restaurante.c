#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <mqueue.h>
#include <string.h>

pid_t pid_sala, pid_cocina;
mqd_t mq_cocina_fd;
sem_t sem_preparacion;
sem_t sem_cocina;

int tiempo_aleatorio(int min, int max) {
    return rand() % (max - min + 1) + min;
}

#define MAX_SIZE 128
#define MSG_BUFFER_SIZE (MAX_SIZE + 1)

void* preparar_ingredientes(void* args) {
    char buffer[MSG_BUFFER_SIZE];
    ssize_t bytes_read;
    
    while (1) {
        printf("[Preparación] Esperando comanda de la Sala...\n");

        bytes_read = mq_receive(mq_cocina_fd, buffer, MSG_BUFFER_SIZE, NULL);

        //El -1 indica que la funcion ha fallado y que no ha podido recibir el mensaje
        if (bytes_read == -1) {
            perror("[Preparación] Error al recibir mensaje de la cola");
            continue; 
        }

        //Al parecer se asegura de que el mensaje recibido sea tratado como un string
        buffer[bytes_read] = '\0'; 
        
        printf("[Preparación] Recibida comanda: %s. Preparando ingredientes...\n", buffer);
        
        sleep(tiempo_aleatorio(3, 6)); 
        printf("[Preparación] Ingredientes listos para cocinar.\n");
        
        if (sem_post(&sem_ingredientes_listos) == -1) {
            perror("[Preparación] Error al hacer sem_post");
        }
    }
    
    // pthread_exit(NULL); // Aunque inalcanzable en un bucle infinito
}

void* cocinar(void* arg) {

    while (1) {
        sem_wait(&sem_preparacion);
        printf("[Cocina] Cocinando plato...\n");
        sleep(tiempo_aleatorio(4, 8));
        printf("[Cocina] Plato cocinado.\n");
        sem_post(&sem_cocina);

    }
}

// Hilo para el emplatado
void* emplatar(void* arg) {

    while (1) {

        printf("[Emplatado] Emplatando el plato...\n");
        sleep(tiempo_aleatorio(2, 4));
        printf("[Emplatado] Plato listo y emplatado.\n");

    }
}

int main(int argc, char* argv[]) {

    pid_sala = fork();

    if (pid_sala != 0) {
        pid_cocina = fork();
        if (pid_cocina != 0) {
		/* Proceso padre */



        } else {
            /* Proceso Cocina */
            printf("[Cocina] Comienzo de la preparación de platos...\n");



        }
    } else {
        /* Proceso Sala */
        printf("[Sala] Inicio de la gestión de comandas...\n");

	int num_comanda = 0;

        while (1) {

            sleep(tiempo_aleatorio(5, 10));
            printf("[Sala] Recibida comanda de cliente. Solicitando plato de la comanda nº %d a la cocina...\n", num_comanda);


            num_comanda++;

        }
    }

    exit(0);
}
