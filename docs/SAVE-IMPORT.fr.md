# Réimporter une sauvegarde modifiée — fork expérimental

## Ce qui est possible

Oui : le protocole utilisé par pwalkerHax contient déjà une écriture EEPROM
(`CMD_EEPROMWRITE`, `0x0A`) et une écriture RAM (`CMD_WRITE`, `0x06`). Le projet
amont s'en sert pour les cadeaux, les watts et les pas. Il ne proposait pas de
restauration générale d'un fichier EEPROM.

Ce fork ajoute un **import sélectif de changements** produits par Pokéwalker Studio :
watts, pas cumulés, jours cumulés, trois Pokémon capturés, trois objets trouvés,
et dix cadeaux de rencontres. Il ne flashe pas les 64 Kio d'un fichier arbitraire.
Il conserve l'appairage, l'identité, la calibration, la route et les graphismes.

Les Pokémon et objets sont des données que le jeu DS pourra ensuite lire au
retour de promenade. Leurs combinaisons ne sont pas validées comme légales dans
HeartGold/SoulSilver. Le compagnon de promenade et les événements spéciaux ne
sont pas édités par cette version. Les routes débloquées appartiennent au jeu DS.

## Mode d'emploi

1. Mettre le `pwalkerHax.3dsx` de ce fork sur la carte SD et le lancer depuis le
   Homebrew Launcher de la 3DS.
2. Utiliser **Dump EEPROM (user data)**. Le fork crée un nouveau fichier daté dans
   `/3ds/pwalkerHax/dumps/`, sans écraser les dumps précédents. Conserver une copie
   de ce fichier hors de la carte SD. La ROM d'origine n'est pas requise par l'éditeur.
3. Sur le Mac, ouvrir ce dump dans **Pokéwalker Studio**, modifier les champs,
   puis choisir **Exporter pour la 3DS (.pwe)…**. Pour récupérer la progression
   de PokeStroller, ouvrir d'abord le dump du matériel puis utiliser **Importer
   une sauvegarde modifiée…** avec l'EEPROM ou la session de l'émulateur.
4. Copier le résultat sous le nom `/3ds/pwalkerHax/import.pwe` sur la carte SD.
5. Dans pwalkerHax, utiliser **Preview save edit** pour lire les changements.
   Cette commande ne démarre pas l'IR et n'écrit rien sur le Pokéwalker.
6. Choisir **Import save edit (experimental)**. Après lecture du récapitulatif,
   maintenir **L + R** et presser **X** pour autoriser la sauvegarde puis l'import ;
   **B** annule. Ouvrir **Connexion** sur le Pokéwalker et garder les capteurs alignés.
7. Le programme lit l'EEPROM, vérifie l'appareil, le dresseur, la promenade et les
   anciennes valeurs, écrit une sauvegarde datée dans `/3ds/pwalkerHax/backups/`,
   la relit, vérifie à nouveau l'état distant, puis importe et relit les changements.
   Attendre **IMPORT VERIFIED**. Conserver le fichier de sauvegarde.
8. Pour le premier essai matériel, utiliser une petite variation de watts, refaire
   un dump ensuite et vérifier la valeur. Tester les captures et les objets dans
   un second temps avant le retour vers le jeu DS.

Si une valeur ciblée a changé depuis le dump ou si une nouvelle promenade a été
commencée, l'import est refusé : refaire un dump et les modifications souhaitées.
Après une coupure ou un message **PARTIAL / UNCERTAIN IMPORT**, conserver le backup
et refaire un dump pour comparer. Ne pas réessayer le même fichier à l'aveugle.
Les écritures IR ne constituent pas une transaction atomique ; le backup n'est
pas réinjecté automatiquement. La restauration arbitraire d'un backup complet
n'est volontairement pas une fonction de cette version.

## Cache d'activité et firmware

Les 24 octets d'activité vivent aussi en RAM, à `0xF780`. Écrire uniquement
l'EEPROM laisserait l'ancien cache remplacer les nouvelles valeurs plus tard.
Le fork écrit donc **uniquement les champs d'activité sélectionnés** dans le cache,
puis appelle l'opération `addWatts(0)` déjà utilisée par le projet amont. Le firmware
persiste lui-même le cache et ses deux contrôles d'intégrité. Les autres champs
vivants du cache sont conservés. La sauvegarde préalable contient l'EEPROM lue,
pas un instantané CPU ou une capture de toute la RAM.

Les adresses et l'opération amont sont celles du firmware retail étudié par
[Dmitry Grinberg](https://dmitry.gr/?r=05.Projects&proj=28.%20pokewalker),
vérifié localement avec une ROM de 48 Kio de SHA-256
`f9e210a3b74afbbd12c5a66a51cc05cb9fbac986805ff0a3bfb4be6074d15607`.
Le fork ne lit pas automatiquement la ROM pour identifier une autre version.
Ne pas utiliser l'import d'activité avec un firmware modifié ou non identifié.

## Vérifier depuis un Mac, sans matériel

```sh
clang -std=c11 -O2 -Iinclude source/save_patch.c tools/check-save-edit.c -o /tmp/check-save-edit
/tmp/check-save-edit /chemin/import.pwe /chemin/PWEEPROM.bin
./tests/run-tests.sh
```

Le vérificateur est en lecture seule. Le format `.pwe` est documenté dans
[PWEDIT1.md](PWEDIT1.md). Aucun dump personnel n'est nécessaire à la CI.

## Validation réalisée et limites

- Compilation `.3dsx` avec devkitARM 16.1.0 et les bibliothèques 3DS locales.
- Tests ASan/UBSan : parseur, tailles, CRC32, zones autorisées, identité, promenade,
  anciennes valeurs, miroirs, données malformées et échecs du parcours d'import.
- Le vrai parcours d'import est exercé avec un transport simulé : pas d'écriture
  si le backup échoue, refus d'un autre appareil, backup relu avant écriture,
  arrêt sur erreur et sauvegarde conservée.
- Les exports Swift de l'éditeur et l'application C des 19 groupes de champs
  donnent une EEPROM strictement identique sur un jeu de données synthétique.
- Avec les dumps personnels lus à leur emplacement d'origine : le code de
  protocole 3DS a dialogué avec le firmware dans PokeStroller, lu les 64 Kio,
  écrit un Pokémon et un objet, réglé les watts à 42 dans la RAM puis l'EEPROM,
  vérifié les deux checksums et conservé la valeur après déconnexion.
- **Pas d'essai sur une 3DS ou un Pokéwalker physique pendant le développement.**
  La couche I²C/IR matérielle, les délais réels, le stockage SD et le retour des
  données vers une cartouche HG/SS restent à valider sur matériel.

Base du fork : `francesco265/pwalkerHax`, commit
`bed237a67550ff5eda2f78a0959b58dfad725416`. Le code du fork reste sous GPL-3.0.
Aucun dump, fichier de changements personnel ni ROM n'est distribué.
