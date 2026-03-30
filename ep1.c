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
    int core;
    int idx;
} TaskParams;

int total_preempcoes = 0;
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

int core_process[1024];
int proc_running[50];
int preempt_flag[50];
int tempo_restante_seg[50];
int total_processos_concluidos = 0;
long long inicio_simulacao_seg;

long long current_time_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec;
}

int get_num_cores(void) {
    cpu_set_t set;
    CPU_ZERO(&set);
    sched_getaffinity(0, sizeof(set), &set);
    return CPU_COUNT(&set);
}

void init_state(Processo processos[], int total_processos) {
    for (int i = 0; i < 1024; i++) core_process[i] = -1;
    memset(proc_running, 0, sizeof(proc_running));
    memset(preempt_flag, 0, sizeof(preempt_flag));
    total_processos_concluidos = 0;
    total_preempcoes = 0;
    
    inicio_simulacao_seg = current_time_sec();
    
    for (int i = 0; i < total_processos; i++) {
        processos[i].fl_completo = 0;
        tempo_restante_seg[i] = processos[i].dt; // Não multiplica mais por 1000
    }
}

void* executar_thread_tick(void* arg) {
    TaskParams *args = (TaskParams*)arg;
    int idx = args->idx;
    int c = args->core;
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(c, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    while (1) {
        sleep(1);

        pthread_mutex_lock(&state_mutex);
        
        if (preempt_flag[idx] == 1) {
            core_process[c] = -1;
            proc_running[idx] = 0;
            pthread_mutex_unlock(&state_mutex);
            break;
        }

        tempo_restante_seg[idx] -= 1;
        
        if (tempo_restante_seg[idx] <= 0) {
            if (!args->processo->fl_completo) {
                args->processo->fl_completo = 1;
                total_processos_concluidos++;
                
                long long tempo_atual_seg = current_time_sec() - inicio_simulacao_seg;
                args->processo->tf = (int)tempo_atual_seg;
                args->processo->tr = args->processo->tf - args->processo->t0;
                args->processo->fl_cumpriu = (args->processo->tf <= args->processo->deadline) ? 1 : 0;
            }
            core_process[c] = -1;
            proc_running[idx] = 0;
            pthread_mutex_unlock(&state_mutex);
            break;
        }

        pthread_mutex_unlock(&state_mutex);
    }
    
    free(args);
    return NULL;
}

void handle_sjf(Processo processos[], int total_processos) {
    init_state(processos, total_processos);
    int num_cores = get_num_cores();

    while (1) {
        pthread_mutex_lock(&state_mutex);
        if (total_processos_concluidos >= total_processos) {
            pthread_mutex_unlock(&state_mutex);
            break;
        }
        
        long long tempo_atual_seg = current_time_sec() - inicio_simulacao_seg;

        for (int c = 0; c < num_cores; c++) {
            if (core_process[c] == -1) {
                int indice_menor = -1;
                
                for (int i = 0; i < total_processos; i++) {
                    if (processos[i].t0 <= tempo_atual_seg && !processos[i].fl_completo && !proc_running[i]) {
                        if (indice_menor == -1 || processos[i].dt < processos[indice_menor].dt || 
                           (processos[i].dt == processos[indice_menor].dt && processos[i].t0 < processos[indice_menor].t0)) {
                            indice_menor = i;
                        }
                    }
                }

                if (indice_menor != -1) {
                    core_process[c] = indice_menor;
                    proc_running[indice_menor] = 1;
                    preempt_flag[indice_menor] = 0;

                    TaskParams *args = malloc(sizeof(TaskParams));
                    args->processo = &processos[indice_menor];
                    args->core = c;
                    args->idx = indice_menor;

                    pthread_t tid;
                    pthread_create(&tid, NULL, executar_thread_tick, args);
                    pthread_detach(tid);
                }
            }
        }
        pthread_mutex_unlock(&state_mutex);
        usleep(50000);
    }
}

void handle_rr(Processo processos[], int total_processos, int quantum) {
    init_state(processos, total_processos);
    int num_cores = get_num_cores();
    int next_index = 0;
    long long tempo_inicio_core[1024] = {0};

    while (1) {
        pthread_mutex_lock(&state_mutex);
        if (total_processos_concluidos >= total_processos) {
            pthread_mutex_unlock(&state_mutex);
            break;
        }
        
        long long tempo_atual_seg = current_time_sec() - inicio_simulacao_seg;

        for (int c = 0; c < num_cores; c++) {
            int p = core_process[c];
            if (p != -1 && !preempt_flag[p] && tempo_restante_seg[p] > 0) {
                if (tempo_atual_seg - tempo_inicio_core[c] >= quantum) {
                    preempt_flag[p] = 1;
                    total_preempcoes++;
                }
            }
        }

        for (int c = 0; c < num_cores; c++) {
            if (core_process[c] == -1) {
                int indice_escolhido = -1;
                
                for (int i = 0; i < total_processos; i++) {
                    int idx = (next_index + i) % total_processos;
                    if (processos[idx].t0 <= tempo_atual_seg && !processos[idx].fl_completo && !proc_running[idx] && tempo_restante_seg[idx] > 0) {
                        indice_escolhido = idx;
                        next_index = (idx + 1) % total_processos;
                        break;
                    }
                }

                if (indice_escolhido != -1) {
                    core_process[c] = indice_escolhido;
                    proc_running[indice_escolhido] = 1;
                    preempt_flag[indice_escolhido] = 0;
                    tempo_inicio_core[c] = tempo_atual_seg;

                    TaskParams *args = malloc(sizeof(TaskParams));
                    args->processo = &processos[indice_escolhido];
                    args->core = c;
                    args->idx = indice_escolhido;

                    pthread_t tid;
                    pthread_create(&tid, NULL, executar_thread_tick, args);
                    pthread_detach(tid);
                }
            }
        }
        pthread_mutex_unlock(&state_mutex);
        usleep(50000);
    }
}

void handle_prioridade(Processo processos[], int total_processos) {
    init_state(processos, total_processos);
    int num_cores = get_num_cores();

    while (1) {
        pthread_mutex_lock(&state_mutex);
        if (total_processos_concluidos >= total_processos) {
            pthread_mutex_unlock(&state_mutex);
            break;
        }
        
        long long tempo_atual_seg = current_time_sec() - inicio_simulacao_seg;

        for (int c = 0; c < num_cores; c++) {
            int p = core_process[c];
            if (p != -1 && !preempt_flag[p] && tempo_restante_seg[p] > 0) {
                int slack_p = processos[p].deadline - tempo_atual_seg - tempo_restante_seg[p];
                
                int tem_melhor = 0;
                for (int i = 0; i < total_processos; i++) {
                    if (processos[i].t0 <= tempo_atual_seg && !processos[i].fl_completo && !proc_running[i] && tempo_restante_seg[i] > 0) {
                        int slack_i = processos[i].deadline - tempo_atual_seg - tempo_restante_seg[i];
                        
                        if (slack_i < slack_p ||
                           (slack_i == slack_p && processos[i].deadline < processos[p].deadline) ||
                           (slack_i == slack_p && processos[i].deadline == processos[p].deadline && processos[i].t0 < processos[p].t0)) {
                            tem_melhor = 1;
                            break;
                        }
                    }
                }

                if (tem_melhor) {
                    preempt_flag[p] = 1;
                    total_preempcoes++;
                }
            }
        }

        for (int c = 0; c < num_cores; c++) {
            if (core_process[c] == -1) {
                int indice_escolhido = -1;
                int melhor_slack = INT_MAX;

                for (int i = 0; i < total_processos; i++) {
                    if (processos[i].t0 <= tempo_atual_seg && !processos[i].fl_completo && !proc_running[i] && tempo_restante_seg[i] > 0) {
                        int slack = processos[i].deadline - tempo_atual_seg - tempo_restante_seg[i];
                        
                        int p_deadline = (indice_escolhido == -1) ? INT_MAX : processos[indice_escolhido].deadline;
                        int p_t0 = (indice_escolhido == -1) ? INT_MAX : processos[indice_escolhido].t0;

                        if (slack < melhor_slack ||
                            (slack == melhor_slack && processos[i].deadline < p_deadline) ||
                            (slack == melhor_slack && processos[i].deadline == p_deadline && processos[i].t0 < p_t0)) {
                            melhor_slack = slack;
                            indice_escolhido = i;
                        }
                    }
                }

                if (indice_escolhido != -1) {
                    core_process[c] = indice_escolhido;
                    proc_running[indice_escolhido] = 1;
                    preempt_flag[indice_escolhido] = 0;

                    TaskParams *args = malloc(sizeof(TaskParams));
                    args->processo = &processos[indice_escolhido];
                    args->core = c;
                    args->idx = indice_escolhido;

                    pthread_t tid;
                    pthread_create(&tid, NULL, executar_thread_tick, args);
                    pthread_detach(tid);
                }
            }
        }
        pthread_mutex_unlock(&state_mutex);
        usleep(50000);
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