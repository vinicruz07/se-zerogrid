# Script PowerShell para parar os servicos do ecossistema SE-ZeroGrid

Write-Host "=======================================================" -ForegroundColor Yellow
Write-Host "         PARANDO ECOSSISTEMA SE-ZEROGRID              " -ForegroundColor Yellow
Write-Host "=======================================================" -ForegroundColor Yellow

# 1. Encerra simulador C (simulator.exe)
$simProcesses = Get-Process -Name "simulator" -ErrorAction SilentlyContinue
if ($simProcesses) {
    $simProcesses | Stop-Process -Force
    Write-Host "[OK] Simulador C encerrado." -ForegroundColor Green
} else {
    Write-Host "[-] Simulador C nao estava rodando." -ForegroundColor Gray
}

# 2. Encerra processos associados as portas 9001 e 3000
$ports = @(9001, 3000)
foreach ($port in $ports) {
    $conns = Get-NetTCPConnection -LocalPort $port -ErrorAction SilentlyContinue
    if ($conns) {
        $pids = $conns | Select-Object -ExpandProperty OwningProcess -Unique
        foreach ($pidToKill in $pids) {
            try {
                Stop-Process -Id $pidToKill -Force -ErrorAction SilentlyContinue
                Write-Host "[OK] Processo PID $pidToKill (porta $port) finalizado." -ForegroundColor Green
            } catch {}
        }
    }
}

# 3. Encerra processo Python rodando expert_system\main.py
Get-CimInstance Win32_Process -Filter "Name LIKE 'python%.exe'" | Where-Object { $_.CommandLine -like "*expert_system*main.py*" } | ForEach-Object {
    try {
        Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
        Write-Host "[OK] Sistema Especialista Python (PID $($_.ProcessId)) encerrado." -ForegroundColor Green
    } catch {}
}

Write-Host "`n[SUCESSO] Todos os componentes foram finalizados." -ForegroundColor Cyan
