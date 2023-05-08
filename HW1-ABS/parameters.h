#define TICK_TIME 100000    // velocità dell'impianto
#define SENSOR_QUEUE_NAME   "/sensor_queue"
#define ACTUATOR_QUEUE_NAME "/actuator_queue"
#define REFERENCE_QUEUE_NAME "/reference_queue"
#define QUEUE_PERMISSIONS 0660
#define MAX_MESSAGES 1      // un solo messaggio nella coda, altrimenti se il controllore inizia tardi legge le vecchie misurazioni
#define MAX_MSG_SIZE 16
#define BUF_SIZE 3      // N nello schema sul pdf - su quanti campioni fare la media mobile