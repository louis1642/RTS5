//------------------- PARAMETERS.H ---------------------- 

#ifndef _PARAMETERS
#define _PARAMETERS

#define TICK_TIME 100000    // velocità dell'impianto   (T nel diagramma)
#define SENSOR_R_QUEUE_NAME   "/sensor_r_queue"
#define SENSOR_L_QUEUE_NAME   "/sensor_l_queue"
#define ACTUATOR_R_QUEUE_NAME "/actuator_r_queue"
#define ACTUATOR_L_QUEUE_NAME "/actuator_l_queue"
#define REFERENCE_R_QUEUE_NAME "/reference_r_queue"
#define REFERENCE_L_QUEUE_NAME "/reference_l_queue"
#define DIAG_REQUEST_QUEUE_NAME "/req_ps"
#define DIAG_RESPONSE_QUEUE_NAME "/res_ps"
#define QUEUE_PERMISSIONS 0660
#define MAX_MESSAGES 1      // un solo messaggio nella coda, altrimenti se il controllore inizia tardi legge le vecchie misurazioni
#define MAX_MSG_SIZE 16
#define BUF_SIZE 3      // N nello schema sul pdf - su quanti campioni fare la media mobile
#define MAX_DIAG_MSG_SIZE 1024   // i messaggi di diagnostica sono lunghi

#endif