"""
Máquina de inferência do Sistema Especialista Zero-Grid
"""

import time
from .rules import ZeroGridRules

class InferenceEngine:
    def __init__(self):
        self.last_switch_times = {}
        self.decision_history = []

    def process_cycle(self, telemetry, client):
        """
        Executa um ciclo completo de inferência e atua sobre o simulador C via client TCP.
        No modo MANUAL, a IA respeita o controle do usuário e não executa atuações.
        """
        mode = telemetry.get("control_mode", "AUTO")
        if mode == "MANUAL":
            return None

        action, string_id, reason = ZeroGridRules.evaluate(telemetry, self.last_switch_times)

        if action is not None and string_id is not None:
            now = time.time()
            self.last_switch_times[string_id] = now

            if action == "TURN_OFF":
                success = client.set_string(string_id, 0)
                status_str = "SUCESSO" if success else "FALHA"
                record = {"time": now, "action": action, "string_id": string_id, "reason": reason, "status": status_str}
                self.decision_history.append(record)
                print(f"\033[91m{reason} -> [{status_str}]\033[0m")
                return record

            elif action == "TURN_ON":
                success = client.set_string(string_id, 1)
                status_str = "SUCESSO" if success else "FALHA"
                record = {"time": now, "action": action, "string_id": string_id, "reason": reason, "status": status_str}
                self.decision_history.append(record)
                print(f"\033[92m{reason} -> [{status_str}]\033[0m")
                return record

        return None
