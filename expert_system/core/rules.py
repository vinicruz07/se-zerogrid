"""
Definição declarativa das regras de inferência do Sistema Especialista Zero-Grid
"""

try:
    from config import GRID_INJECTION_THRESHOLD, HYSTERESIS_SAFETY_MARGIN, MIN_SWITCH_LOCKOUT_SECONDS
except (ImportError, ValueError):
    from ..config import GRID_INJECTION_THRESHOLD, HYSTERESIS_SAFETY_MARGIN, MIN_SWITCH_LOCKOUT_SECONDS
import time

class ZeroGridRules:
    @staticmethod
    def evaluate(telemetry, last_switch_times):
        """
        Avalia o conjunto de regras sobre a telemetria atual.
        Retorna uma tupla (acao, string_id, explicacao) ou (None, None, None).
        Acoes possiveis: 'TURN_OFF', 'TURN_ON'
        """
        now = time.time()
        p_grid = telemetry.get("p_grid", 0.0)
        p_load = telemetry.get("p_load", 0.0)
        p_pv = telemetry.get("p_pv", 0.0)
        irradiance = telemetry.get("irradiance", 1000.0)
        strings = telemetry.get("strings", [])

        # -------------------------------------------------------------
        # REGRA 1: CORTE POR INJEÇÃO NA REDE (VIOLAÇÃO DO ZERO GRID)
        # Se P_grid > limiar, a geração solar supera a carga e há energia
        # sendo enviada para a rede pública.
        # -------------------------------------------------------------
        if p_grid > GRID_INJECTION_THRESHOLD:
            # Procura uma string ativa para desligar
            # Seleciona de trás para frente (da última para a primeira) para chaveamento ordenado
            for s in reversed(strings):
                s_id = s["id"]
                if s["state"] == 1:
                    last_time = last_switch_times.get(s_id, 0.0)
                    if (now - last_time) >= MIN_SWITCH_LOCKOUT_SECONDS:
                        motivo = (
                            f"[REGRA 1 - CORTE] Injeção de +{p_grid:.0f}W detectada no PAC (limiar: {GRID_INJECTION_THRESHOLD}W). "
                            f"Desligando String #{s_id} (potência atual: {s['p_actual']:.0f}W) para anular injeção."
                        )
                        return ("TURN_OFF", s_id, motivo)

        # -------------------------------------------------------------
        # REGRA 2: RELIGAMENTO SEGURO COM HISTERESE
        # Se a carga aumentou e estamos importando energia suficiente da rede,
        # podemos religar uma string desligada SEM violar o zero grid.
        # Condição: Importação (-p_grid) > (P_estimada_da_string + Margem de histerese)
        # -------------------------------------------------------------
        elif p_grid < 0:
            import_power = -p_grid
            sun_factor = max(0.0, irradiance / 1000.0)

            for s in strings:
                s_id = s["id"]
                if s["state"] == 0:
                    last_time = last_switch_times.get(s_id, 0.0)
                    if (now - last_time) >= MIN_SWITCH_LOCKOUT_SECONDS:
                        # Estima quanto a string gerará se for religada
                        p_estimated = s["p_nominal"] * sun_factor
                        threshold_needed = p_estimated + HYSTERESIS_SAFETY_MARGIN

                        if import_power >= threshold_needed:
                            motivo = (
                                f"[REGRA 2 - RELIGAMENTO] Importando {import_power:.0f}W da rede. "
                                f"String #{s_id} gerará ~{p_estimated:.0f}W (necessário: {threshold_needed:.0f}W c/ histerese). "
                                f"Religando String #{s_id} com segurança."
                            )
                            return ("TURN_ON", s_id, motivo)

        # Nenhuma regra de corte ou reconexão precisou ser disparada (estado estável)
        return (None, None, None)
