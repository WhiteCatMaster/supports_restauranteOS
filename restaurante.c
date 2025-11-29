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
#define COLACOCINA "/cola_cocina"

pid_t pid_sala, pid_cocina;
mqd_t mq_cocina_fd;

// Declarar los semáforos
sem_t semIngredientesListos;
sem_t semCocina;

// Funcion que unicamente se encarga de imprimir por pantalla que un plato está listo
void manejadorPlatoListo(int senial) {
  printf("[Sala] Señal recibida: Plato listo en cocina.\n");
}

// Funcion que devuelve un número aleatorio que será usado para los tiempos aleatorios
int tiempo_aleatorio(int min, int max) {
  return rand() % (max - min + 1) + min;
}

// Logica que ejecuta al detectar el "crtl + c"
void cerrar(int senial) {
  signal(SIGINT, cerrar);
}

#define MAX_SIZE 128
#define MSG_BUFFER_SIZE (MAX_SIZE + 1)

// Función que prepara los ingredientes
void *prepararIngredientes(void *args) {
  // Crear un char donde se guardarán las comandas de la cola, con longitu "MSG_BUFFER_SIZE"
  char buffer[MSG_BUFFER_SIZE];
  ssize_t bytes_read;

  // Va a estar ejecutandose continuamente para prepara los ingredientes
  while (1) {
    printf("[Preparación] Esperando comanda...\n");

    // Una cola es como un buzón para pasar mensajes entre procesos, la sala mete un mensaje en el buzón y la cocina los recibe mediante "mq_receive" y "mq_send"

    // "mq_receive" va a leer la cola "mq_cocina_fd" y va a guardar en "buffer" (variable creada con antelación) 
    // "MSG_BUFFER_SIZE" va a indicar el tamaño máximo de bytes que "mq_receive" puede copiar en buffer
    // "NULL" indica que no hay prioridad de mensaje
    bytes_read = mq_receive(mq_cocina_fd, buffer, MSG_BUFFER_SIZE, NULL);

    // Si el "bytes_read" da -1 significa que ha habido un error al intentar leer la cola
    if (bytes_read == -1) {
      perror("[Preparación] Error al recibir mensaje de la cola");
      continue;
    }

    // Asegurar que el mensaje recibido termina correctamente en "\0"
    if (bytes_read >= MSG_BUFFER_SIZE) {
      // Cortarlo donde esté el tamaño máximo y le ponerle el final ("\0") ahi mismo
      buffer[MSG_BUFFER_SIZE - 1] = '\0';
    } else {
      // Ponerle el final en el final, donde toca
      buffer[bytes_read] = '\0';
    }

    // Enseñar por pantalla la comanda recibida, la cual antes se ha guardado en "buffer"
    printf("[Preparación] Recibida comanda: %s. Preparando ingredientes...\n", buffer);

    // Esperar tiempo aleatorio para simular que se están preparando de verdad los ingredientes
    sleep(tiempo_aleatorio(3, 6));
    printf("[Preparación] Ingredientes listos.\n");

    // Indicar a la función "cocinar" que los ingredientes están listos, poniendo el semáforo a 1
    sem_post(&semIngredientesListos);
  }
}

// Función que cocina con los ingredientes que recibe
void *cocinar(void *arg) {

  while (1) {
    // El semáforo hace que el programa no siga hasta que no estén los ingredientes listos
    sem_wait(&semIngredientesListos);
    printf("[Cocina] Cocinando plato...\n");
    sleep(tiempo_aleatorio(4, 8));
    printf("[Cocina] Plato cocinado.\n");

    // Misma lógica que el semáforo anterior, solo que esta vez se ocupa de que el emplatado no se haga hasta que los ingredientes hayan sido cocinados
    sem_post(&semCocina);
  }
}

// Función que prepara el emplatado
void *emplatar(void *arg) {

  while (1) {
    sem_wait(&semCocina);
    printf("[Emplatado] Emplatando el plato...\n");
    sleep(tiempo_aleatorio(2, 4));
    printf("[Emplatado] Plato listo y emplatado.\n");

    // Enviar la señal "SENYAL" al proceso sala mediante su PID obtenida de "pid_sala" para indicar que el plato ya está listo
    kill(pid_sala, SENYAL);
  }
}

// Función main
int main(int argc, char *argv[]) {

  // Inicializar las variables que serán usadas
  int estado;
  int finCocina = 0;
  int finSala = 0;
  int pidFin;
  
  // Se crea un proceso hijo, por lo que hay dos procesos ejecutandose (proceso padre e hijo)
  // El padre, "pid_sala" contiene el PID (Process ID) real del hijo, un número mayor que 0
  // En el hijo, "pid_sala" valdrá 0
  pid_sala = fork();

  // Los procesos hijo tienen como valor 0, por lo que por este "if" solo lo pasará el proceso padre
  if (pid_sala != 0) {
    // Hacer un segundo fork, esta vez de cocina
    pid_cocina = fork();

    // Misma lógica que con el anterior "if", aqui solo entrará el proceso padre de cocina
    if (pid_cocina != 0) {
      do {
        // El "wait(&estado)" bloqueará al padre hasta que un hijo (cualquiera) muera, luego el PID del hijo se guardará en "pidFin"
        pidFin = wait(&estado);
        
        // Aquí se hará la comprobación entre el pid del hijo (pidFin) y el del padre (pid_sala o pid_cocina) para saber si el hijo que acaba de morir es de cocina o sala
        if (pidFin == pid_sala) {
          finSala = 1;
        } else if (pidFin == pid_cocina) {
          finCocina = 1;
        }

        // El padre seguira dando vueltas al "do-while" hasta que ambos hijos hayan muerto
      } while (!finCocina || !finSala);

    // Ruta alternativa del "if" a la que solo accederá el proceso hijo de cocina
    } else {
      
      // Declarar los hilos que se usarán en el proceso cocina
      pthread_t hiloPreparacion, hiloCocinar, hiloEmplatar;

      // Inicializar los semáforos del proceso cocina a 0
      // El primer 0 indica que el semáforo será privado de los hilos de un proceso
      // El segundo 0 es el valor inicial del semáforo, indicando que la "barrera" estará cerrada al empezar
      sem_init(&semIngredientesListos, 0, 0);
      sem_init(&semCocina, 0, 0);

      // Configuración de la cola de mensajes POSIX
      struct mq_attr attr;
      attr.mq_flags = 0;
      attr.mq_maxmsg = 10;
      attr.mq_msgsize = MAX_SIZE;
      attr.mq_curmsgs = 0;

      // Con "mq_open" abre la cola de mensajes con el nombre "COLACOCINA"
      // Con "O_CREAT", si la cola no existe de antes, la crea
      // "O_RDONLY" indica que la cola solo será abierta para leer los mensajes
      // "0644" son permisos de seguridad y "&attr" son las reglas de configuración definidas en las lineas anteriores
      mq_cocina_fd = mq_open(COLACOCINA, O_CREAT | O_RDONLY, 0644, &attr);

      printf("[Cocina] Comienzo de la preparación de platos...\n");

      // Inicializar los hilos definidos anteriormente, especificandoles que función de la cocina hará cada uno
      pthread_create(&hiloPreparacion, NULL, prepararIngredientes, NULL);
      pthread_create(&hiloCocinar, NULL, cocinar, NULL);
      pthread_create(&hiloEmplatar, NULL, emplatar, NULL);

      // Hacen que el proceso cocina espere a que los tres hilos hayan terminado (a que todos los trabajos de la cocina hayan sido terminados) antes de poder cerrarse
      pthread_join(hiloPreparacion, NULL);
      pthread_join(hiloCocinar, NULL);
      pthread_join(hiloEmplatar, NULL);
    }

  } else {
    
    // El hijo de proceso sala
    printf("[Sala] Inicio de la gestión de comandas...\n");

    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;      
    attr.mq_msgsize = MAX_SIZE;
    attr.mq_curmsgs = 0;

    // Se crea la cola de mensajes, en este caso con "O_WRONLY", indicando que solo se harán escrituras. "La sala escribe las comandas y la cocina las lee"
    mq_cocina_fd = mq_open(COLACOCINA, O_CREAT | O_WRONLY, 0644, &attr);
    char mensajeComanda[MAX_SIZE];

    int num_comanda = 0;

    // Cuando le llegue la señal "SENYAL" (cuando el plato ha terminado de emplatarse) ejecutar la funcion "manejadorPlatoListo"
    signal(SENYAL, manejadorPlatoListo);
    
    // Bucle infinito que cada cierto tiempo simula la llegada de una comanda y la envía a la cocina mediante una cola de mensajes
    while (1) {
      sleep(tiempo_aleatorio(5, 10));
      
      printf("[Sala] Recibida comanda de cliente. Solicitando plato de la "
             "comanda nº %d a la cocina...\n",
             num_comanda);

      sprintf(mensajeComanda, "Mesa_%d_Plato_del_dia", num_comanda);       

      mq_send(mq_cocina_fd, mensajeComanda, strlen(mensajeComanda) + 1, 1);
        
      
      num_comanda++;
    }
  }

  exit(0);
}