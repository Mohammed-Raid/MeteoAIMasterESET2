# ============================================================================
#  demo.ps1 - charge la partie 5 sur la carte et ouvre son tableau de bord.
#
#  Usage, dans PowerShell, depuis le dossier du projet :
#      powershell -ExecutionPolicy Bypass -File outils\demo.ps1
#
#  La carte n'a pas de Flash interne : le programme est charge en RAM et
#  disparait hors tension. Ce script le recharge comme le fait CubeIDE :
#    1. un serveur GDB ST-LINK neuf, connecte sous reset (-k), sur le port
#       d'acces 1 du coeur (-m 1), indispensable sur le STM32N6 ;
#    2. GDB charge l'ELF, pointe VTOR sur la table des vecteurs en RAM, puis
#       lance le programme ;
#    3. on attend que la carte obtienne une adresse par le partage de
#       connexion du PC, et on ouvre sa page dans le navigateur.
#
#  Prerequis : partage de connexion actif (Ethernet du PC en 192.168.137.1),
#  carte branchee en USB (ST-LINK) et en RJ45 sur le PC, cavalier JP2 (BOOT1)
#  en position 2, ELF compile par "make PARTIE=5".
#  Docklight peut rester ouvert : le chargement passe par le ST-LINK.
# ============================================================================
param(
  [string]$Elf = (Join-Path $PSScriptRoot "..\build\partie5\TP2_NUCLEO_N657.elf"),
  [string]$Ide = "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins",
  [switch]$SansNavigateur            # charger sans ouvrir la page
)

$ErrorActionPreference = "Stop"
$Mac = "00-80-E1-2E-61-54"          # adresse MAC de la carte (console : [ETH])

function Etape([string]$texte) { Write-Host ""; Write-Host "== $texte" -ForegroundColor Cyan }
function Erreur([string]$texte) { Write-Host "ERREUR : $texte" -ForegroundColor Red; exit 1 }

# --- 0. Verifications ---------------------------------------------------------
Etape "Verifications"
if (-not (Test-Path $Elf)) { Erreur "programme introuvable : $Elf. Le compiler avec 'make PARTIE=5', ou passer par CubeIDE (voir DEMO.md)." }
$Elf = (Resolve-Path $Elf).Path
Write-Host "programme : $Elf"

$ipPc = Get-NetIPAddress -InterfaceAlias Ethernet -AddressFamily IPv4 -ErrorAction SilentlyContinue |
        Where-Object { $_.IPAddress -eq "192.168.137.1" }
if ($ipPc) { Write-Host "partage de connexion : actif (192.168.137.1)" -ForegroundColor Green }
else {
  Write-Host "partage de connexion : PAS ACTIF. ncpa.cpl > clic droit Wi-Fi > Proprietes > Partage," -ForegroundColor Yellow
  Write-Host "  cocher la case, choisir Ethernet. La carte tournera, mais sans adresse ni page web." -ForegroundColor Yellow
}

$gdbServeur = Get-ChildItem $Ide -Recurse -Filter "ST-LINK_gdbserver.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
$programmeur = Get-ChildItem $Ide -Recurse -Filter "STM32_Programmer_CLI.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
$gdb = Get-ChildItem $Ide -Recurse -Filter "arm-none-eabi-gdb.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not ($gdbServeur -and $programmeur -and $gdb)) { Erreur "outils de STM32CubeIDE introuvables sous $Ide" }

# --- 1. Serveur GDB neuf ------------------------------------------------------
Etape "Connexion a la carte (ST-LINK)"
Get-Process ST-LINK_gdbserver -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800
$tmp = Join-Path $env:TEMP "demo_stm32"
New-Item -ItemType Directory -Force $tmp | Out-Null
$journal = Join-Path $tmp "serveur_gdb.txt"
Remove-Item $journal -ErrorAction SilentlyContinue
Start-Process $gdbServeur.FullName -WindowStyle Hidden -RedirectStandardOutput $journal `
  -ArgumentList "-p 61234 -l 1 -d -e -k -m 1 -cp `"$($programmeur.DirectoryName)`"" | Out-Null
$t = 0
while ($t -lt 40 -and -not ((Test-Path $journal) -and (Select-String -Path $journal -Pattern "Waiting for debugger" -Quiet))) {
  Start-Sleep -Milliseconds 500; $t++
}
if (-not ((Test-Path $journal) -and (Select-String -Path $journal -Pattern "Waiting for debugger" -Quiet))) {
  Get-Content $journal -ErrorAction SilentlyContinue | Select-Object -Last 5
  Erreur "la carte ne repond pas. Verifier le cable USB et le cavalier JP2, debrancher et rebrancher l'USB, relancer."
}
Write-Host "carte connectee" -ForegroundColor Green

# --- 2. Chargement --------------------------------------------------------------
Etape "Chargement de la partie 5"
$script = Join-Path $tmp "charger.gdb"
@(
  "set confirm off",
  "set pagination off",
  "target extended-remote localhost:61234",
  "load",
  "set `$vtor = 0x34180400",
  "set *(unsigned int *)0xE000ED08 = `$vtor",
  "set `$sp = *(unsigned int *)`$vtor",
  "set `$pc = *(unsigned int *)(`$vtor + 4)",
  "detach",
  "quit"
) | Set-Content -Path $script -Encoding ASCII
$sortie = & $gdb.FullName --batch -x $script $Elf 2>&1 | Out-String
if ($sortie -notmatch "Start address") { Write-Host $sortie; Erreur "le chargement a echoue" }
Write-Host "programme charge et lance" -ForegroundColor Green

# --- 3. Adresse de la carte et page web --------------------------------------
Etape "Attente de l'adresse de la carte (jusqu'a 60 s)"
$adresse = $null
$debut = Get-Date
while (-not $adresse -and ((Get-Date) - $debut).TotalSeconds -lt 60) {
  Start-Sleep -Seconds 3
  $candidats = Get-NetNeighbor -InterfaceAlias Ethernet -AddressFamily IPv4 -ErrorAction SilentlyContinue |
               Where-Object { $_.LinkLayerAddress -eq $Mac } | ForEach-Object { $_.IPAddress }
  foreach ($ip in $candidats) {
    try {
      Invoke-WebRequest "http://$ip/api/etat" -UseBasicParsing -TimeoutSec 2 | Out-Null
      $adresse = $ip; break
    } catch { }
  }
}
if (-not $adresse) {
  Write-Host "Adresse non trouvee. Lire la ligne 'adresse IP' sur la console (Docklight, COM3, 115200)" -ForegroundColor Yellow
  Write-Host "et l'ouvrir dans le navigateur. Si elle n'apparait pas : partage de connexion pas actif." -ForegroundColor Yellow
  exit 1
}
Write-Host "carte : http://$adresse" -ForegroundColor Green
if (-not $SansNavigateur) { Start-Process "http://$adresse/" }
Write-Host ""
Write-Host "Pret. Le premier verdict Cloud peut mettre 2 a 3 minutes a arriver (teste le 19/11/2026) :" -ForegroundColor Green
Write-Host "lancer ce script au moins 5 minutes avant la demo. Pour le forcer : ThingSpeak," -ForegroundColor Green
Write-Host "Apps > MATLAB Analysis > Meteo IA Cloud > Run." -ForegroundColor Green
