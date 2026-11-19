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
#    3. le script lit la console de la carte (COM3) : l'adresse IP obtenue
#       et le resultat du premier envoi vers ThingSpeak ;
#    4. il ouvre la page de la carte dans le navigateur.
#
#  Deux montages possibles :
#    - carte branchee sur la switch de la salle : le PC doit etre sur le meme
#      reseau (branche sur la meme switch) pour voir la page, et le partage
#      de connexion du PC doit etre DESACTIVE ;
#    - carte branchee sur le PC, qui partage son Wi-Fi (ncpa.cpl).
#
#  Fermer Docklight avant de lancer le script (il lit COM3), le rouvrir apres.
#  Prerequis : cavalier JP2 (BOOT1) en position 2, ELF compile par
#  "make PARTIE=5" (les cles ThingSpeak y sont incluses).
# ============================================================================
param(
  [string]$Elf = (Join-Path $PSScriptRoot "..\build\partie5\TP2_NUCLEO_N657.elf"),
  [string]$Ide = "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins",
  [string]$Com = "COM3",
  [switch]$SansNavigateur            # charger sans ouvrir la page
)

$ErrorActionPreference = "Stop"
$Mac = "00-80-E1-2E-61-54"          # adresse MAC de la carte (console : [ETH])

function Etape([string]$texte) { Write-Host ""; Write-Host "== $texte" -ForegroundColor Cyan }
function Info([string]$texte)  { Write-Host $texte }
function Ok([string]$texte)    { Write-Host $texte -ForegroundColor Green }
function Alerte([string]$texte){ Write-Host $texte -ForegroundColor Yellow }
function Erreur([string]$texte){ Write-Host "ERREUR : $texte" -ForegroundColor Red; exit 1 }

# --- 0. Verifications ---------------------------------------------------------
Etape "Verifications"
if (-not (Test-Path $Elf)) { Erreur "programme introuvable : $Elf. Le compiler avec 'make PARTIE=5', ou passer par CubeIDE (voir DEMO.md)." }
$Elf = (Resolve-Path $Elf).Path
Info "programme : $Elf"

$partage = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
           Where-Object { $_.IPAddress -eq "192.168.137.1" }
if ($partage) {
  Info "partage de connexion du PC : ACTIF (montage carte branchee sur le PC)"
  Alerte "  Si la carte et le PC sont sur la switch de la salle, le DESACTIVER (ncpa.cpl > Wi-Fi > Partage)."
} else {
  Info "partage de connexion du PC : inactif (montage carte sur la switch de la salle)"
}

$gdbServeur  = Get-ChildItem $Ide -Recurse -Filter "ST-LINK_gdbserver.exe"    -ErrorAction SilentlyContinue | Select-Object -First 1
$programmeur = Get-ChildItem $Ide -Recurse -Filter "STM32_Programmer_CLI.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
$gdb         = Get-ChildItem $Ide -Recurse -Filter "arm-none-eabi-gdb.exe"    -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not ($gdbServeur -and $programmeur -and $gdb)) { Erreur "outils de STM32CubeIDE introuvables sous $Ide" }

# Console de la carte, ouverte AVANT le chargement et videe : sinon on
# pourrait lire l'adresse IP d'un demarrage precedent restee en memoire.
$port = $null
try {
  $port = New-Object System.IO.Ports.SerialPort $Com, 115200, None, 8, one
  $port.ReadTimeout = 500
  $port.NewLine = "`n"
  $port.Open()
  Start-Sleep -Milliseconds 300
  $port.DiscardInBuffer()
  try { while ($true) { [void]$port.ReadLine() } } catch { }
  Info "console $Com : ouverte"
} catch {
  $port = $null
  Alerte "console $Com : occupee (Docklight ouvert ?). Le chargement marchera, mais l'adresse"
  Alerte "  sera cherchee autrement, et le premier envoi ne sera pas verifie."
}

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
  if ($port) { $port.Close() }
  Erreur "la carte ne repond pas. Verifier le cable USB et le cavalier JP2, debrancher et rebrancher l'USB, relancer."
}
Ok "carte connectee"

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
if ($sortie -notmatch "Start address") {
  Write-Host $sortie
  if ($port) { $port.Close() }
  Erreur "le chargement a echoue"
}
Ok "programme charge et lance"

# --- 3. Adresse de la carte et premier envoi ---------------------------------
Etape "Demarrage de la carte (jusqu'a 60 s)"
$adresse = $null
$envoi = $null
$debut = Get-Date
if ($port) {
  # On lit la console jusqu'au resultat du premier envoi vers ThingSpeak
  while (-not $envoi -and ((Get-Date) - $debut).TotalSeconds -lt 60) {
    try { $ligne = $port.ReadLine().TrimEnd("`r") } catch { continue }
    if ($ligne -match "adresse IP : (\d+\.\d+\.\d+\.\d+)") { $adresse = $Matches[1]; Ok "adresse de la carte : $adresse" }
    elseif ($ligne -match "\[TEST\]") { Info ("autotest      : " + $ligne.Trim()) }
    elseif ($ligne -match "envoi ThingSpeak|Talkback") { $envoi = $ligne.Trim() }
    elseif ($ligne -match "redirige vers") { $envoi = "$envoi | $($ligne.Trim())" }
  }
  # La ligne "redirige vers" suit celle de l'envoi : la lire si elle arrive
  try { $suite = $port.ReadLine(); if ($suite -match "redirige vers") { $envoi = "$envoi | $($suite.Trim())" } } catch { }
  $port.Close()
} else {
  # Sans console : l'adresse se trouve dans la table ARP, si le PC a deja
  # echange avec la carte (montage partage de connexion)
  while (-not $adresse -and ((Get-Date) - $debut).TotalSeconds -lt 60) {
    Start-Sleep -Seconds 3
    $candidats = Get-NetNeighbor -AddressFamily IPv4 -ErrorAction SilentlyContinue |
                 Where-Object { $_.LinkLayerAddress -eq $Mac } | ForEach-Object { $_.IPAddress }
    foreach ($ip in $candidats) {
      try { Invoke-WebRequest "http://$ip/api/etat" -UseBasicParsing -TimeoutSec 2 | Out-Null; $adresse = $ip; break } catch { }
    }
  }
}

if (-not $adresse) {
  Alerte "La carte n'a pas obtenu d'adresse : le reseau ne lui en donne pas (pas de DHCP pour elle)."
  Alerte "  Solution : la brancher sur le PC et activer le partage de connexion (voir DEMO.md)."
  exit 1
}

# --- 4. Diagnostic du premier envoi ------------------------------------------
Etape "Acces a ThingSpeak"
if (-not $envoi)                          { Alerte "premier envoi non vu sur la console : regarder Docklight." }
elseif ($envoi -match "ok en")            { Ok ($envoi) }
elseif ($envoi -match "302|redirige")     { Alerte $envoi; Alerte "  -> le reseau a une page de connexion : brancher la carte sur le PC (partage de connexion)." }
elseif ($envoi -match "DNS")              { Alerte $envoi; Alerte "  -> pas d'acces Internet direct (proxy ?) : brancher la carte sur le PC (partage de connexion)." }
elseif ($envoi -match "connexion")        { Alerte $envoi; Alerte "  -> le reseau bloque le port 80 : brancher la carte sur le PC (partage de connexion)." }
else                                      { Alerte $envoi }

# --- 5. Page web -------------------------------------------------------------
Etape "Tableau de bord"
$joignable = $false
try { Invoke-WebRequest "http://$adresse/api/etat" -UseBasicParsing -TimeoutSec 3 | Out-Null; $joignable = $true } catch { }
if ($joignable) {
  Ok "page de la carte : http://$adresse"
  if (-not $SansNavigateur) { Start-Process "http://$adresse/" }
} else {
  Alerte "La carte a l'adresse $adresse, mais le PC ne la joint pas : le PC n'est pas sur le meme"
  Alerte "  reseau. Le brancher sur la meme switch que la carte (partage de connexion desactive)."
}

Write-Host ""
Ok "Le premier verdict Cloud peut mettre 2 a 3 minutes (teste le 19/11/2026) : lancer ce"
Ok "script au moins 5 minutes avant la demo. Pour le forcer : ThingSpeak, Apps > MATLAB"
Ok "Analysis > Meteo IA Cloud > Run. Docklight peut etre rouvert maintenant."
