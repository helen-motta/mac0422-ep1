#ifndef EP1_H
#define EP1_H

#include <stddef.h>

typedef struct {
    char nome[33];
    int deadline;
    int t0;
    int dt;
    int fl_completo;
    int tf;
    int tr;
    int fl_cumpriu;
} Processo;

void handle_sjf(Processo processos[], int total_processos);
void handle_rr(Processo processos[], int total_processos, int quantum);
void handle_prioridade(Processo processos[], int total_processos);
int get_num_cores(void);

#endif
