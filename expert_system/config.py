"""
Configurações do Sistema Especialista Zero-Grid
"""

SIMULATOR_HOST = "127.0.0.1"
SIMULATOR_PORT = 9001

# Limiar a partir do qual a injeção na rede é considerada violação (em Watts)
GRID_INJECTION_THRESHOLD = 30.0

# Margem de segurança de histerese para religamento (em Watts)
# Para religar uma string de 2500W, o consumo importado da rede deve ser maior que 2500W + 250W = 2750W
HYSTERESIS_SAFETY_MARGIN = 250.0

# Tempo mínimo (em segundos) entre comutações de uma mesma string (evita chattering/desgaste mecânico)
MIN_SWITCH_LOCKOUT_SECONDS = 2.0

# Intervalo do loop de decisão (em segundos)
LOOP_INTERVAL_SECONDS = 0.5
