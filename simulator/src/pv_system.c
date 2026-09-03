#include "../include/pv_system.h"
#include <stdio.h>
#include <string.h>

void pv_system_init(SistemaFV *sys, int num_strings, double p_nominal_string, double initial_load, double initial_irradiance) {
    if (num_strings > MAX_STRINGS) num_strings = MAX_STRINGS;
    if (num_strings < 1) num_strings = 1;

    sys->num_strings = num_strings;
    sys->irradiance = initial_irradiance;
    sys->timestamp_ms = 0;
    sys->medidores.p_load = initial_load;
    sys->auto_mode = 1; // Padrão: Modo Automático (IA)

    for (int i = 0; i < MAX_STRINGS; i++) {
        sys->strings[i].id = i;
        sys->strings[i].p_nominal = p_nominal_string;
        sys->strings[i].state = (i < num_strings) ? 1 : 0;
        sys->strings[i].last_switch_time = 0;
        sys->strings[i].p_actual = (i < num_strings) ? (p_nominal_string * (initial_irradiance / 1000.0)) : 0.0;
    }

    // Calcula estado inicial das potências
    pv_system_step(sys, 0.0);
}

void pv_system_set_string(SistemaFV *sys, int string_id, int state) {
    if (string_id >= 0 && string_id < sys->num_strings) {
        if (sys->strings[string_id].state != state) {
            sys->strings[string_id].state = (state != 0) ? 1 : 0;
            sys->strings[string_id].last_switch_time = (double)sys->timestamp_ms / 1000.0;
            printf("[SIMULADOR] String #%d %s (Potencia nominal: %.0f W)\n",
                   string_id, sys->strings[string_id].state ? "LIGADA" : "DESLIGADA",
                   sys->strings[string_id].p_nominal);
        }
    }
}

void pv_system_set_load(SistemaFV *sys, double load_watts) {
    if (load_watts < 0.0) load_watts = 0.0;
    sys->medidores.p_load = load_watts;
    printf("[SIMULADOR] Carga alterada para: %.1f W\n", load_watts);
}

void pv_system_set_irradiance(SistemaFV *sys, double irradiance) {
    if (irradiance < 0.0) irradiance = 0.0;
    sys->irradiance = irradiance;
    printf("[SIMULADOR] Irradiancia alterada para: %.1f W/m2\n", irradiance);
}

void pv_system_set_mode(SistemaFV *sys, int auto_mode) {
    sys->auto_mode = (auto_mode != 0) ? 1 : 0;
    printf("[SIMULADOR] Modo alterado para: %s\n", sys->auto_mode ? "AUTOMATICO (IA)" : "MANUAL");
}

void pv_system_set_num_strings(SistemaFV *sys, int num_strings) {
    if (num_strings < 1) num_strings = 1;
    if (num_strings > MAX_STRINGS) num_strings = MAX_STRINGS;

    int old_num = sys->num_strings;
    sys->num_strings = num_strings;

    // Se expandiu o número de strings, garante que as novas estejam configuradas
    double sun_ratio = sys->irradiance / 1000.0;
    for (int i = old_num; i < num_strings; i++) {
        sys->strings[i].id = i;
        sys->strings[i].p_nominal = 2500.0;
        sys->strings[i].state = 1;
        sys->strings[i].last_switch_time = (double)sys->timestamp_ms / 1000.0;
        sys->strings[i].p_actual = sys->strings[i].p_nominal * sun_ratio;
    }

    pv_system_step(sys, 0.0);
    printf("[SIMULADOR] Numero de strings alterado para: %d\n", num_strings);
}

void pv_system_step(SistemaFV *sys, double dt_seconds) {
    sys->timestamp_ms += (long long)(dt_seconds * 1000.0);

    double total_pv = 0.0;
    double sun_ratio = sys->irradiance / 1000.0;

    for (int i = 0; i < sys->num_strings; i++) {
        if (sys->strings[i].state == 1) {
            sys->strings[i].p_actual = sys->strings[i].p_nominal * sun_ratio;
            total_pv += sys->strings[i].p_actual;
        } else {
            sys->strings[i].p_actual = 0.0;
        }
    }

    sys->medidores.p_pv = total_pv;
    sys->medidores.p_grid = total_pv - sys->medidores.p_load;
}

int pv_system_to_json(const SistemaFV *sys, char *buffer, size_t max_len) {
    char strings_json[1024] = "[";
    for (int i = 0; i < sys->num_strings; i++) {
        char item[128];
        snprintf(item, sizeof(item),
                 "{\"id\":%d,\"p_nominal\":%.1f,\"p_actual\":%.1f,\"state\":%d}%s",
                 sys->strings[i].id,
                 sys->strings[i].p_nominal,
                 sys->strings[i].p_actual,
                 sys->strings[i].state,
                 (i < sys->num_strings - 1) ? "," : "");
        strncat(strings_json, item, sizeof(strings_json) - strlen(strings_json) - 1);
    }
    strncat(strings_json, "]", sizeof(strings_json) - strlen(strings_json) - 1);

    const char *status_str = (sys->medidores.p_grid > 50.0) ? "VIOLATION" : "OK";
    const char *mode_str = (sys->auto_mode == 1) ? "AUTO" : "MANUAL";

    int written = snprintf(buffer, max_len,
        "{"
        "\"timestamp\":%lld,"
        "\"p_load\":%.1f,"
        "\"p_pv\":%.1f,"
        "\"p_grid\":%.1f,"
        "\"irradiance\":%.1f,"
        "\"num_strings\":%d,"
        "\"control_mode\":\"%s\","
        "\"strings\":%s,"
        "\"zero_grid_status\":\"%s\""
        "}\n",
        sys->timestamp_ms,
        sys->medidores.p_load,
        sys->medidores.p_pv,
        sys->medidores.p_grid,
        sys->irradiance,
        sys->num_strings,
        mode_str,
        strings_json,
        status_str
    );

    return (written > 0 && (size_t)written < max_len) ? 0 : -1;
}
