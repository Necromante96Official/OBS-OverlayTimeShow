# Adds required build tool folders to the current user's PATH (no duplicates).

$ErrorActionPreference = 'Stop'

function Add-PathEntry {
    param(
        [string]$Current,
        [string]$Entry
    )
    if (-not $Entry) { return $Current }
    if (-not (Test-Path -LiteralPath $Entry)) { return $Current }

    $normalized = $Entry.TrimEnd('\')
    $parts = @()
    if ($Current) {
        $parts = $Current -split ';' | Where-Object { $_ -and $_.Trim() } | ForEach-Object { $_.TrimEnd('\') }
    }
    foreach ($p in $parts) {
        if ($p -ieq $normalized) {
            return ($parts -join ';')
        }
    }
    if ($parts.Count -eq 0) { return $normalized }
    return (($parts + $normalized) -join ';')
}

$userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
if ($null -eq $userPath) { $userPath = '' }

$candidates = @(
    'C:\Program Files\CMake\bin',
    'C:\Program Files\Git\cmd',
    'C:\Program Files\Git\bin',
    "$env:LOCALAPPDATA\Programs\Python\Python312",
    "$env:LOCALAPPDATA\Programs\Python\Python312\Scripts",
    "$env:LOCALAPPDATA\Programs\Python\Launcher"
)

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    $vswhere = Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe'
}

if (Test-Path $vswhere) {
    $vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null |
        Select-Object -First 1
    if ($vsInstall) {
        $cmakeVs = Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
        $msbuild = Join-Path $vsInstall 'MSBuild\Current\Bin'
        $candidates += @($cmakeVs, $msbuild)
    }
}

$before = $userPath
foreach ($c in $candidates) {
    $userPath = Add-PathEntry -Current $userPath -Entry $c
}

if ($userPath -ne $before) {
    [Environment]::SetEnvironmentVariable('Path', $userPath, 'User')
    Write-Host 'PATH do usuario atualizado.'
} else {
    Write-Host 'PATH do usuario ja estava ok.'
}

# Also refresh this process PATH
$machine = [Environment]::GetEnvironmentVariable('Path', 'Machine')
$env:Path = "$userPath;$machine"
Write-Host 'Pastas garantidas:'
foreach ($c in $candidates) {
    if (Test-Path $c) { Write-Host "  OK  $c" }
}
