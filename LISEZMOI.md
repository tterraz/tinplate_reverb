# Tinplate Reverb

Réverbération à plaque, reprise de **`PlateX2`** de CAPS (Tim Goetze, « C* PlateX2 », appelée
**Plate2x2** dans la série 0.4), réécrite en C++ avec DPF, interface NanoVG.
Formats : VST3 et CLAP pour Windows 64 bits ; VST3, CLAP et LV2 pour Linux.
**Licence : GPL 3 ou ultérieure** (`COPYING`), comme CAPS ; DPF est sous ISC (compatible).
Nom provisoire (changer : `DistrhoPluginInfo.h`, `Makefile`, installateur, préférences).

![interface, thème Slate](doc/captures/capture_interface.png)
![interface, thème Tin, bulle d'aide](doc/captures/capture_interface_tin.png)

**Manuels d'utilisation** (18 pages : réglages, préréglages, algorithme avec schémas et
figures) : français `doc/manuel.pdf` (source `doc/manuel.tex`), anglais `doc/manual.pdf`
(`doc/manual.tex`, même contenu, à tenir à jour ensemble) ; pdflatex ×3 dans `doc/` ; figures
`doc/figures.py` (`fig_*.pdf` en français, `fig_*_en.pdf` en anglais ; mêmes formules que le DSP,
table des RT60 lue dans `plugin/Rt60Table.hpp`, réponses impulsionnelles calculées par la plaque
elle-même via `tools/impulse.cpp`, compilé dans `build/` par le script ; argument `left` :
entrée stéréo, impulsion à gauche).

## Algorithme
Cœur identique à l'original (`plugin/PlateCore.hpp`, transcription de `PlateStub` de CAPS
0.9.26 et des classes `Delay`, `LP1`, `Sine` qu'il emploie ; plaque de Dattorro, JAES 1997) :
- entrée mono (L + R) / 2 → passe-bas « bandwidth » → 4 passe-tout de diffusion ;
- réservoir de deux moitiés croisées : passe-tout modulé (LFO 1,2 Hz, ±17 échantillons à
  44,1 kHz) → retard → passe-bas « damping » → × decay → passe-tout → retard → × decay ;
- sortie : 6 prises par canal, × 0,6.

Les trois réglages de la plaque gardent les plages 0..1 (Tail : 0..1,16 depuis 1.1.0, même loi
au-delà de 1) et les lois de l'original (`TinplateParams.hpp`) :
bandwidth a0 = exp(-π(1 - (0,005 + 0,994 p))), decay = 0,749 p, damping a0 =
exp(-π(0,0005 + 0,9995 p)). Valeurs par défaut : celles qu'un hôte LADSPA donne à PlateX2
(0,5 ; 0,5 ; 0,75). Le Blend de l'original (wet = blend^1,53, dry = 1 - wet) est remplacé en
1.1.0 par deux faders en dB, Dry et Reverb (décision de l'utilisateur). Les mêmes types flottants que
l'original sont employés opération par opération (échantillons en `float`, certaines sommes en
`double`), y compris la constante « normal » de ±1e-20 ajoutée à l'entrée contre les nombres
dénormaux.

**Identité avec l'original** : avec les ajouts au neutre (Pre-delay 0, Low cut off, Width 100 %,
Output 0 dB, Freeze, Duck et Clear off) et Tail ≤ 1, la sortie est **celle de l'original au bit
près** en réverbération seule (Reverb 0 dB, Dry off = Blend 1) et en sec seul (Dry 0 dB, Reverb
off = Blend 0), réglages fixes (test 1 : 32 cas, 7 réglages × 44,1, 48, 96 et 192 kHz, blocs de
tailles variées, en compilation stricte). Mélanges (Blend converti en dB) : écart d'arrondi des
gains, 1,3e-7 de la crête au plus (20 cas) ; options de DPF `-O3 -ffast-math` : < 1e-5. La référence est le code de CAPS lui-même (`tests/caps/`, sources
d'origine non modifiées), appelé par son descripteur LADSPA comme le ferait un hôte.

Chaîne : L, R → Pre-delay → (L + R + normal) / 2 (ou L et R, entrée stéréo) → × entrée (Freeze)
→ plaque → Low cut → High cut → Mono bass et Width → × ducking → wet ; sortie = wet × 10^(Reverb/20) + sec × 10^(Dry/20) → Output → fondu du Bypass
(niveaux à -60 dB : gain 0 ; à 0 dB : exactement 1).
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
| Pre-delay | 0-250 ms avant la plaque, sur les deux canaux (depuis 1.2.0 ; à 0 : mêmes échantillons) ; un changement passe de l'ancien au nouveau retard par un fondu de 30 ms (pas de glissement de hauteur) |
| Sync, Note | (1.2.0, point G) bouton SYNC : pré-retard = valeur de note (`TinplateNote`, 13 valeurs 1/64 à 1/4, pointées D, triolets T, rangées par durée ; paramètre entier, liste énumérée pour l'hôte) au tempo de l'hôte (`DISTRHO_PLUGIN_WANT_TIMEPOS`, `getTimePosition().bbt`, lu à chaque bloc ; 120 BPM sans tempo) ; au plus 2 s (`kPredelayMaxMs`, mémoire en conséquence) ; changement de tempo ou de note : même fondu de 30 ms ; dans l'interface, le bouton Pre-delay devient NOTE (même place ; glisser par crans, molette = une valeur, pas de saisie au clavier) ; test 18 |
| Bandwidth | passe-bas d'entrée (original) ; affiché : sa fréquence à -3 dB à la fréquence d'échantillonnage courante (« open » s'il reste au-dessus de -3 dB jusqu'à Nyquist) |
| Tail | longueur de la réverbération (original jusqu'à 1 : 5,1 s à 500 Hz) ; jusqu'à 1,16 depuis 1.1.0, même loi (decay 0,869 : 10 s à 500 Hz, Damping au minimum) ; affiché : RT60 à 500 Hz mesuré (voir plus bas) |
| Damping | passe-bas dans la boucle (original) ; affiché : sa fréquence à -3 dB |
| Depth, Rate | (point B) profondeur 0-200 % et vitesse 0,1-5 Hz (logarithmique) de la modulation des deux passe-tout modulés ; 100 % et 1,2 Hz : l'original au bit près (1,2 Hz pris en double exact, le paramètre float n'étant pas 1,2) ; Depth multiplie la rampe du gel (200 ms) ; Rate : `Sine::retune` garde la phase (tirée des deux dernières valeurs) ; mémoire pour Depth 200 % ; table des RT60 mesurée à Depth 100 % ; test 15 |
| Size | (après `v1.1.0`, point C d'`AMELIORATIONS.md` ; choix 1b, 2a, puis 50-200 %) taille de la plaque, 50-200 % : retards et passe-tout de la boucle, passe-tout modulés et 12 prises × Size, arrondis (diffuseurs d'entrée inchangés) ; gain de boucle non compensé (durée ≈ × Size) ; changement glissé en 100 ms, lectures interpolées pendant le glissement seulement (`PlateCore::Plate::setSize`) ; 100 % : identité au bit (le pointeur de lecture est calculé depuis celui d'écriture, mêmes valeurs) ; mémoire pour 200 % ; Clear : la plaque vide prend la taille aussitôt (`finishGlide`) ; test 14 |
| Freeze | réverbération tenue sans fin : entrée de la plaque fermée, decay à 1, damping supprimé (20 ms), profondeur de modulation à 0 (200 ms) ; le signal sec passe toujours |
| Dry, Reverb | (1.1.0, à la place de Blend) faders des niveaux du sec et de la réverbération seule, off (-60) à +6 dB ; loi de position par morceaux (off, -40, -24, -12, 0, +6 dB à 0, 0,15, 0,35, 0,55, 0,82, 1) |
| Width | largeur stéréo de la réverbération (0 % mono, 200 % côté doublé) |
| Stereo input | (1.2.0, point D, menu OPTIONS) off : somme mono (l'original) ; on : `PlateCore::processStereo`, second jeu de filtres d'entrée (`input2`), L dans la moitié droite du réservoir (ses prises font l'essentiel de la sortie gauche ; vérifié par mesure, l'inverse donne une sortie à droite), R dans la gauche ; rampe de 20 ms `fStereo` (t : moitié gauche = da + t (db - da), da = diffusion de x + t (L - x)) ; t = 0 en fin de rampe : `input2` vidé et plus calculé ; L = R : sortie identique au mono (au bit) ; effet : 60 ms d'un seul côté puis +2 à +4 dB jusqu'à 0,5 s, la queue revient au centre (structure) ; test 16 |
| Mono bass | (1.2.0, point E, menu OPTIONS) 20 (off) à 500 Hz, logarithmique : passe-haut RBJ 2e ordre (`Cut2::process1`) sur le côté s = (wL - wR)/2 seul, avant Width ; off : contourné (identité au bit) ; test 17 (200 Hz : côté -24 dB à 50 Hz, 0 dB à 2 kHz, milieu intact) |
| Low cut | passe-haut à 12 dB/octave sur la réverbération seule, 20 Hz (off) à 1 kHz, bouton logarithmique |
| High cut | (point A d'`AMELIORATIONS.md`) passe-bas à 12 dB/octave sur la réverbération seule, après Low cut, 1 kHz à 20 kHz (off), logarithmique ; même structure que Low cut (`Cut2`, RBJ en double, contourné à off) ; test 4 (8 kHz : -38 dB pour 1 kHz) |
| Output | gain de toute la sortie, -24..+12 dB (fader depuis 1.1.0) |
| Link, Mix | icône chaîne (liée, dorée) / chaîne brisée au-dessus des faders. Lié (point H) : le paramètre `mix` (0-1) est le Blend de CAPS (wet = mix^1,53, dry = 1 - wet, au bit près), Dry et Reverb ignorés par le DSP ; les faders affichent les niveaux du mix (`showMix`) et, à la souris, écrivent Mix (`target`, `TinplateLaw::mixOfWetGain`, faders ≤ 0 dB). En liant : Mix tiré du fader Dry ; en déliant : Dry et Reverb écrits aux niveaux affichés (pas de saut). L'hôte voit et automatise Mix. Préréglages d'usine : Link off, Mix = leur ancien Blend. Test 13 |
| Bypass | paramètre Bypass déclaré à l'hôte, fondu de 10 ms |
| Duck | (1.1.0) ducking : baisse maximale de la réverbération pendant que l'entrée joue, 0-30 dB (0 = off, défaut) |
| Threshold | seuil du ducking sur la crête de l'entrée, -60..0 dBFS (défaut -30) |
| Attack, Release | constantes de temps du ducking, 1-200 ms (10) et 50-3000 ms (300), boutons logarithmiques |
| Clear | (1.1.0) chaque nouvelle attaque vide la plaque (défaut : éteint) |
| Floor, Trigger, Fade, Window | seuil d'armement -80..-20 dBFS (-50), seuil d'attaque -60..0 dBFS (-20), fondu de l'ancienne plaque 10-500 ms (60, logarithmique), fenêtre de détection 1-200 ms (20, logarithmique) |

**Ducking** (1.1.0, `TinplatePlugin.cpp`, `TinplateDuck::targetDb`) : crête de l'entrée
max(|L|, |R|) avant le Pre-delay ; baisse visée = dépassement du seuil en dB, au plus Duck ;
lissée en dB par un filtre du premier ordre (Attack à la montée, Release à la descente) ; gain
10^(-c/20) sur la réverbération après Width, avant le mélange ; la plaque n'est pas touchée.
Duck 0 et baisse revenue sous 1e-3 dB : gain non appliqué, sortie au bit près sans ducking
(test 11). Sortie `meter_duck` (baisse en dB) pour le vu-mètre CUT. Pas d'entrée latérale
(sidechain externe écarté, décision de l'utilisateur).

**Ordre des paramètres** (`TinplateParams.hpp`) : plaque (Bandwidth, Tail, Damping, Size, Depth,
Rate), Wet, Dry, Link, Mix, Pre-delay, Low cut, High cut, Width, Output, ducking, Clear, Stereo
input, Mono bass, Sync, Note, puis Freeze et Bypass, puis les sorties (vu-mètres, puis historique
`hist_in`, `hist_wet`, `hist_cut`, `hist_clears`) ; les préréglages (d'usine, de l'utilisateur,
jeux A/B) couvrent `kParamBandwidth` .. `kParamNote`. Les index ont changé depuis 1.1.0 (Size,
Depth, Rate, Link, Mix, High cut insérés) : l'automation d'un projet 1.1.0 se rattache par les
symboles là où l'hôte s'en sert (LV2, CLAP par identifiant : non vérifié). Aucune compatibilité avec
1.0.0 (aucun projet 1.0.0, dit l'utilisateur) : ordre et symboles changés librement.

**Clear** (1.1.0, variante « deux plaques », choisie par l'utilisateur ; optionnel, éteint par
défaut) : `fPlate[2]` ; à une attaque, l'autre plaque (vide) devient courante et reçoit l'entrée
dès cet échantillon ; l'ancienne, entrée fermée, s'éteint en Fade ms (demi-cosinus) puis est
vidée (`activate`) et cesse de calculer. Détection : enveloppe de crête de l'entrée avant le
Pre-delay (montée immédiate, descente 30 ms) ; armée sous Floor **et hors fondu** (pas de
réarmement pendant le fondu, demande de l'utilisateur) ; attaque = dépassement de Trigger moins de
Window ms après celui de Floor, sinon désarmée. Rien pendant le gel (son gelé gardé, décision de
l'utilisateur : pas de remplacement du son gelé). Sortie
`meter_clear` (0 attente, 1 armée, 2 vidée, tenu 200 ms). Test 12 : après le fondu, sortie au bit
près celle d'une plaque neuve nourrie depuis l'attaque ; 2e attaque pendant un fondu de 200 ms
ignorée ; rampe de 500 ms et gel : identiques à Clear éteint ; même rampe avec Window 200 ms :
vidée.

Les coefficients des filtres d'origine ne dépendent pas de la fréquence d'échantillonnage (les
retards, si) : à 96 kHz, Bandwidth et Damping coupent deux fois plus haut qu'à 48 kHz, comme dans
l'original ; l'interface affiche les fréquences réelles.

**Freeze sans perte** : avec la modulation, l'interpolation linéaire des retards modulés fait perdre
les aigus pendant le gel (-11 dB à 8 kHz, -16 dB à 16 kHz en 10 s) ; profondeur à 0, les retards
sont entiers et la boucle ne perd rien (niveau à ±0,3 dB sur 20 s de 100 Hz à 16 kHz ; test 6 :
+0,01 dB en 10 s). Les entrées pendant le gel n'ont aucun effet (sortie identique).

**Temps de réverbération affiché** (`plugin/Rt60.hpp`) : **table mesurée** sur la réponse
impulsionnelle (`plugin/Rt60Table.hpp`, écrite par `tools/rt60_table.cpp` : 4 Size (50, 100,
150, 200 %) × 25 Tail de 0 à 1,2 par pas de 0,05 (points de 1.0.0 identiques) × 9 Damping
× 9 octaves de 62,5 Hz à 16 kHz, à 44,1 kHz ; bande RBJ Q = 2, intégrale de Schroeder, pente
-5..-25 dB), interpolée linéairement. Le calcul par le seul gain de boucle (decay⁴ et deux
passages du damping par tour de 0,725 s) donne 20 à 60 % de moins : les passe-tout du réservoir
(coefficients 0,72) résonnent d'eux-mêmes (0,5-0,65 s même à Tail = 0). À une autre fréquence
d'échantillonnage fs, la table est lue à f × 44 100 / fs. Test 7 : valeur affichée = nouvelle
mesure à 10 % près entre les points de la table (15 cas, 44,1 et 96 kHz, dont 3 avec Tail > 1 et
5 à d'autres tailles ; écart maximal 7,6 % ; Tail 1,16 : 9,87 s mesurés, 9,94 s affichés).
Échelle du graphique : 2 à 32 s.

Préréglages d'usine : Default, Original (CAPS), Vocal plate, Drum plate, Ambience, Dark and
long, Bright and wide, Send (100 % wet), Ducked vocal (1.1.0), Piano (clear), Snare (clear),
Guitar (stereo) (1.2.0, point J : Clear, Link, entrée stéréo, Mono bass, Sync) ; réglés à la
lecture des lois, **pas à l'oreille** (RT60 du manuel calculés par `TinplateRt60::seconds`).
Préréglages de l'utilisateur : comme Brimstone (voir « Interface »).

## Interface
900 × 858 (730 en 1.1.0, 600 en 1.0.0), redimensionnable (proportions gardées), HiDPI. Trois sections numérotées dans l'ordre
du signal : **1 INPUT** (Pre-delay ou NOTE selon le bouton SYNC, Bandwidth), **2 PLATE** (Tail, Damping, Freeze, RT60 à
500 Hz, 125 Hz et 4 kHz ; Tail et Damping grisés pendant le gel), **3 OUTPUT** (Width, Low cut, High cut ;
faders DRY, REVERB, OUTPUT ; vu-mètres IN, REVERB, OUT). Dessous, la bande **4 DUCKING** (Duck, Threshold,
Attack, Release, grisés sauf Duck quand Duck = 0 ; vu-mètre CUT 0-30 dB) et **5 CLEAR** (bouton,
état waiting/ARMED/CLEARED, Floor, Trigger, Fade, Window grisés si éteint). Puis **6 HISTORY**
(1.2.0, point I) : crêtes d'entrée (acier) et de la réverbération après ducking (laiton) sur une
échelle fixe 0 à -80 dBFS, baisse du ducking ombrée depuis le haut, vidages en traits verticaux,
lignes THRESHOLD (à gauche, si Duck > 0), FLOOR et TRIGGER (à droite, si Clear) ; un point toutes
les 25 ms (`uiIdle`, crêtes des ~30 dernières ms données par le DSP, `RecentPeak` de Brimstone ;
vidage = compteur `hist_clears` changé), 30 s gardées, 3/7/30 s affichées (non enregistré avec
le projet). En-tête : bouton **OPTIONS** (point allumé si une option est active) ouvrant un
panneau (Stereo input, bouton Mono bass ; clic dehors ou Échap ferme ; tant qu'il est ouvert,
seuls ses éléments réagissent). En bas : **temps de
réverbération en fonction de la fréquence** (laiton, secondes, échelle de 2 à 12 s selon la courbe) et **tonalité de la
réverbération** (acier, tirets, dB : Bandwidth et Low cut).

Repris de Brimstone tel quel (même comportement, voir `../brimstone_vst3/LISEZMOI.md`) :
- boutons : glisser verticalement (Maj : fin), molette, double-clic ou Ctrl+clic : défaut,
  clic droit : saisie au clavier ; bulle d'aide sur chaque élément (valeur détaillée dans le
  titre : fréquence à -3 dB, RT60, gains sec et traité en dB) ;
- **préréglages** : liste déroulante (FACTORY puis USER, « Save current settings... »,
  suppression par la croix cliquée deux fois, molette sur la boîte fermée) ; fichiers
  `<nom>.preset` dans `%APPDATA%\Tinplate\presets\` ou `~/.config/tinplate/presets/` (clés :
  symboles des paramètres, `bandwidth`, `tail`, `damping`, `size`, `depth`, `rate`, `wet`, `dry`,
  `link`, `mix`, `predelay`, `lowcut`, `highcut`, `width`, `output`, `duck`, `threshold`,
  `attack`, `release`, `clear`, `floor`, `trigger`, `fade`, `window`, `stereo`, `monobass`,
  `sync`, `note` ; Freeze et Bypass exclus) ;
- **A/B** : deux jeux de réglages, état DPF `ab` enregistré avec le projet ;
- **thèmes** Slate (sombre) et Tin (clair, gris légèrement vert « sage » : gris clairs de 1.0.0
  mêlés à 22 % de (155, 175, 155) ; choisi par l'utilisateur parmi tin, pewter, mist, sage), pastille SLATE | TIN, préférence de l'utilisateur
  dans `tinplate.conf` (`theme=slate|tin`), non enregistrée avec le projet.
Code : `plugin/TinplateUI.cpp`, `TinplatePrefs.hpp`, `TinplateUserPresets.hpp`.

## Compiler
`./build.sh` (Linux, MinGW-w64 `-posix`, git) :
1. clone DPF au commit testé (`4238e1c`) et applique `patches/` (correctif VST3 de Brimstone) ;
2. tests du DSP (deux fois : stricte, options de DPF), des préférences et des préréglages ;
3. Windows → `bin/windows/`, Linux → `bin/linux/` ;
4. installateur Windows (`installer/tinplate.nsi`) si `makensis` est présent
   → `bin/Tinplate-Reverb-1.2.0-setup.exe` (VST3 imposé, CLAP et manuels PDF en option, cochés
   par défaut ; les manuels vont dans `Program Files\Tinplate\Tinplate-Reverb-manual.pdf` et
   `...-manuel.pdf`, avec deux raccourcis dans le menu Démarrer de tous les utilisateurs). Les PDF
   ne sont pas refaits par `build.sh` : recompiler `doc/manual.tex` et `doc/manuel.tex` avant si
   les manuels ont changé.

Installation manuelle sous Windows : copier `bin/windows/tinplate_reverb.vst3` (le dossier entier)
dans `C:\Program Files\Common Files\VST3\`, `tinplate_reverb.clap` dans `...\Common Files\CLAP\`.

Table des RT60 (à refaire si le cœur change) : commandes en tête de `tools/rt60_table.cpp`
(4 min 46 s, un fil par taille, 4 × 25 × 9 réponses de 45 s).

## Vérifications (2026-11-25 ; 1.1.0 le 2026-11-26)
- `tests/test_dsp.cpp` (54 contrôles, deux compilations) : identité au bit avec CAPS (32 cas, plus 20 mélanges à l'arrondi),
  Bypass exact, Pre-delay (réponse décalée d'exactement le retard), Low cut (-40 dB à 50 Hz pour
  500 Hz), Width (0 % : L = R exactement ; 200 % : côté × 2), Freeze (niveau, entrée ignorée,
  décroissance ensuite), RT60 affiché contre mesuré, lissage de Dry/Reverb et du Pre-delay, niveau Dry,
  préréglages, vu-mètres, état A/B, état du FPU rendu ; ducking (test 11 : baisse de 12 dB
  tenue, 6 dB pour 6 dB de dépassement (5,4 mesurés : crête par échantillon), attaque à 63 % en
  10 ms, retour au bit près après la baisse, Duck 0 identique) ; Clear (test 12) ; Link (test 13 : Dry meneur en liant, puis
  automation de Reverb seule, sortie au bit près celle des deux niveaux réglés à la main).
- Test 14 (Size) : passage de 100 à 200 % pendant la lecture sans clic (plus grand saut 0,026
  contre 0,028 à taille fixe) ; RT60 à 500 Hz 2,08 s à 100 %, 4,25 s à 200 %.
- Banc de CPU (`tests/bench_cpu.cpp`, hors `build.sh`, commande en tête du fichier ; point K,
  2026-11-26, `taskset -c 2`) : préréglage Default, 124 ns par trame stéréo à 44,1 kHz (0,55 %
  d'un cœur), 127 ns à 192 kHz (2,4 %) ; silence identique (pas de dénormaux) ; Clear avec
  fondus 40 % du temps, Size en glissement permanent, ducking actif : +20 à +40 % (au plus
  178 ns, 3,4 % d'un cœur à 192 kHz). Mesures à ±10 % (bruit d'une exécution à l'autre).
- 1.2.0 (2026-11-26) : `tests/test_dsp.cpp` 72 contrôles par compilation (tests 16 entrée
  stéréo, 17 Mono bass, 18 pré-retard au tempo, 19 sorties de l'historique ; identité au bit
  avec CAPS inchangée : 60 cas, mélanges 1,34e-7 strict, 2,25e-7 en `-ffast-math`) ; préréglages
  de l'utilisateur 17 ; `build.sh` complet ; validateur VST3 47/47 Linux et Windows (sous Wine) ;
  installateur 1.2.0 sous Wine (`/S`, préfixe neuf) : VST3 identique à `bin/windows/`, CLAP, deux
  manuels identiques à `doc/`, deux raccourcis, puis tout retiré ; captures refaites (affichage du
  panneau OPTIONS, de SYNC et de l'historique vu ; clics dans l'hôte non essayés).
- `tests/test_prefs.cpp` (7) et `tests/test_user_presets.cpp` (17, sous locale `fr_FR.UTF-8`).
- Validateur VST3 de Steinberg : 47/47 sur le VST3 Linux et sur le VST3 Windows (sous Wine) ;
  1.1.0 : 47/47 sur les deux (script corrigé pour MinGW comme Noise Padifier :
  `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`).
- Installateur (NSIS) : installation et désinstallation silencieuses (`/S`) sous Wine dans un
  préfixe neuf : VST3 identique à `bin/windows/`, CLAP, licence, entrée du registre, puis tout
  retiré.
  Refait avec le manuel (session 68) : PDF installé identique à `doc/manuel.pdf`, raccourci
  dans `ProgramData\...\Start Menu\Programs\Tinplate Reverb\`, puis tout retiré.
- Interface affichée par la version autonome Windows sous Wine, deux thèmes (captures, refaites
  en 1.1.0 avec le préréglage Ducked vocal ; Tin : Clear allumé, bulle d'aide de Clear,
  `-DTINPLATE_UI_DEMO=112 -DTINPLATE_UI_DEMO_CLEAR=1`).
- **Non vérifié** : écoute (aucun réglage n'a été jugé à l'oreille) ; chargement dans un vrai
  logiciel audio ; clics et saisie dans un hôte ; installateur graphique et vrai Windows ;
  sortie identique à un binaire de CAPS distribué (compilé avec `-ffast-math` par un autre
  compilateur : écarts d'arrondi possibles) ; source de la série 0.4 (Plate2x2) non obtenue
  (le site de CAPS renvoie une erreur 500) : d'après le journal de CAPS, les plages des
  commandes ont été ramenées à 0..1 et les lois retouchées depuis (0.9.x).

## Pour reprendre
- Dossier créé en session 68 (2026-11-25) à côté de `brimstone_vst3/`, dont il reprend la
  structure (DPF, `build.sh`, préférences, préréglages, A/B, thèmes, installateur, validateur
  `tools/validate_vst3.sh`). Dépôt git : `master`, distant `origin` = `git@github.com:tterraz/tinplate_reverb.git` (dépôt GitHub
  renommé de `tinplate` en `tinplate_reverb` le 2026-11-25 ; l'ancienne adresse redirige).
- Captures : `CXXFLAGS="-DTINPLATE_UI_DEMO=<zone> -DTINPLATE_UI_DEMO_THEME=<0|1>
  [-DTINPLATE_UI_DEMO_PRESET=<n>] [-DTINPLATE_UI_DEMO_FREEZE=1] [-DTINPLATE_UI_DEMO_CLEAR=1]
  [-DTINPLATE_UI_DEMO_OPTIONS=1] [-DTINPLATE_UI_DEMO_SYNC=1]"`, version autonome
  (`make -C plugin jack BUILD_DIR_SUFFIX=-demo-<x> DPF_TARGET_DIR=$PWD/build-demo-<x>/bin` avec
  les variables MinGW de `build.sh`), puis `tools/capture_ui.sh build-demo-<x>/bin/tinplate_reverb.exe
  OUT.png` (Wine, préfixe `build/wineprefix`, attend la fenêtre 900 × 858). Zones : -1 aucune,
  knobs (ordre de `kKnobs`), bulles 100 + indice de `kZones`. Historique : synthétique en mode
  démo (`demoHistory`, vidages seulement avec `_CLEAR`). Captures 1.2.0 : Slate = préréglage 8
  (Ducked vocal) ; Tin = préréglage 8, `_CLEAR`, `_OPTIONS`, `_SYNC`.
- **Release GitHub** (procédure ; `V` = numéro) : `./build.sh` complet sur `master` propre ;
  fichiers assemblés dans `build/release/` (non suivi) : installateur
  `bin/Tinplate-Reverb-V-setup.exe`, `Tinplate-Reverb-V-windows.zip` (VST3, CLAP),
  `Tinplate-Reverb-V-linux-x86_64.tar.gz` (VST3, CLAP, LV2), chacun avec `COPYING` et le manuel, et le
  manuel seul `Tinplate-Reverb-manuel.pdf` ; notes en anglais (`build/release/notes-V.md` :
  nouveautés, téléchargements, installation, essais faits et non faits, licence) ; étiquette annotée
  `vV` (auteur Claude) poussée, puis `/usr/bin/gh release create vV --repo tterraz/tinplate_reverb
  --title ... --notes-file notes-V.md <fichiers>` (`gh` hors du PATH, connecté au compte `tterraz`).
  **Une version publiée** : tout changement livré ensuite demande un nouveau numéro (`getVersion`,
  installateur, documents ; choix de l'utilisateur).
  Le nom du greffon reste provisoire : le changer demande aussi de renommer le dépôt GitHub et de
  refaire une release.
- **1.1.0 (2026-11-26, release GitHub `v1.1.0` publiée, notes `build/release/notes-1.1.0.md`)** : ducking interne (Duck, Threshold, Attack, Release,
  vu-mètre CUT, préréglage Ducked vocal), Tail jusqu'à 1,16 (10 s à 500 Hz) et Clear (vidage à
  chaque attaque, deux plaques) ; fenêtre 900 × 730 ; manuel 15 pages (figure `fig_duck.pdf`). Version 1.1.0 dans `getVersion`,
  l'installateur, `README.md`, le manuel. Reste : écoute du ducking et des longues queues,
  vrai hôte. Release et étiquette `v1.0.0` supprimées de GitHub le 2026-11-26
  (demande de l'utilisateur, aucun utilisateur de 1.0.0) : `v1.1.0` est la seule release.
- **Après `v1.1.0` (session 72, non publié)** : C Size, A High cut, K banc CPU, B Depth/Rate,
  H Mix lié.
- **1.2.0 (session 73, 2026-11-26, non publiée, non commitée à la rédaction)** : D entrée stéréo
  et E Mono bass (menu OPTIONS), G pré-retard au tempo (SYNC), I historique (section 6, fenêtre
  900 × 858), J trois préréglages, L manuel anglais (installateur : deux manuels), K cas
  « options ». Version 1.2.0 dans `getVersion`, l'installateur, `README.md`, les manuels.
  Reste : M (validateur CLAP : `clap-validator` absent, binaire à télécharger, non fait),
  ergonomie à revoir avec l'utilisateur, écoute, vrai hôte, release.
- Pistes non faites : réglages des préréglages à l'oreille ;
  autres pistes dans `AMELIORATIONS.md`.
