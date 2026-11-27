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

## 1.3.0 (session 76, 2026-11-27, release GitHub `v1.3.0` publiée)
- **N. Deuxième plaque en parallèle** : **faite** (demande de l'utilisateur ; choix : plaque B à
  Bandwidth, Tail, Damping, Size propres, modulation, gel et Clear partagés ; bouton Split à
  puissance constante, plaque A × cos, plaque B × sin de Split × 90°). Split 0 (défaut) : sortie
  au bit près celle d'avant (tests inchangés, identité avec CAPS) ; plaque sans entrée calculée
  jusqu'à ce que sa queue reste 50 ms sous -120 dBFS, puis vidée et arrêtée. Préréglage « Two
  plates ». Test 20 ; banc : cas « two » (Split 50 %) 145 ns par trame contre 79 pour « music »
  (× 1,8) ; Split 0 : +5 à 7 % sur la référence 1.2.0 mesurée en même temps (structure à deux
  plaques ; ni la multiplication par l'envoi ni le compteur de silence n'en sont la cause).
  Détail : `LISEZMOI.md` (Réglages, Interface).
- **O. Cadres 1 à 3 plus hauts, vu-mètres agrandis** : **fait** (demande de l'utilisateur) :
  fenêtre 900 × 918 (sections du haut 440 de haut au lieu de 380), vu-mètres 164 × 15 (au lieu
  de 132 × 9) avec une échelle dBFS dessous.
- **P. Retouches de l'interface** : **faites** (demande de l'utilisateur) : plaques nommées A et B
  (1 et 2 se confondaient avec les numéros des sections ; noms et symboles des paramètres
  `bandwidth_b`, `tail_b`, `damping_b`, `size_b`) ; une couleur par plaque (A laiton, B cuivre :
  pastilles, RT60, courbes de même épaisseur, à la place d'un trait fin pour B) ; FREEZE sous Low
  cut et High cut dans OUTPUT, sa place de l'en-tête gardée pour un bouton RANDOM (à venir, comme
  Brimstone et Noise Padifier) ; SYNC et STEREO sous la valeur de leur bouton.
- **Q. Bouton RANDOM** : **fait** (en-tête, à la place de FREEZE ; règle de Noise Padifier, accord
  de l'utilisateur) : tirage uniforme sur la course du bouton de Bandwidth, Tail, Damping, Size des
  deux plaques, Split, Depth, Rate, Pre-delay, Low cut, High cut, Width, Mono bass ; niveaux, Link,
  ducking, Clear, Sync, Stereo input, Freeze et Bypass gardés. Tirage vu en démonstration ; pas
  essayé au clic.

## 1.4.0 (session 78, 2026-11-27, non publiée)
- **R. Smooth (fin de queue sans vaguelettes)** : **fait** (demande de l'utilisateur : ondulation
  de volume en fin de queue ; « diffusion » demandée, B + C accordés).
  - Mesure (C) : `tools/ripple.cpp`, `tools/ripple_measure.hpp` (niveau par 5 ms, écart à une
    parabole sur 1 s, part de la raie la plus forte entre 3 et 90 Hz ; « ondulation périodique » =
    écart RMS × racine de cette part). Original : battements à 33 et 11 Hz, nets sous -40 dB
    (1,3 dB à -40/-80 dB pour Bandwidth 0,5, Tail 0,5, Damping 0,75).
  - Cause : retard de groupe des passe-tout de la boucle (g ≈ 0,72) de 0,16 m à 6,2 m, pic toutes
    les 1/m ; ces fréquences font moins de tours par seconde et survivent : peigne en fin de queue.
    La modulation n'y est pour rien (Depth 0 : même battement). Des passe-tout longs ajoutés
    l'aggraveraient (prototype : gain faible) ; baisser les coefficients le traite.
  - Fait : coefficients 0,723/0,729 → 0,5 et 2 courts passe-tout par retard (± 0,5 s³ ; ouverts
    linéairement, ils aggravaient l'ondulation à mi-course, mesuré) ; Tail compensé par table
    mesurée. À 100 % : ondulation périodique tardive 1,30 → 0,90, 0,93 → 0,56, 1,25 → 0,54 dB
    (test 21) ; à 50 % le gain est surtout pour les queues longues et claires.
  - Prix : queue un peu plus granuleuse ; sous -50 dB elle s'éteint plus vite (RT60 tenu) ; calcul
    +46 % à Smooth > 0 (0,9 % d'un cœur à 44,1 kHz), rien à Smooth 0 (contournement exact).
  - Non fait : réglage à l'oreille de 0,5 et des longueurs ; Smooth dans des préréglages d'usine
    (tous à 0).
