"""
TP3 ETRS923 - etape 1 : recuperation des donnees Meteostat.

Telecharge les releves horaires de trois stations parisiennes et les ecrit
dans un CSV, pour que l'entrainement puisse iterer sans retelecharger.

Grandeurs retenues : temperature, humidite relative, pression.
Ce sont les trois parametres que la carte X-NUCLEO-IKS4A1 sait mesurer, ce
qui est impose par le TP3 §3.c (inference a partir des capteurs reels).

Etiquette : COCO, le code de condition meteo de Meteostat.

Usage :  py -3.12 1_donnees.py
"""
from datetime import datetime
from pathlib import Path

import meteostat
from meteostat import Parameter

# Paris Orly, Paris Montsouris, Paris Le Bourget
STATIONS = ["07149", "07156", "07150"]
DEBUT = datetime(2015, 1, 1)
FIN = datetime(2025, 12, 31)

SORTIE = Path(__file__).parent / "meteostat_paris.csv"

# meteostat bloque par defaut les requetes horaires de plus de 3 ans
meteostat.config.block_large_requests = False


def main() -> None:
    print(f"Telechargement : stations {STATIONS}, de {DEBUT:%Y-%m-%d} a {FIN:%Y-%m-%d}")

    ts = meteostat.hourly(
        STATIONS,
        DEBUT,
        FIN,
        parameters=[Parameter.TEMP, Parameter.RHUM, Parameter.PRES, Parameter.COCO],
    )

    df = ts.fetch()
    print(f"  lignes brutes          : {len(df)}")

    # Une ligne n'est exploitable que si les trois entrees ET l'etiquette
    # sont presentes
    df = df.dropna(subset=["temp", "rhum", "pres", "coco"])
    print(f"  lignes completes       : {len(df)}")

    # Le code 0 n'est pas une condition meteo documentee : on l'ecarte
    df = df[df["coco"] > 0]
    print(f"  lignes apres filtrage  : {len(df)}")

    df = df.reset_index()
    df["coco"] = df["coco"].astype(int)
    df.to_csv(SORTIE, index=False)

    print(f"\nEcrit : {SORTIE}")
    print(f"Periode couverte : {df['time'].min()} -> {df['time'].max()}")
    print("\nRepartition des codes COCO :")
    for code, n in df["coco"].value_counts().sort_index().items():
        print(f"  {code:3d} : {n:7d}")


if __name__ == "__main__":
    main()
