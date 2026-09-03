"""
Loop principal do Sistema Especialista Zero-Grid
"""

import sys
import os
import time

# Adiciona o diretório raiz do expert_system ao sys.path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from config import SIMULATOR_HOST, SIMULATOR_PORT, LOOP_INTERVAL_SECONDS
from comms.c_client import CSimulatorClient
from core.engine import InferenceEngine

def main():
    print("=======================================================")
    print("   SE-ZEROGRID - SISTEMA ESPECIALISTA DE CONTROLE (IA)  ")
    print("=======================================================")
    print(f"[IA] Conectando ao simulador C em {SIMULATOR_HOST}:{SIMULATOR_PORT}...")

    client = CSimulatorClient(SIMULATOR_HOST, SIMULATOR_PORT)
    if not client.connect(retries=15, retry_delay=1.0):
        print("[IA - ERRO] Não foi possível conectar ao Simulador em C.")
        return 1

    engine = InferenceEngine()
    print("[IA] Conexão estabelecida! Monitorando telemetria e aplicando regras...\n")

    step_counter = 0

    try:
        while True:
            telemetry = client.receive_telemetry()
            if telemetry is None:
                # Tenta reconectar se a conexão caiu
                print("[IA] Conexão perdida. Tentando reconectar...")
                if not client.connect(retries=5, retry_delay=1.0):
                    time.sleep(2.0)
                    continue
                continue

            # Processa o ciclo de inferência
            decision = engine.process_cycle(telemetry, client)

            step_counter += 1
            if step_counter % 4 == 0:
                p_grid = telemetry.get("p_grid", 0.0)
                p_load = telemetry.get("p_load", 0.0)
                p_pv = telemetry.get("p_pv", 0.0)
                status = telemetry.get("zero_grid_status", "OK")
                strings = telemetry.get("strings", [])
                states = [str(s["state"]) for s in strings]

                status_color = "\033[92m" if status == "OK" else "\033[91m"
                print(f"[IA STATUS] PAC: {p_grid:+6.0f}W | Carga: {p_load:5.0f}W | Solar: {p_pv:5.0f}W | Strings: [{' '.join(states)}] | Zero-Grid: {status_color}{status}\033[0m")

            time.sleep(LOOP_INTERVAL_SECONDS)

    except KeyboardInterrupt:
        print("\n[IA] Encerrando Sistema Especialista...")
    finally:
        client.close()

    return 0

if __name__ == "__main__":
    sys.exit(main())
