# Script PowerShell para verificar e instalar os pre-requisitos do SE-ZeroGrid
# Pre-requisitos:
# 1. Compilador C (GCC / MinGW)
# 2. Node.js & npm (Dashboard)
# 3. Python 3 (Sistema Especialista)

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host "   VERIFICADOR E INSTALADOR DE PRE-REQUISITOS          " -ForegroundColor Cyan
Write-Host "               PROJETO SE-ZEROGRID                     " -ForegroundColor Cyan
Write-Host "=======================================================" -ForegroundColor Cyan

function Refresh-EnvironmentPath {
    $machinePath = [System.Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath = [System.Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = "$machinePath;$userPath"

    $wingetLinks = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Links"
    if ((Test-Path $wingetLinks) -and ($env:Path -notlike "*$wingetLinks*")) {
        $env:Path = "$wingetLinks;$env:Path"
    }

    $commonGccPaths = @(
        "C:\MinGW\bin",
        "C:\msys64\ucrt64\bin",
        "C:\msys64\mingw64\bin",
        "$env:LOCALAPPDATA\Programs\WinLibs\bin"
    )
    foreach ($p in $commonGccPaths) {
        if ((Test-Path $p) -and ($env:Path -notlike "*$p*")) {
            $env:Path = "$p;$env:Path"
        }
    }
}

Refresh-EnvironmentPath

# 1. Checagem do Compilador C (GCC)
function Test-GccInstalled {
    $gccCmd = Get-Command gcc -ErrorAction SilentlyContinue
    if ($gccCmd) {
        return $true
    }
    $fallbacks = @(
        "C:\MinGW\bin\gcc.exe",
        "C:\msys64\ucrt64\bin\gcc.exe",
        "C:\msys64\mingw64\bin\gcc.exe"
    )
    foreach ($fb in $fallbacks) {
        if (Test-Path $fb) {
            $env:Path = (Split-Path -Parent $fb) + ";" + $env:Path
            return $true
        }
    }
    return $false
}

# 2. Checagem do Node.js & npm
function Test-NodeInstalled {
    $nodeCmd = Get-Command node -ErrorAction SilentlyContinue
    $npmCmd = Get-Command npm -ErrorAction SilentlyContinue
    if ($nodeCmd -and $npmCmd) {
        return $true
    }
    $fallbackNode = "C:\Program Files\nodejs\node.exe"
    if (Test-Path $fallbackNode) {
        $env:Path = (Split-Path -Parent $fallbackNode) + ";" + $env:Path
        return $true
    }
    return $false
}

# 3. Checagem do Python 3
function Test-PythonInstalled {
    $pyCmd = Get-Command python -ErrorAction SilentlyContinue
    if (-not $pyCmd) {
        $pyCmd = Get-Command py -ErrorAction SilentlyContinue
    }
    if ($pyCmd) {
        return $true
    }
    $pyDirs = Get-ChildItem -Path "$env:LOCALAPPDATA\Programs\Python" -Filter "python.exe" -Recurse -ErrorAction SilentlyContinue
    if ($pyDirs) {
        $foundPy = $pyDirs[0].FullName
        $env:Path = (Split-Path -Parent $foundPy) + ";" + $env:Path
        return $true
    }
    return $false
}

Write-Host "`n[1/3] Diagnosticando ambiente atual..." -ForegroundColor Cyan

$hasGcc = Test-GccInstalled
$hasNode = Test-NodeInstalled
$hasPython = Test-PythonInstalled

if ($hasGcc) {
    $gccVer = (& gcc --version 2>$null | Select-Object -First 1)
    Write-Host "  [OK] Compilador C (GCC): Encontrado ($gccVer)" -ForegroundColor Green
} else {
    Write-Host "  [FALTANDO] Compilador C (GCC): Nao localizado." -ForegroundColor Yellow
}

if ($hasNode) {
    $nodeVer = (& node -v 2>$null)
    $npmVer = (& npm -v 2>$null)
    Write-Host "  [OK] Node.js & npm: Encontrado (Node $nodeVer, npm $npmVer)" -ForegroundColor Green
} else {
    Write-Host "  [FALTANDO] Node.js / npm: Nao localizado." -ForegroundColor Yellow
}

if ($hasPython) {
    $pyVer = (& python --version 2>&1)
    if (-not $pyVer) { $pyVer = (& py -3 --version 2>&1) }
    Write-Host "  [OK] Python 3: Encontrado ($pyVer)" -ForegroundColor Green
} else {
    Write-Host "  [FALTANDO] Python 3: Nao localizado." -ForegroundColor Yellow
}

$allInstalled = $hasGcc -and $hasNode -and $hasPython

if ($allInstalled) {
    Write-Host "`n=======================================================" -ForegroundColor Green
    Write-Host " [SUCESSO] Todas as dependencias ja estao instaladas!" -ForegroundColor Green
    Write-Host "=======================================================" -ForegroundColor Green
    Write-Host "Voce ja pode executar o ecossistema diretamente com:" -ForegroundColor Cyan
    Write-Host "  .\scripts\run_all.ps1`n" -ForegroundColor White
    exit 0
}

Write-Host "`n[2/3] Identificando gerenciador de pacotes para instalacao..." -ForegroundColor Cyan

$hasWinget = Get-Command winget -ErrorAction SilentlyContinue
$hasChoco  = Get-Command choco -ErrorAction SilentlyContinue
$hasScoop  = Get-Command scoop -ErrorAction SilentlyContinue

if ($hasWinget) {
    Write-Host "  -> Usando Microsoft WinGet (gerenciador nativo do Windows)." -ForegroundColor Green
} elseif ($hasChoco) {
    Write-Host "  -> Usando Chocolatey." -ForegroundColor Green
} elseif ($hasScoop) {
    Write-Host "  -> Usando Scoop." -ForegroundColor Green
} else {
    Write-Host "  [AVISO] Nenhum gerenciador de pacotes automatico (winget, choco ou scoop) foi detectado." -ForegroundColor Red
    Write-Host "  Por favor, faca a instalacao manual dos seguintes pacotes:" -ForegroundColor Yellow
    if (-not $hasGcc) { Write-Host "    - GCC / MinGW: https://winlibs.com ou https://www.msys2.org" -ForegroundColor White }
    if (-not $hasNode) { Write-Host "    - Node.js LTS: https://nodejs.org" -ForegroundColor White }
    if (-not $hasPython) { Write-Host "    - Python 3:    https://www.python.org/downloads (marque 'Add python.exe to PATH')" -ForegroundColor White }
    exit 1
}

Write-Host "`n[3/3] Instalando dependencias ausentes..." -ForegroundColor Cyan

# Instalacao do Compilador C (GCC)
if (-not $hasGcc) {
    Write-Host "`nInstalando Compilador C (GCC / MinGW-w64)..." -ForegroundColor Yellow
    if ($hasWinget) {
        winget install --id BrechtSanders.WinLibs.POSIX.UCRT --exact --source winget --accept-package-agreements --accept-source-agreements --silent
    } elseif ($hasChoco) {
        choco install -y mingw
    } elseif ($hasScoop) {
        scoop install gcc
    }
    Refresh-EnvironmentPath
}

# Instalacao do Node.js e npm
if (-not $hasNode) {
    Write-Host "`nInstalando Node.js LTS e npm..." -ForegroundColor Yellow
    if ($hasWinget) {
        winget install --id OpenJS.NodeJS.LTS --exact --source winget --accept-package-agreements --accept-source-agreements --silent
    } elseif ($hasChoco) {
        choco install -y nodejs-lts
    } elseif ($hasScoop) {
        scoop install nodejs-lts
    }
    Refresh-EnvironmentPath
}

# Instalacao do Python 3
if (-not $hasPython) {
    Write-Host "`nInstalando Python 3..." -ForegroundColor Yellow
    if ($hasWinget) {
        winget install --id Python.Python.3.12 --exact --source winget --accept-package-agreements --accept-source-agreements --silent
    } elseif ($hasChoco) {
        choco install -y python3
    } elseif ($hasScoop) {
        scoop install python
    }
    Refresh-EnvironmentPath
}

# Validacao Final
Refresh-EnvironmentPath

Write-Host "`n=======================================================" -ForegroundColor Cyan
Write-Host "                VALIDACAO DA INSTALACAO                " -ForegroundColor Cyan
Write-Host "=======================================================" -ForegroundColor Cyan

$finalGcc = Test-GccInstalled
$finalNode = Test-NodeInstalled
$finalPython = Test-PythonInstalled

Write-Host "  * GCC (Compilador C): " -NoNewline
if ($finalGcc) { Write-Host "OK" -ForegroundColor Green } else { Write-Host "PENDENTE (Pode requerer reiniciar o terminal)" -ForegroundColor Yellow }

Write-Host "  * Node.js & npm:      " -NoNewline
if ($finalNode) { Write-Host "OK" -ForegroundColor Green } else { Write-Host "PENDENTE (Pode requerer reiniciar o terminal)" -ForegroundColor Yellow }

Write-Host "  * Python 3:           " -NoNewline
if ($finalPython) { Write-Host "OK" -ForegroundColor Green } else { Write-Host "PENDENTE (Pode requerer reiniciar o terminal)" -ForegroundColor Yellow }

Write-Host "`nInstalacao finalizada!" -ForegroundColor Green
Write-Host "Dica: Se algum comando recem-instalado nao for reconhecido de imediato, feche e reabra o terminal para atualizar o PATH." -ForegroundColor Gray
Write-Host "`nAgora voce pode iniciar o ecossistema com:" -ForegroundColor Cyan
Write-Host "  .\scripts\run_all.ps1`n" -ForegroundColor White
