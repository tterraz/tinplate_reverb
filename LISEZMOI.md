# Tinplate Reverb

Réverbération à plaque, reprise de **`PlateX2`** de CAPS (Tim Goetze, « C* PlateX2 », appelée
**Plate2x2** dans la série 0.4), réécrite en C++ avec DPF, interface NanoVG.
Formats : VST3 et CLAP pour Windows 64 bits ; VST3, CLAP et LV2 pour Linux.
**Licence : GPL 3 ou ultérieure** (`COPYING`), comme CAPS ; DPF est sous ISC (compatible).
Nom provisoire (changer : `DistrhoPluginInfo.h`, `Makefile`, installateur, préférences).

![interface, thème Slate](doc/captures/capture_interface.png)
![interface, thème Tin, bulle d'aide](doc/captures/capture_interface_tin.png)

**Manuel d'utilisation** (français, 12 pages : réglages, préréglages, algorithme avec schémas et
figures) : `doc/manuel.pdf` ; source `doc/manuel.tex` (pdflatex ×3 dans `doc/`), figures
`doc/figures.py` (mêmes formules que le DSP, table des RT60 lue dans `plugin/Rt60Table.hpp`,
réponses impulsionnelles calculées par la plaque elle-même via `tools/impulse.cpp`, compilé
dans `build/` par le script).

## Algorithme
Cœur identique à l'original (`plugin/PlateCore.hpp`, transcription de `PlateStub` de CAPS
0.9.26 et des classes `Delay`, `LP1`, `Sine` qu'il emploie ; plaque de Dattorro, JAES 1997) :
- entrée mono (L + R) / 2 → passe-bas « bandwidth » → 4 passe-tout de diffusion ;
- réservoir de deux moitiés croisées : passe-tout modulé (LFO 1,2 Hz, ±17 échantillons à
  44,1 kHz) → retard → passe-bas « damping » → × decay → passe-tout → retard → × decay ;
- sortie : 6 prises par canal, × 0,6.

Les quatre réglages gardent les plages 0..1 et les lois de l'original (`TinplateParams.hpp`) :
bandwidth a0 = exp(-π(1 - (0,005 + 0,994 p))), decay = 0,749 p, damping a0 =
exp(-π(0,0005 + 0,9995 p)), wet = blend^1,53 et dry = 1 - wet. Valeurs par défaut : celles
qu'un hôte LADSPA donne à PlateX2 (0,5 ; 0,5 ; 0,75 ; 0,25). Les mêmes types flottants que
l'original sont employés opération par opération (échantillons en `float`, certaines sommes en
`double`), y compris la constante « normal » de ±1e-20 ajoutée à l'entrée contre les nombres
dénormaux.

**Identité avec l'original** : avec les ajouts au neutre (préréglage « Original (CAPS) », ou tout
réglage des quatre commandes avec Pre-delay 0, Low cut off, Width 100 %, Output 0 dB, Freeze off),
la sortie est **celle de l'original au bit près**, réglages fixes (test 1 : 28 cas, 7 réglages ×
44,1, 48, 96 et 192 kHz, blocs de tailles variées ; aussi bien en compilation stricte qu'avec les
options de DPF `-O3 -ffast-math`). La référence est le code de CAPS lui-même (`tests/caps/`, sources
d'origine non modifiées), appelé par son descripteur LADSPA comme le ferait un hôte.

Chaîne : (L + R + normal) / 2 → Pre-delay → × entrée (Freeze) → plaque → Low cut → Width →
wet ; sortie = wet × blend^1,53 + sec × (1 - blend^1,53) → Output → fondu du Bypass.
Latence nulle.

Différences voulues avec l'original :
- **lissage** : l'original applique un changement au bloc suivant (bruit de « fermeture éclair »
  sur Tail et Blend : saut de 0,76 sur une sinusoïde de 0,5 quand Blend passe de 0 à 1) ; ici
  chaque coefficient va linéairement à sa nouvelle valeur en 20 ms puis la prend exactement
  (saut 0,025, test 8) ; réglages fixes : aucune différence ;
- **dénormaux** : `run` active aussi FTZ/DAZ et rend à l'hôte son propre état.

## Réglages
| Réglage | Rôle |
|---|---|
| Pre-delay | 0-250 ms avant la plaque ; un changement passe de l'ancien au nouveau retard par un fondu de 30 ms (pas de glissement de hauteur) |
| Bandwidth | passe-bas d'entrée (original) ; affiché : sa fréquence à -3 dB à la fréquence d'échantillonnage courante (« open » s'il reste au-dessus de -3 dB jusqu'à Nyquist) |
| Tail | longueur de la réverbération (original) ; affiché : RT60 à 500 Hz mesuré (voir plus bas) |
| Damping | passe-bas dans la boucle (original) ; affiché : sa fréquence à -3 dB |
| Freeze | réverbération tenue sans fin : entrée de la plaque fermée, decay à 1, damping supprimé (20 ms), profondeur de modulation à 0 (200 ms) ; le signal sec passe toujours |
| Blend | sec/traité (original : wet = blend^1,53) ; à 1 : réverbération seule (départ auxiliaire) |
| Width | largeur stéréo de la réverbération (0 % mono, 200 % côté doublé) |
| Low cut | passe-haut à 12 dB/octave sur la réverbération seule, 20 Hz (off) à 1 kHz, bouton logarithmique |
| Output | gain de toute la sortie, -24..+12 dB |
| Bypass | paramètre Bypass déclaré à l'hôte, fondu de 10 ms |

Les coefficients des filtres d'origine ne dépendent pas de la fréquence d'échantillonnage (les
retards, si) : à 96 kHz, Bandwidth et Damping coupent deux fois plus haut qu'à 48 kHz, comme dans
l'original ; l'interface affiche les fréquences réelles.

**Freeze sans perte** : avec la modulation, l'interpolation linéaire des retards modulés fait perdre
les aigus pendant le gel (-11 dB à 8 kHz, -16 dB à 16 kHz en 10 s) ; profondeur à 0, les retards
sont entiers et la boucle ne perd rien (niveau à ±0,3 dB sur 20 s de 100 Hz à 16 kHz ; test 6 :
+0,01 dB en 10 s). Les entrées pendant le gel n'ont aucun effet (sortie identique).

**Temps de réverbération affiché** (`plugin/Rt60.hpp`) : **table mesurée** sur la réponse
impulsionnelle (`plugin/Rt60Table.hpp`, écrite par `tools/rt60_table.cpp` : 11 Tail × 9 Damping
× 9 octaves de 62,5 Hz à 16 kHz, à 44,1 kHz ; bande RBJ Q = 2, intégrale de Schroeder, pente
-5..-25 dB), interpolée linéairement. Le calcul par le seul gain de boucle (decay⁴ et deux
passages du damping par tour de 0,725 s) donne 20 à 60 % de moins : les passe-tout du réservoir
(coefficients 0,72) résonnent d'eux-mêmes (0,5-0,65 s même à Tail = 0). À une autre fréquence
d'échantillonnage fs, la table est lue à f × 44 100 / fs. Test 7 : valeur affichée = nouvelle
mesure à 10 % près entre les points de la table (7 cas, 44,1 et 96 kHz ; écart maximal 7,6 %).

Préréglages d'usine : Default, Original (CAPS), Vocal plate, Drum plate, Ambience, Dark and
long, Bright and wide, Send (100 % wet) ; réglés à la lecture des lois, **pas à l'oreille**.
Préréglages de l'utilisateur : comme Brimstone (voir « Interface »).

## Interface
900 × 600, redimensionnable (proportions gardées), HiDPI. Trois sections numérotées dans l'ordre
du signal : **1 INPUT** (Pre-delay, Bandwidth), **2 PLATE** (Tail, Damping, Freeze, RT60 à
500 Hz, 125 Hz et 4 kHz ; Tail et Damping grisés pendant le gel), **3 OUTPUT** (Blend, Width,
Low cut, Output ; vu-mètres IN, REVERB, OUT). En bas : **temps de réverbération en fonction de la
fréquence** (laiton, secondes, échelle de 2, 4, 6 ou 8 s selon la courbe) et **tonalité de la
réverbération** (acier, tirets, dB : Bandwidth et Low cut).

Repris de Brimstone tel quel (même comportement, voir `../brimstone_vst3/LISEZMOI.md`) :
- boutons : glisser verticalement (Maj : fin), molette, double-clic ou Ctrl+clic : défaut,
  clic droit : saisie au clavier ; bulle d'aide sur chaque élément (valeur détaillée dans le
  titre : fréquence à -3 dB, RT60, gains sec et traité en dB) ;
- **préréglages** : liste déroulante (FACTORY puis USER, « Save current settings... »,
  suppression par la croix cliquée deux fois, molette sur la boîte fermée) ; fichiers
  `<nom>.preset` dans `%APPDATA%\Tinplate\presets\` ou `~/.config/tinplate/presets/` (clés :
  symboles des paramètres, `bandwidth`, `tail`, `damping`, `blend`, `predelay`, `lowcut`, `width`,
  `output` ; Freeze et Bypass exclus) ;
- **A/B** : deux jeux de réglages, état DPF `ab` enregistré avec le projet ;
- **thèmes** Slate (sombre) et Tin (clair), pastille SLATE | TIN, préférence de l'utilisateur
  dans `tinplate.conf` (`theme=slate|tin`), non enregistrée avec le projet.
Code : `plugin/TinplateUI.cpp`, `TinplatePrefs.hpp`, `TinplateUserPresets.hpp`.

## Compiler
`./build.sh` (Linux, MinGW-w64 `-posix`, git) :
1. clone DPF au commit testé (`4238e1c`) et applique `patches/` (correctif VST3 de Brimstone) ;
2. tests du DSP (deux fois : stricte, options de DPF), des préférences et des préréglages ;
3. Windows → `bin/windows/`, Linux → `bin/linux/` ;
4. installateur Windows (`installer/tinplate.nsi`) si `makensis` est présent
   → `bin/Tinplate-Reverb-1.0.0-setup.exe` (VST3 imposé, CLAP et manuel PDF en option, cochés
   par défaut ; le manuel va dans `Program Files\Tinplate\Tinplate-Reverb-manuel.pdf`, avec un
   raccourci dans le menu Démarrer de tous les utilisateurs). Le PDF n'est pas refait par
   `build.sh` : recompiler `doc/manuel.tex` avant si le manuel a changé.

Installation manuelle sous Windows : copier `bin/windows/tinplate_reverb.vst3` (le dossier entier)
dans `C:\Program Files\Common Files\VST3\`, `tinplate_reverb.clap` dans `...\Common Files\CLAP\`.

Table des RT60 (à refaire si le cœur change) : commandes en tête de `tools/rt60_table.cpp`
(16 s de calcul).

## Vérifications (2026-11-25)
- `tests/test_dsp.cpp` (29 contrôles, deux compilations) : identité au bit avec CAPS (28 cas),
  Bypass exact, Pre-delay (réponse décalée d'exactement le retard), Low cut (-40 dB à 50 Hz pour
  500 Hz), Width (0 % : L = R exactement ; 200 % : côté × 2), Freeze (niveau, entrée ignorée,
  décroissance ensuite), RT60 affiché contre mesuré, lissage de Blend et du Pre-delay,
  préréglages, vu-mètres, état A/B, état du FPU rendu.
- `tests/test_prefs.cpp` (7) et `tests/test_user_presets.cpp` (17, sous locale `fr_FR.UTF-8`).
- Validateur VST3 de Steinberg : 47/47 sur le VST3 Linux et sur le VST3 Windows (sous Wine).
- Installateur (NSIS) : installation et désinstallation silencieuses (`/S`) sous Wine dans un
  préfixe neuf : VST3 identique à `bin/windows/`, CLAP, licence, entrée du registre, puis tout
  retiré.
  Refait avec le manuel (session 68) : PDF installé identique à `doc/manuel.pdf`, raccourci
  dans `ProgramData\...\Start Menu\Programs\Tinplate Reverb\`, puis tout retiré.
- Interface affichée par la version autonome Windows sous Wine, deux thèmes (captures).
- **Non vérifié** : écoute (aucun réglage n'a été jugé à l'oreille) ; chargement dans un vrai
  logiciel audio ; clics et saisie dans un hôte ; installateur graphique et vrai Windows ;
  sortie identique à un binaire de CAPS distribué (compilé avec `-ffast-math` par un autre
  compilateur : écarts d'arrondi possibles) ; source de la série 0.4 (Plate2x2) non obtenue
  (le site de CAPS renvoie une erreur 500) : d'après le journal de CAPS, les plages des
  commandes ont été ramenées à 0..1 et les lois retouchées depuis (0.9.x).

## Pour reprendre
- Dossier créé en session 68 (2026-11-25) à côté de `brimstone_vst3/`, dont il reprend la
  structure (DPF, `build.sh`, préférences, préréglages, A/B, thèmes, installateur, validateur
  `tools/validate_vst3.sh`). Pas encore de dépôt git ni de commit (en attente d'accord).
- Captures : `CXXFLAGS="-DTINPLATE_UI_DEMO=<zone> -DTINPLATE_UI_DEMO_THEME=<0|1>
  [-DTINPLATE_UI_DEMO_PRESET=<n>] [-DTINPLATE_UI_DEMO_FREEZE=1]"`, version autonome
  (`make -C plugin jack BUILD_DIR_SUFFIX=-demo-<x> DPF_TARGET_DIR=../build-demo-<x>/bin` avec les
  variables MinGW de `build.sh`), lancée sous Wine (`WINEPREFIX` dédié), puis `import -window`
  sur la fenêtre « Tinplate: Tinplate Reverb » (`xwininfo -root -tree`). Zones : knobs 0-7
  (ordre de `kKnobs`), bulles 100 + indice de `kZones`.
- Pistes non faites : réglages des préréglages à l'oreille ;
  coupe-haut de la réverbération, modulation réglable, taille de la plaque (les longueurs des
  retards sont celles de l'original).
