"""
TP3 ETRS923 - script ThingSpeak pret a coller.

Le script matlab/ThingSpeak_MeteoAnalysis.m attend que les poids du reseau,
generes par le notebook dans export/meteo_weights_inline.m, soient colles a
la main entre deux marqueurs. C'est l'etape la plus fragile de la mise en
place : colles au-dessus de "P = struct();", les poids sont effaces ; colles
sous le test "isfield", ils arrivent trop tard.

Ce script fait l'insertion et ecrit export/ThingSpeak_MeteoAnalysis_pret.m :
il ne reste qu'a renseigner les identifiants et les cles dans la section
CONFIGURATION, puis a tout coller dans l'application MATLAB Analysis.

Il verifie aussi que l'altitude du script est celle de la carte (main.h) :
la pression doit etre corrigee de la meme facon des deux cotes, sinon le
Cloud et la carte ne classent pas la meme observation.

Usage :  py -3.12 TP3_IA/preparer_thingspeak.py   (apres le notebook)
"""
import re
from pathlib import Path

DOSSIER = Path(__file__).resolve().parent
SCRIPT = DOSSIER / "matlab" / "ThingSpeak_MeteoAnalysis.m"
POIDS = DOSSIER / "export" / "meteo_weights_inline.m"
MAIN_H = DOSSIER.parent / "Core" / "Inc" / "main.h"
SORTIE = DOSSIER / "export" / "ThingSpeak_MeteoAnalysis_pret.m"

DEBUT = "% ---- début du collage ----"
FIN = "% ---- fin du collage ----"


def lire(chemin: Path) -> str:
    """Le notebook ecrit avec l'encodage par defaut du systeme : on accepte
    l'UTF-8 et, a defaut, le cp1252 de Windows."""
    brut = chemin.read_bytes()
    try:
        return brut.decode("utf-8")
    except UnicodeDecodeError:
        return brut.decode("cp1252")


def main() -> None:
    script = lire(SCRIPT)
    poids = lire(POIDS).strip()

    if script.count(DEBUT) != 1 or script.count(FIN) != 1:
        raise SystemExit("Marqueurs de collage introuvables dans le script MATLAB")
    if "P.W{" not in poids or "P.mean" not in poids:
        raise SystemExit(f"{POIDS.name} ne contient pas les poids attendus")

    avant, reste = script.split(DEBUT)
    _, apres = reste.split(FIN)
    pret = f"{avant}{DEBUT}\n{poids}\n{FIN}{apres}"

    altitude_c = float(re.search(r"#define\s+ALTITUDE_CARTE_M\s+([\d.]+)f?",
                                 MAIN_H.read_text(encoding="utf-8")).group(1))
    altitude_m = float(re.search(r"ALTITUDE_M\s*=\s*([\d.]+)", pret).group(1))
    if altitude_c != altitude_m:
        raise SystemExit(f"Altitude differente : main.h {altitude_c} m, "
                         f"script MATLAB {altitude_m} m. Les aligner d'abord.")

    SORTIE.write_text(pret, encoding="utf-8")
    nb_couches = len(re.findall(r"(?m)^P\.W\{\d+\}", poids))
    print(f"Ecrit : {SORTIE}")
    print(f"  {nb_couches} couches inserees, altitude {altitude_m:.0f} m identique a main.h")
    print("  Reste a remplir : CANAL_MESURES, CLE_LECTURE, CANAL_RESULTAT,")
    print("                    CLE_ECRITURE, TALKBACK_ID, TALKBACK_CLE")


if __name__ == "__main__":
    main()
