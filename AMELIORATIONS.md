# Tinplate Reverb : améliorations proposées

Proposées le 2026-11-26 (après la release `v1.1.0`) ; toutes faites le 2026-11-26 sauf ce qui
revient à l'utilisateur (écoute, réglage des préréglages à l'oreille, vrai hôte) ; validateur
CLAP abandonné (décision). Version 1.2.0 publiée (release GitHub `v1.2.0`, 2026-11-26). Toute livraison
demandera un nouveau numéro de version. Détails de l'existant : `LISEZMOI.md`.

## Son
- **A. Coupe-haut de la réverbération** : **fait** (High cut 1 à 20 kHz, off au maximum, après
  Low cut ; courbe de tonalité ; identité avec CAPS gardée à off).
- **B. Modulation réglable** : **faite** (Depth 0 à 200 %, Rate 0,1 à 5 Hz ; identité à 100 % et
  1,2 Hz ; changement de vitesse sans saut de phase).
- **C. Taille de la plaque** : **faite** (non publiée) : Size 50 à 200 %, durée non compensée
  (1b), glissement de 100 ms (2a), table des RT60 à 50, 100, 150 et 200 % (`LISEZMOI.md`).
- **D. Entrée stéréo** (option) : **faite** (bouton STEREO de la section INPUT) : second jeu de filtres d'entrée, L
  dans la moitié droite du réservoir (qui fait la sortie gauche), R dans la gauche ; rampe de
  20 ms ; source au centre : sortie identique au mono (au bit). Seul le début suit le
  panoramique (60 ms d'un seul côté, puis +2 à +4 dB jusqu'à 0,5 s ; la queue revient au
  centre : structure de la plaque). Test 16, figure `fig_stereo`.
- **E. Grave mono dans la réverbération** : **fait** (section OUTPUT, avec Width) : Mono bass 20 (off) à
  500 Hz, passe-haut de Butterworth 12 dB/octave sur le côté seul (le milieu intact), avant
  Width ; -24 dB sur le côté à 50 Hz pour 200 Hz. Test 17.
- **F.** retirée (décision de l'utilisateur, 2026-11-26).
- **G. Pre-delay calé sur le tempo** : **fait** (bouton SYNC de la section INPUT ; paramètres
  Sync et Note, 13 valeurs de 1/64 à 1/4 rangées par durée ; 120 BPM sans tempo de l'hôte ;
  retard borné à 2 s, mémoire du pré-retard portée à 2 s). Test 18.

## Interface et hôte
- **H. Réglage Mix quand Link est actif** : **fait** (Mix = Blend de CAPS au bit près ; les faders
  en sont l'affichage ; l'hôte automatise Mix).
- **I. Historique** : **fait** (section 6 HISTORY, fenêtre 900 × 858) : crêtes d'entrée et de
  la réverbération, baisse du ducking (ombrée), vidages (traits), lignes Threshold, Floor,
  Trigger ; 3, 7 ou 30 s (non enregistré avec le projet) ; sorties DSP `hist_*`. Test 19.
- **J. Préréglages** utilisant Clear et Link : **faits** (Piano (clear), Snare (clear), Guitar
  (stereo), d'après les lois) ; reste le réglage de tous les préréglages à l'oreille (par
  l'utilisateur).

## Qualité et diffusion
- **K. Banc de CPU** : **fait** (`tests/bench_cpu.cpp`) : 0,55 % d'un cœur à 44,1 kHz, 2,4 % à
  192 kHz ; Clear, Size en glissement, ducking : +20 à +40 % ; silence sans surcoût. Rien à
  optimiser. Cas « options » ajouté (entrée stéréo et Mono bass) : +22 % sur « music » dans la
  même exécution (2026-11-26, 0,39 % d'un cœur à 44,1 kHz ; la machine donnait ce jour-là
  73 ns par trame pour « music », contre 124 ns la veille : comparer dans une même exécution).
- **L. Manuel en anglais** : **fait** (`doc/manual.pdf`, 18 pages, même contenu que
  `manuel.pdf` ; figures `fig_*_en.pdf` par `figures.py` ; les deux dans l'installateur).
- **M. Validation** : validateur CLAP abandonné (décision de l'utilisateur, 2026-11-26) ; reste
  l'essai dans un vrai logiciel audio sous Windows et Linux (par l'utilisateur).
