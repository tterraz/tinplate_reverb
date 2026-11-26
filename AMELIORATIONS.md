# Tinplate Reverb : améliorations proposées

Proposées le 2026-11-26 (après la release `v1.1.0`), en attente de décision. Toute livraison
demandera un nouveau numéro de version. Détails de l'existant : `LISEZMOI.md`.

## Son
- **A. Coupe-haut de la réverbération** : **fait** (High cut 1 à 20 kHz, off au maximum, après
  Low cut ; courbe de tonalité ; identité avec CAPS gardée à off).
- **B. Modulation réglable** : **faite** (Depth 0 à 200 %, Rate 0,1 à 5 Hz ; identité à 100 % et
  1,2 Hz ; changement de vitesse sans saut de phase).
- **C. Taille de la plaque** : **faite** (non publiée) : Size 50 à 200 %, durée non compensée
  (1b), glissement de 100 ms (2a), table des RT60 à 50, 100, 150 et 200 % (`LISEZMOI.md`).
- **D. Entrée stéréo** (option) : L et R entrent chacun dans une moitié de la plaque au lieu de
  la somme mono de l'original : la réverbération suit le panoramique. Off = l'original.
- **E. Grave mono dans la réverbération** : Width appliqué au-dessus d'une fréquence réglable,
  la réverbération reste mono en dessous (mixage plus solide en mono).
- **F.** retirée (décision de l'utilisateur, 2026-11-26).
- **G. Pre-delay calé sur le tempo** de l'hôte (1/64 à 1/4, pointé, triolet), en option.

## Interface et hôte
- **H. Réglage Mix quand Link est actif** : **fait** (Mix = Blend de CAPS au bit près ; les faders
  en sont l'affichage ; l'hôte automatise Mix).
- **I. Historique** (comme Brimstone) : niveau d'entrée, baisse du ducking et vidages de Clear
  sur quelques secondes, pour régler Threshold, Floor et Trigger à l'œil.
- **J. Préréglages** utilisant Clear et Link (piano, guitare, caisse claire), et réglage de tous
  les préréglages à l'oreille (par l'utilisateur).

## Qualité et diffusion
- **K. Banc de CPU** : **fait** (`tests/bench_cpu.cpp`) : 0,55 % d'un cœur à 44,1 kHz, 2,4 % à
  192 kHz ; Clear, Size en glissement, ducking : +20 à +40 % ; silence sans surcoût. Rien à
  optimiser.
- **L. Manuel en anglais** (`doc/manual.pdf`, comme Noise Padifier) : la release est publique
  et le README est en anglais.
- **M. Validation** : validateur CLAP (`clap-validator`) ; essai dans un vrai logiciel audio
  sous Windows et Linux (par l'utilisateur).
