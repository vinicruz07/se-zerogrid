#!/usr/bin/env bash
# Script Bash para verificar e instalar dependências do ecossistema SE-ZeroGrid (Linux/macOS)
# 1. Compilador C (GCC)
# 2. Node.js & npm
# 3. Python 3

set -e

echo "======================================================="
echo "   VERIFICADOR E INSTALADOR DE PRÉ-REQUISITOS          "
echo "               PROJETO SE-ZEROGRID                     "
echo "======================================================="

HAS_GCC=0
HAS_NODE=0
HAS_PYTHON=0

if command -v gcc >/dev/null 2>&1; then
    HAS_GCC=1
    echo "  [OK] Compilador C (GCC): $(gcc --version | head -n1)"
else
    echo "  [FALTANDO] Compilador C (GCC): Não localizado."
fi

if command -v node >/dev/null 2>&1 && command -v npm >/dev/null 2>&1; then
    HAS_NODE=1
    echo "  [OK] Node.js & npm: Node $(node -v), npm $(npm -v)"
else
    echo "  [FALTANDO] Node.js / npm: Não localizado."
fi

if command -v python3 >/dev/null 2>&1; then
    HAS_PYTHON=1
    echo "  [OK] Python 3: $(python3 --version)"
elif command -v python >/dev/null 2>&1; then
    HAS_PYTHON=1
    echo "  [OK] Python 3: $(python --version)"
else
    echo "  [FALTANDO] Python 3: Não localizado."
fi

if [ $HAS_GCC -eq 1 ] && [ $HAS_NODE -eq 1 ] && [ $HAS_PYTHON -eq 1 ]; then
    echo ""
    echo "======================================================="
    echo " [SUCESSO] Todas as dependências já estão instaladas!"
    echo "======================================================="
    echo "Você já pode executar o ecossistema com:"
    echo "  ./scripts/run_local.sh"
    echo ""
    exit 0
fi

echo ""
echo "Instalando pacotes faltantes via gerenciador do sistema..."

if command -v apt-get >/dev/null 2>&1; then
    sudo apt-get update
    [ $HAS_GCC -eq 0 ] && sudo apt-get install -y build-essential
    [ $HAS_NODE -eq 0 ] && sudo apt-get install -y nodejs npm
    [ $HAS_PYTHON -eq 0 ] && sudo apt-get install -y python3 python3-pip
elif command -v dnf >/dev/null 2>&1; then
    [ $HAS_GCC -eq 0 ] && sudo dnf install -y gcc
    [ $HAS_NODE -eq 0 ] && sudo dnf install -y nodejs npm
    [ $HAS_PYTHON -eq 0 ] && sudo dnf install -y python3
elif command -v pacman >/dev/null 2>&1; then
    [ $HAS_GCC -eq 0 ] && sudo pacman -Sy --noconfirm base-devel
    [ $HAS_NODE -eq 0 ] && sudo pacman -Sy --noconfirm nodejs npm
    [ $HAS_PYTHON -eq 0 ] && sudo pacman -Sy --noconfirm python
elif command -v brew >/dev/null 2>&1; then
    [ $HAS_GCC -eq 0 ] && brew install gcc
    [ $HAS_NODE -eq 0 ] && brew install node
    [ $HAS_PYTHON -eq 0 ] && brew install python
else
    echo "Gerenciador de pacotes não suportado automaticamente. Instale gcc, nodejs e python3 manualmente."
    exit 1
fi

echo ""
echo "======================================================="
echo " Instalação finalizada com sucesso!"
echo " Inicie o projeto com: ./scripts/run_local.sh"
echo "======================================================="
