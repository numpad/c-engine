# Unnamed Roguelike

A medieval-themed roguelike deckbuilding game idea.


## Building

The game is being built using C99, OpenGL ES2 [SDL2](https://www.libsdl.org/) and the [flecs](https://www.flecs.dev/flecs/) entity component system.
Currently this game is supported to run on linux and the web using emscripten.
Native support for Windows and Android is a high priority.

```bash
# Linux:
$ make

# WebAssembly:
$ make CC=emcc
```

Afterwards, run the game using `$ ./soil_soldiers` or `$ emrun soil_soldiers.html`, depending on your platform.


## Ideas

This project is very much work-in-progress. Here are some ideas, some great and some bad.


### Lore, Campaign
 
No idea yet.


### Milestones

There are two epic milestones which guide the whole project.

 1. Develop a fun and deep game for myself to play on my phone.
 2. Design and polish the game enough for my friends to (voluntarily) play it, and potentially monetize some content.


#### v0.1  Prototype
 - [x] Basic Map (Rendering, Data)
 - [ ] Map Interactions (Select Block)
 - [ ] Basic Characters (Render, Move)
 - [ ] Simple Cards (Cards in Hand, Play a Card)
 - [ ] Simple Attacks (Melee, Ranged)


#### v0.2  Playable Demo

 - [ ] Save/Load Game
 - [ ] A few Hardcoded Levels
 - [ ] More Cards.


#### v0.3  Polishing

 - [ ] Animations, Transitions


#### Future

 - Campaign Mode (DnD Storyteller-like)
 - Multiplayer Mode (PvP and 2PvE)
 - Procedural Levels (Floors: Plains, Forest, Castle, Dungeon, Hell)
 - Open World Mode
 - Tower Defense Mode
 - Heroes/Legends (After Completing the Story, Keep your character until he dies)
 - Card Animations (Juicy Legendary Cards)

