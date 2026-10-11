# Baut MadTV.exe: C#-Launcher mit eingebettetem DOSBox (Windows-eigener .NET-4-Compiler).
param(
    [string]$DosboxDir = "C:\Program Files (x86)\DOSBox-0.74-3"
)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$csc  = "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe"

foreach ($f in "DOSBox.exe", "SDL.dll", "SDL_net.dll") {
    if (-not (Test-Path (Join-Path $DosboxDir $f))) { throw "Nicht gefunden: $DosboxDir\$f" }
}

$resArgs = "DOSBox.exe", "SDL.dll", "SDL_net.dll" | ForEach-Object { "/resource:`"$(Join-Path $DosboxDir $_)`",$_" }
$iconArg = @()
if (Test-Path "$root\src\madtv.ico") { $iconArg = "/win32icon:`"$root\src\madtv.ico`"" }

& $csc /nologo /target:winexe /platform:anycpu /optimize+ `
    /out:"$root\MadTV.exe" `
    /reference:System.Windows.Forms.dll `
    @resArgs @iconArg `
    "$root\src\MadTVLauncher.cs"
if ($LASTEXITCODE -ne 0) { throw "Kompilieren fehlgeschlagen" }
Write-Host "Fertig: $root\MadTV.exe"
