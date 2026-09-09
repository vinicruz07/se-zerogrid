#!/usr/bin/env bash
# Script Bash para iniciar os 3 serviços juntos

set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "======================================================="
echo "         INICIALIZANDO ECOSSISTEMA SE-ZEROGRID         "
echo "======================================================="

# Compilar C se necessário
if [ ! -f "$ROOT_DIR/simulator/simulator.exe" ] && [ ! -f "$ROOT_DIR/simulator/simulator" ]; then
    echo "[1/3] Compilando simulador C..."
    cd "$ROOT_DIR/simulator"
    gcc -Wall -O2 -Iinclude src/main.c src/pv_system.c src/ipc_socket.c -lws2_32 -o simulator.exe || gcc -Wall -O2 -Iinclude src/main.c src/pv_system.c src/ipc_socket.c -o simulator
fi

echo "[2/3] Iniciando Simulador C..."
cd "$ROOT_DIR/simulator"
(./simulator.exe 2>/dev/null || ./simulator) &
SIM_PID=$!
sleep 1

echo "[3/3] Iniciando Dashboard Node.js..."
cd "$ROOT_DIR/dashboard"
node server.js &
NODE_PID=$!
sleep 1

echo "[4/4] Iniciando Sistema Especialista Python..."
cd "$ROOT_DIR/expert_system"
python main.py &
PY_PID=$!

echo "Todos os serviços foram iniciados!"
echo "Dashboard disponível em: http://localhost:3000"

wait $SIM_PID $NODE_PID $PY_PID
