# Script PowerShell para iniciar o ecossistema SE-ZeroGrid completo
# 1. Simulador em C
# 2. Dashboard Node.js / Express
# 3. Sistema Especialista Python

$ErrorActionPreference = "Stop"

$RootDir = Split-Path -Parent $PSScriptRoot
Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host "         INICIALIZANDO ECOSSISTEMA SE-ZEROGRID         " -ForegroundColor Cyan
Write-Host "=======================================================" -ForegroundColor Cyan

# 1. Compilar Simulador C se necessário
$SimExe = Join-Path $RootDir "simulator\simulator.exe"
if (-not (Test-Path $SimExe)) {
    Write-Host "[1/3] Compilando simulador em C com gcc..." -ForegroundColor Yellow
    Push-Location (Join-Path $RootDir "simulator")
    gcc -Wall -O2 -Iinclude src/main.c src/pv_system.c src/ipc_socket.c -lws2_32 -o simulator.exe
    Pop-Location
} else {
    Write-Host "[1/3] Simulador em C já compilado ($SimExe)." -ForegroundColor Green
}

# 2. Instalar dependências do Dashboard Node.js se necessário
$NodeModulesDir = Join-Path $RootDir "dashboard\node_modules"
if (-not (Test-Path $NodeModulesDir)) {
    Write-Host "[2/3] Instalando dependências do Dashboard com npm install..." -ForegroundColor Yellow
    Push-Location (Join-Path $RootDir "dashboard")
    npm install
    Pop-Location
} else {
    Write-Host "[2/3] Dependências do Dashboard já instaladas." -ForegroundColor Green
}

# Configura PATH com Python instalado
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")

Write-Host "`nIniciando serviços..." -ForegroundColor Cyan
Write-Host "  * Simulador C:       TCP 127.0.0.1:9001" -ForegroundColor Gray
Write-Host "  * Dashboard Node.js: http://localhost:3000" -ForegroundColor Gray
Write-Host "  * Sistema Especialista: Motor de Inferência Python" -ForegroundColor Gray
Write-Host "=======================================================" -ForegroundColor Cyan

# Inicia Simulador C
$SimProcess = Start-Process -FilePath $SimExe -WorkingDirectory (Join-Path $RootDir "simulator") -PassThru
Start-Sleep -Seconds 1

# Inicia Dashboard Node.js
$NodeProcess = Start-Process -FilePath "node" -ArgumentList "server.js" -WorkingDirectory (Join-Path $RootDir "dashboard") -PassThru
Start-Sleep -Seconds 1

# Inicia Sistema Especialista Python
$PythonExe = "python"
$DirectPython = "C:\Users\Rambor\AppData\Local\Programs\Python\Python312\python.exe"
if (Test-Path $DirectPython) {
    $PythonExe = $DirectPython
}
$PyScript = Join-Path $RootDir "expert_system\main.py"
$PyProcess = Start-Process -FilePath $PythonExe -ArgumentList $PyScript -WorkingDirectory (Join-Path $RootDir "expert_system") -PassThru

Write-Host "`n[SUCESSO] Todos os 3 componentes foram iniciados com sucesso!" -ForegroundColor Green
Write-Host "PIDs: Simulador C ($($SimProcess.Id)), Dashboard ($($NodeProcess.Id)), IA Python ($($PyProcess.Id))" -ForegroundColor Gray
Write-Host "Acesse o painel em: http://localhost:3000" -ForegroundColor Cyan
