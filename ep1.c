#define _GNU_SOURCE
#include "ep1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>

typedef struct {
    Processo *processo;
    int slice;
    int core;
} TaskParams;

int total_preempcoes = 0;

int get_num_cores(void) {
    cpu_set_t set;
    CPU_ZERO(&set);

    sched_getaffinity(0, sizeof(set), &set);

    return CPU_COUNT(&set);
}

void* executar_slice(void* arg) {
    TaskParams *args = (TaskParams*)arg;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(args->core, &cpuset);

    pthread_t tid = pthread_self();
    pthread_setaffinity_np(tid, sizeof(cpuset), &cpuset);

    sleep(args->slice);
    free(args);
    return NULL;
}

void handle_sjf(Processo processos[], int total_processos) {
    int processos_finalizados = 0;
    int tempo_atual = 0;
    
    for(int i = 0; i < total_processos; i++) {
        processos[i].fl_completo = 0;
    }

    time_t inicio = time(NULL);

    while (processos_finalizados < total_processos) {
        int indice_menor = -1;

        for (int i = 0; i < total_processos; i++) {
            if (processos[i].t0 <= tempo_atual && !processos[i].fl_completo) {
                if (indice_menor == -1 || processos[i].dt < processos[indice_menor].dt || 
                (processos[i].dt == processos[indice_menor].dt && processos[i].t0 < processos[indice_menor].t0)) {
                    indice_menor = i;
                }
            }
        }

        if (indice_menor == -1) {
            sleep(1);
            tempo_atual = (int)(time(NULL) - inicio);
        } else {
            int num_cores = get_num_cores();
            int core = indice_menor % num_cores;
            pthread_t tid;
            TaskParams *args = malloc(sizeof(TaskParams));
            args->processo = &processos[indice_menor];
            args->slice = processos[indice_menor].dt;
            args->core = core;

            pthread_create(&tid, NULL, executar_slice, args);
            pthread_join(tid, NULL);

            processos[indice_menor].fl_completo = 1;
            processos_finalizados++;

            tempo_atual = (int)(time(NULL) - inicio);

            processos[indice_menor].tf = tempo_atual;
            processos[indice_menor].tr = processos[indice_menor].tf - processos[indice_menor].t0;
            
            if (processos[indice_menor].tf <= processos[indice_menor].deadline) {
                processos[indice_menor].fl_cumpriu = 1;
            } else {
                processos[indice_menor].fl_cumpriu = 0;
            }
        }
    }
}

void handle_rr(Processo processos[], int total_processos, int quantum) {
    int processos_finalizados = 0;
    int tempo_atual = 0;
    int rem_dt[50];

    for (int i = 0; i < total_processos; i++) {
        rem_dt[i] = processos[i].dt;
        processos[i].fl_completo = 0;
    }

    time_t inicio = time(NULL);
    int next_index = 0;

    while (processos_finalizados < total_processos) {
        int indice_escolhido = -1;

        for (int i = 0; i < total_processos; i++) {
            int idx = (next_index + i) % total_processos;
            if (processos[idx].t0 <= tempo_atual && !processos[idx].fl_completo && rem_dt[idx] > 0) {
                indice_escolhido = idx;
                break;
            }
        }

        if (indice_escolhido == -1) {
            sleep(1);
            tempo_atual = (int)(time(NULL) - inicio);
            continue;
        }

        int num_cores = get_num_cores();
        int core = indice_escolhido % num_cores;
        pthread_t tid;
        TaskParams *args = malloc(sizeof(TaskParams));
        args->processo = &processos[indice_escolhido];
        args->slice = quantum;
        args->core = core;

        pthread_create(&tid, NULL, executar_slice, args);
        pthread_join(tid, NULL);

        rem_dt[indice_escolhido] -= quantum;
        tempo_atual = (int)(time(NULL) - inicio);

        if (rem_dt[indice_escolhido] <= 0) {
            processos[indice_escolhido].fl_completo = 1;
            processos_finalizados++;
            processos[indice_escolhido].tf = tempo_atual;
            processos[indice_escolhido].tr = processos[indice_escolhido].tf - processos[indice_escolhido].t0;
            processos[indice_escolhido].fl_cumpriu = (processos[indice_escolhido].tf <= processos[indice_escolhido].deadline) ? 1 : 0;
        } else {
            total_preempcoes++;
        }

        next_index = (indice_escolhido + 1) % total_processos;
    }
}

void handle_prioridade(Processo processos[], int total_processos) {
    int processos_finalizados = 0;
    int tempo_atual = 0;
    int rem_dt[50];

    for (int i = 0; i < total_processos; i++) {
        rem_dt[i] = processos[i].dt;
        processos[i].fl_completo = 0;
    }

    time_t inicio = time(NULL);

    while (processos_finalizados < total_processos) {
        int indice_escolhido = -1;
        int melhor_slack = INT_MAX;

        for (int i = 0; i < total_processos; i++) {
            if (processos[i].t0 <= tempo_atual && !processos[i].fl_completo && rem_dt[i] > 0) {
                int slack = processos[i].deadline - tempo_atual - rem_dt[i];

                if (slack < melhor_slack ||
                    (slack == melhor_slack && processos[i].deadline < processos[indice_escolhido].deadline) ||
                    (slack == melhor_slack && processos[i].deadline == processos[indice_escolhido].deadline && processos[i].t0 < processos[indice_escolhido].t0)) {
                    melhor_slack = slack;
                    indice_escolhido = i;
                }
            }
        }

        if (indice_escolhido == -1) {
            sleep(1);
            tempo_atual = (int)(time(NULL) - inicio);
            continue;
        }

        int quantum = 1;

        int num_cores = get_num_cores();
        int core = indice_escolhido % num_cores;
        pthread_t tid;
        TaskParams *args = malloc(sizeof(TaskParams));
        args->processo = &processos[indice_escolhido];
        args->slice = quantum;
        args->core = core;

        pthread_create(&tid, NULL, executar_slice, args);
        pthread_join(tid, NULL);

        rem_dt[indice_escolhido] -= quantum;
        tempo_atual = (int)(time(NULL) - inicio);

        if (rem_dt[indice_escolhido] <= 0) {
            processos[indice_escolhido].fl_completo = 1;
            processos_finalizados++;
            processos[indice_escolhido].tf = tempo_atual;
            processos[indice_escolhido].tr = processos[indice_escolhido].tf - processos[indice_escolhido].t0;
            processos[indice_escolhido].fl_cumpriu = (processos[indice_escolhido].tf <= processos[indice_escolhido].deadline) ? 1 : 0;
        } else {
            total_preempcoes++;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Esperado: %s <escalonador> <arquivo_entrada> <arquivo_saida>\n", argv[0]);
        return 1;
    }

    int escalonador = atoi(argv[1]);
    char *arquivo_entrada = argv[2];
    char *arquivo_saida = argv[3];

    FILE *entrada = fopen(arquivo_entrada, "r");

    if (entrada == NULL) {
        printf("Erro: Não foi possível abrir o arquivo de entrada: '%s'\n", arquivo_entrada);
        return 1;
    }

    Processo processos[50];
    int total_processos = 0;

    while (fscanf(entrada, "%32s %d %d %d",
                  processos[total_processos].nome,
                  &processos[total_processos].deadline,
                  &processos[total_processos].t0,
                  &processos[total_processos].dt) == 4) {

        total_processos++;

        if (total_processos >= 50) {
            break;
        }
    }
    fclose(entrada);

    if (escalonador == 1) {
        handle_sjf(processos, total_processos);
    } else if (escalonador == 2) {
        handle_rr(processos, total_processos, 3);
    } else if (escalonador == 3) {
        handle_prioridade(processos, total_processos);
    } else {
        printf("Escalonador desconhecido.\n");
        return 1;
    }

    FILE *file_out = fopen(arquivo_saida, "w");
    if (file_out == NULL) {
        perror("ep1: Erro ao criar arquivo de saída");
        return 1;
    }

    for (int i = 0; i < total_processos; i++) {
        fprintf(file_out, "%d %s %d %d\n", 
                processos[i].fl_cumpriu, 
                processos[i].nome, 
                processos[i].tf, 
                processos[i].tr);
    }

    fprintf(file_out, "%d\n", total_preempcoes);
    fclose(file_out);

    return 0;
}