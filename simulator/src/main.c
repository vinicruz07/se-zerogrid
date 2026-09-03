/*
src/main.c
    O projeto simulador de sistema fotovoltaico deverá ter como funcionalidade principal a simulação
de um sistema utilizando inversores antigos sem zero grid, onde todo o excedente é enviado para a
rede elétrica.

    Como o objetivo final do projeto inteiro consiste em manter o sistema fotovoltaico em zero-grid
o simulador devera que considerar o uso de modulos strings para ligar/desligar a fim de manter a corrente
que volta para rede em zero.

===================================================================================
CHECKLIST DO SIMULADOR EM C (Escopo Inicial / MVP - Medidores de Potência & Strings)
===================================================================================

[X] 1. MODELAGEM DE DADOS BÁSICA
    [X] 1.1. Definição da estrutura de String Fotovoltaica (`StringFV`):
             - Identificador (id)
             - Potência nominal / pico (W ou kW)
             - Potência atual gerada (W ou kW)
             - Estado operacional: LIGADA (1) ou DESLIGADA (0)
    [X] 1.2. Definição da estrutura de Medidores de Potência (`MedidoresPAC`):
             - Potência da Carga (P_load): demanda instantânea do consumidor local
             - Potência Solar Total Gerada (P_pv): soma das strings ativas
             - Potência no Ponto de Conexão com a Rede (P_grid = P_pv - P_load)
               * P_grid > 0: Injeção/Excedente na rede (VIOLAÇÃO DO ZERO-GRID)
               * P_grid < 0: Importação da rede (consumo complementar da concessionária)
               * P_grid == 0: Equilíbrio perfeito (Zero-Grid)
    [X] 1.3. Definição do estado global da simulação:
             - Vetor de strings configuradas
             - Tempo decorrido / Passo de tempo discreto (delta_t)

[X] 2. DINÂMICA DA SIMULAÇÃO (MOTOR FÍSICO / TEMPO DISCRETO)
    [X] 2.1. Simulação do perfil de carga (Consumo):
             - Inicialmente simples: valores configuráveis em tempo de execução (degraus de carga)
               ou gerador de carga variável para testar cenários de corte/reconexão.
    [X] 2.2. Simulação da geração solar:
             - Potência disponibilizada pelo sol por string em função do tempo/irradiação.
             - Cálculo da potência efetiva: P_pv_total = Σ (P_string_i * estado_i).
    [X] 2.3. Cálculo do Balanço Energético no PAC (Ponto de Acoplamento Comum):
             - Atualização instantânea a cada ciclo de P_grid = P_pv_total - P_load.

[X] 3. MECANISMO DE ATUAÇÃO (CONTROLE DAS STRINGS)
    [X] 3.1. Função de comando para ligar/desligar string:
             - `set_string_state(int string_id, int state)`
    [X] 3.2. Validação dos comandos:
             - Checagem de limites (id válido, estados válidos 0 ou 1).
    [X] 3.3. [Opcional/Segurança] Temporizador mínimo de comutação (lockout time):
             - Evitar chaveamento excessivo (chattering) das strings na comutação.

[X] 4. INTERFACE DE COMUNICAÇÃO (INTEGRAÇÃO COM IA E DASHBOARD)
    [X] 4.1. Emissão de Telemetria (Saída do Simulador):
             - Enviar dados dos medidores (P_grid, P_load, P_pv, estados das strings, timestamp).
             - Formato padronizado simples (ex: JSON ou strings formatadas) via Socket (TCP/UDP)
               ou IPC compartilhado.
    [X] 4.2. Recepção de Comandos (Entrada do Simulador):
             - Escuta de comandos enviados pelo Sistema Especialista (IA) para alterar estados
               das strings (`SET_STRING <id> <0|1>` ou máscara binária).

[X] 5. LOOP PRINCIPAL E MONITORAMENTO (`main.c`)
    [X] 5.1. Inicialização do sistema:
             - Configurar número de strings, carga inicial e canais de comunicação.
    [X] 5.2. Loop de simulação com passo fixo (ex: a cada 100ms ou 1s):
             - Atualizar carga e irradiação solar.
             - Processar comandos pendentes da IA.
             - Recalcular balanço no medidor PAC (P_grid).
             - Transmitir telemetria atualizada.
    [X] 5.3. Logs no Console:
             - Exibição tabular legível das grandezas no terminal para depuração local.
===================================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #define sleep_ms(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define sleep_ms(ms) usleep((ms) * 1000)
#endif

#include "../include/pv_system.h"
#include "../include/ipc_socket.h"

int main(int argc, char *argv[]) {
    printf("=======================================================\n");
    printf("   SE-ZEROGRID - SIMULADOR FISICO FOTOVOLTAICO (C)     \n");
    printf("=======================================================\n");

    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    // Inicializa sistema: 4 strings de 2500W (Total 10kWp), Carga inicial de 4200W, Sol 1000 W/m²
    SistemaFV sys;
    pv_system_init(&sys, 4, 2500.0, 4200.0, 1000.0);

    // Inicializa servidor TCP
    if (ipc_server_init(port) != 0) {
        fprintf(stderr, "[ERRO] Nao foi possivel iniciar o servidor TCP na porta %d\n", port);
        return 1;
    }

    printf("[SIMULADOR] Configurado com %d strings de %.0f W cada.\n", sys.num_strings, sys.strings[0].p_nominal);
    printf("[SIMULADOR] Carga inicial: %.0f W | Irradiancia: %.0f W/m2\n", sys.medidores.p_load, sys.irradiance);
    printf("[SIMULADOR] Aguardando conexoes de IA e Dashboard...\n\n");

    const double DT = 0.5; // Passo de 500ms
    int loop_count = 0;
    char json_buffer[2048];

    while (1) {
        // 1. Processa comandos dos clientes conectados (IA e Dashboard)
        ipc_server_poll(&sys);

        // 2. Passo de tempo da simulação física
        pv_system_step(&sys, DT);

        // 3. Serializa telemetria em JSON e transmite para os clientes
        if (pv_system_to_json(&sys, json_buffer, sizeof(json_buffer)) == 0) {
            ipc_server_broadcast(json_buffer);
        }

        // 4. Log periódico no console (a cada 2 passos = 1 segundo)
        loop_count++;
        if (loop_count % 2 == 0) {
            const char *status_str = (sys.medidores.p_grid > 50.0)
                                     ? "VIOLACAO ZERO-GRID (INJECAO)"
                                     : (sys.medidores.p_grid < -50.0 ? "IMPORTANDO DA REDE" : "EQUILIBRIO ZERO-GRID");

            printf("[SIM t=%4.1fs] Sol: %4.0f W/m2 | Carga: %5.0f W | PV: %5.0f W | PAC (Rede): %+6.0f W -> [%s] | Strings: [",
                   (double)sys.timestamp_ms / 1000.0,
                   sys.irradiance,
                   sys.medidores.p_load,
                   sys.medidores.p_pv,
                   sys.medidores.p_grid,
                   status_str);

            for (int i = 0; i < sys.num_strings; i++) {
                printf("%d%s", sys.strings[i].state, (i < sys.num_strings - 1) ? " " : "");
            }
            printf("]\n");
        }

        sleep_ms((int)(DT * 1000.0));
    }

    ipc_server_cleanup();
    return 0;
}
