//------------------- DIAG_DATA.C ---------------------- 

#include "diag_data.h"

void append_to_string(struct diag_data* data, char string[]) {
    char tmp[256];
    dtc(*data, tmp);
    strcat(string, tmp);
}

void dtc(struct diag_data data, char string[]) {
    // variabile di buffer
    char temp[128];
    // nome e WCET devono sempre essere stampati
    sprintf(string, "%s:\n\tWCET = %ld\n", data.name, data.WCET);
    // se il campo e' presente, allora viene stampato e viene pulito il buffer
    if (data.avg_sensor != -100) {
        sprintf(temp, "\tavg_sensor = %d\n", data.avg_sensor);
        strcat(string, temp);
        strcpy(temp, "\0");
    }
    if (data.control != -100) {
        sprintf(temp, "\tcontrol = %d\n", data.control);
        strcat(string, temp);
        strcpy(temp, "\0");
    }
    if (data.control_action != -100) {
        sprintf(temp, "\tcontrol_action = %d\n", data.control_action);
        strcat(string, temp);
        strcpy(temp, "\0");
    }
    if (data.reference != -100) {
        sprintf(temp, "\treference = %d\n", data.reference);
        strcat(string, temp);
        strcpy(temp, "\0");
    }

} 

void init_diag_data(struct diag_data* data, char name[]) {
    strcpy(data->name, name);
    // Il WCET è inizializzato a 0, perchè così facendo alla prima iterazione
	// il max viene sicuramente aggiornato.
    data->WCET = 0;
	// tutti i campi sono inizializzati a -100 perchè questo è un valore sicuramente
	// non possibile.
    data->control = -100;
    data->control_action = -100;
    data->avg_sensor = -100;
    data->reference = -100;
}