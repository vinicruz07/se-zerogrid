#ifndef PV_SYSTEM_H
#define PV_SYSTEM_H

#include <stddef.h>

#define MAX_STRINGS 16

typedef struct {
    int id;                 // Identificador da string (0 a N-1)
    double p_nominal;       // Potência nominal sob STC (ex: 2500.0 W)
    double p_actual;        // Potência atual gerada (W)
    int state;              // 1 = Ligada (Conectada), 0 = Desligada (Desconectada)
    double last_switch_time;// Marca de tempo do último acionamento (para debounce/lockout)
} StringFV;

typedef struct {
    double p_load;          // Potência da carga consumidora local (W)
    double p_pv;            // Potência total gerada pelas strings ativas (W)
    double p_grid;          // Potência no ponto de acoplamento comum: p_pv - p_load (W)
                            // p_grid > 0: Injeção na rede (VIOLAÇÃO)
                            // p_grid < 0: Importação da rede
                            // p_grid == 0: Equilíbrio zero-grid
} MedidoresPAC;

typedef struct {
    int num_strings;
    StringFV strings[MAX_STRINGS];
    double irradiance;      // Irradiação solar (W/m²), nominal = 1000.0
    MedidoresPAC medidores;
    int auto_mode;          // 1 = AUTO (IA atua), 0 = MANUAL (usuário define estados)
    long long timestamp_ms; // Tempo de simulação em ms
} SistemaFV;

// Funções do sistema fotovoltaico
void pv_system_init(SistemaFV *sys, int num_strings, double p_nominal_string, double initial_load, double initial_irradiance);
void pv_system_set_string(SistemaFV *sys, int string_id, int state);
void pv_system_set_load(SistemaFV *sys, double load_watts);
void pv_system_set_irradiance(SistemaFV *sys, double irradiance);
void pv_system_set_mode(SistemaFV *sys, int auto_mode);
void pv_system_set_num_strings(SistemaFV *sys, int num_strings);
void pv_system_step(SistemaFV *sys, double dt_seconds);
int pv_system_to_json(const SistemaFV *sys, char *buffer, size_t max_len);

#endif // PV_SYSTEM_H
