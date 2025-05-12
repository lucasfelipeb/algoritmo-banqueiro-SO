#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

// ----- Constantes -----
#define NUMERO_CLIENTES 5
#define NUMERO_RECURSOS 3

// ----- Variaveis Globais -----
int disponivel[NUMERO_RECURSOS];
int maximo[NUMERO_CLIENTES][NUMERO_RECURSOS];
int alocado[NUMERO_CLIENTES][NUMERO_RECURSOS];
int necessidade[NUMERO_CLIENTES][NUMERO_RECURSOS];

pthread_mutex_t mutex_banco; // Mutex para dados compartilhados

// Funcao para verificar se o estado atual é seguro
bool verifica_seguranca() {
    int trabalho[NUMERO_RECURSOS];
    bool finalizado[NUMERO_CLIENTES];

    for (int i = 0; i < NUMERO_RECURSOS; i++) {
        trabalho[i] = disponivel[i];
    }
    for (int i = 0; i < NUMERO_CLIENTES; i++) {
        finalizado[i] = false;
    }

    int count = 0;
    while (count < NUMERO_CLIENTES) {
        bool achou = false;
        for (int i = 0; i < NUMERO_CLIENTES; i++) {
            if (!finalizado[i]) {
                bool pode_rodar = true;
                for (int j = 0; j < NUMERO_RECURSOS; j++) {
                    if (necessidade[i][j] > trabalho[j]) {
                        pode_rodar = false;
                        break;
                    }
                }

                if (pode_rodar) {
                    for (int j = 0; j < NUMERO_RECURSOS; j++) {
                        trabalho[j] += alocado[i][j];
                    }
                    finalizado[i] = true;
                    count++;
                    achou = true;
                }
            }
        }
        if (!achou) {
            return false;
        }
    }
    return true;
}

// Funcao para solicitar recursos
int solicitar_recursos(int id_cliente, int pedido[]) {
    pthread_mutex_lock(&mutex_banco);

    // Verifica se pedido <= necessidade
    for (int j = 0; j < NUMERO_RECURSOS; j++) {
        if (pedido[j] > necessidade[id_cliente][j]) {
            printf("Cliente %d: Pedido excede necessidade!\n", id_cliente);
            pthread_mutex_unlock(&mutex_banco);
            return -1;
        }
    }

    // Verifica se pedido <= disponivel
    bool esperar = false;
    for (int j = 0; j < NUMERO_RECURSOS; j++) {
        if (pedido[j] > disponivel[j]) {
            esperar = true;
            break;
        }
    }

    if (esperar) {
        printf("Cliente %d: Recursos indisponiveis, esperando...\n", id_cliente);
        pthread_mutex_unlock(&mutex_banco);
        return -1; // Nao pode atender agora
    }

    // Tenta alocar (simulacao)
    for (int j = 0; j < NUMERO_RECURSOS; j++) {
        disponivel[j] -= pedido[j];
        alocado[id_cliente][j] += pedido[j];
        necessidade[id_cliente][j] -= pedido[j];
    }

    // Verifica se o novo estado é seguro
    if (verifica_seguranca()) {
        printf("Cliente %d: Pedido ATENDIDO.\n", id_cliente);
        // Estado é seguro, mantem a alocacao
        pthread_mutex_unlock(&mutex_banco);
        return 0; // Sucesso
    } else {
        printf("Cliente %d: Pedido NEGADO (inseguro).\n", id_cliente);
        // Estado inseguro, desfaz a alocacao
        for (int j = 0; j < NUMERO_RECURSOS; j++) {
            disponivel[j] += pedido[j];
            alocado[id_cliente][j] -= pedido[j];
            necessidade[id_cliente][j] += pedido[j];
        }
        pthread_mutex_unlock(&mutex_banco);
        return -1; // Falha (inseguro)
    }
}

// Funcao para liberar recursos
int liberar_recursos(int id_cliente, int liberacao[]) {
    pthread_mutex_lock(&mutex_banco);

    // Verifica se pode liberar (liberacao <= alocado)
    bool erro = false;
     for (int j = 0; j < NUMERO_RECURSOS; j++) {
         if (liberacao[j] > alocado[id_cliente][j]) {
             printf("Cliente %d: Erro ao liberar recurso %d!\n", id_cliente, j);
             erro = true;
             break; // Nao precisa checar mais nada
         }
     }

     if (erro) {
         pthread_mutex_unlock(&mutex_banco);
         return -1;
     }

    // Libera os recursos
    printf("Cliente %d liberando recursos...\n", id_cliente);
    for (int j = 0; j < NUMERO_RECURSOS; j++) {
        disponivel[j] += liberacao[j];
        alocado[id_cliente][j] -= liberacao[j];
        necessidade[id_cliente][j] += liberacao[j];
    }

    pthread_mutex_unlock(&mutex_banco);
    return 0; // Sucesso
}

// Rotina executada por cada thread cliente
void* rotina_cliente(void* arg) {
    int id_cliente = *(int*)arg;
    free(arg); // Libera memoria do ID

    int pedido[NUMERO_RECURSOS];
    int liberacao[NUMERO_RECURSOS];

    while (true) {
        // Gera um pedido aleatorio (ate a necessidade)
        bool precisa_algo = false;
        pthread_mutex_lock(&mutex_banco); // Leitura segura da necessidade
        for (int j = 0; j < NUMERO_RECURSOS; j++) {
            if (necessidade[id_cliente][j] > 0) {
                precisa_algo = true;
                pedido[j] = rand() % (necessidade[id_cliente][j] + 1);
            } else {
                pedido[j] = 0;
            }
        }
        pthread_mutex_unlock(&mutex_banco);

        bool pediu_algo = false;
        for(int j=0; j<NUMERO_RECURSOS; ++j) if(pedido[j] > 0) pediu_algo = true;

        if (precisa_algo && pediu_algo) {
            printf("Cliente %d tentando solicitar...\n", id_cliente);
            if (solicitar_recursos(id_cliente, pedido) == 0) {
                // Sucesso, simula uso
                printf("Cliente %d usando recursos...\n", id_cliente);
                sleep((rand() % 2) + 1); // Usa por 1 ou 2 seg

                // Gera liberacao aleatoria (ate o alocado)
                bool alocou_algo = false;
                 pthread_mutex_lock(&mutex_banco); // Leitura segura do alocado
                for (int j = 0; j < NUMERO_RECURSOS; j++) {
                     if (alocado[id_cliente][j] > 0) {
                        alocou_algo = true;
                        liberacao[j] = rand() % (alocado[id_cliente][j] + 1);
                    } else {
                        liberacao[j] = 0;
                    }
                }
                 pthread_mutex_unlock(&mutex_banco);

                bool libera_algo = false;
                for(int j=0; j<NUMERO_RECURSOS; ++j) if(liberacao[j] > 0) libera_algo = true;

                if (alocou_algo && libera_algo) {
                    liberar_recursos(id_cliente, liberacao);
                }

            } else {
                // Falha na solicitacao, espera um pouco
                sleep(1);
            }
        } else if (!precisa_algo) {
             // Cliente nao precisa mais de nada, pode parar ou esperar mais
             // printf("Cliente %d: Nao preciso de mais nada.\n", id_cliente);
             sleep(5);
        }


        // Pequena pausa
        sleep(rand() % 3);
    }
    return NULL;
}


int main(int argc, char* argv[]) {
    if (argc != NUMERO_RECURSOS + 1) {
        fprintf(stderr, "Uso: %s <R0> <R1> ...\n", argv[0]);
        return 1;
    }

    // Inicia disponivel
    for (int i = 0; i < NUMERO_RECURSOS; i++) {
        disponivel[i] = atoi(argv[i + 1]);
        if (disponivel[i] < 0) {
             fprintf(stderr, "Valor de recurso nao pode ser negativo.\n");
             return 1;
        }
    }

    srand(time(NULL)); // Seed para numeros aleatorios

    // Inicia maximo, alocado e necessidade
    for (int i = 0; i < NUMERO_CLIENTES; i++) {
        for (int j = 0; j < NUMERO_RECURSOS; j++) {
            // Maximo aleatorio (ate o disponivel inicial)
             maximo[i][j] = rand() % (disponivel[j] + 1);
             alocado[i][j] = 0;
             necessidade[i][j] = maximo[i][j];
        }
    }

     // Mostra estado inicial (opcional)
     printf("--- Estado Inicial ---\n");
     printf("Disponivel: [");
     for(int j=0; j<NUMERO_RECURSOS; ++j) printf("%d ", disponivel[j]);
     printf("]\n");
     printf("Necessidade:\n");
     for(int i=0; i<NUMERO_CLIENTES; ++i) {
         printf(" C%d: [", i);
         for(int j=0; j<NUMERO_RECURSOS; ++j) printf("%d ", necessidade[i][j]);
         printf("]\n");
     }
     printf("---------------------\n\n");


    // Inicia mutex
    pthread_mutex_init(&mutex_banco, NULL);

    // Cria threads
    pthread_t threads[NUMERO_CLIENTES];
    for (int i = 0; i < NUMERO_CLIENTES; i++) {
        int* id = malloc(sizeof(int));
        *id = i;
        if (pthread_create(&threads[i], NULL, rotina_cliente, id) != 0) {
            perror("Erro ao criar thread");
            return 1;
        }
         printf("Cliente %d criado.\n", i);
    }

    // Espera threads (loop infinito, precisa de Ctrl+C para parar)
    for (int i = 0; i < NUMERO_CLIENTES; i++) {
        pthread_join(threads[i], NULL);
    }

    // Limpeza (nunca alcancado)
    pthread_mutex_destroy(&mutex_banco);

    return 0;
}