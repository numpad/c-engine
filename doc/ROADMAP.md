
done dump:
----------

allow player during turn to set position to a reachable tile (say in a 2 tile radius)
hexmap find all "reachable" tiles in distance
allow it only once (or for a movement cost)
player can move turn based
enemy moves 1 tile in random direction
enemy health bar
update hexmap obstacle tiles
heal card heals for a few hp
fix text rendering on mobile/highdpi
character (position) linear tile movement along tile positions path


plan pile:
----------
player makes a basic attack by clicking on a neighboring enemy
hexmap highlight for enemy (only those in reach?) for basic attacks

characters (position & angle) tile movement along spline path, catmullrom?

font
 - horizontal alignment
 - outlines
 - fontsize scale to fit into x,y,w,h (card body, needs to support scaling)

hexmap highlight affected the entities/tiles which are affected when player hovers a card

random turn order of all characters

the armor/shield card increases the player armor by 1
the armor/shield card makes an effect when played:
 - armor buff icon apears
 - player model shader effect
 - model scale increase for a quick second

enemy has simple AI, moves towards player and makes basic attacks

animated models
 - only draw one of the KayKit character model's equipment
 - load gltf skeletal animation data
 - skeletal animation

pathfinder edges can have increased movement cost (eg: water tile, flowing river)
implement SSAO

simple dialogue screen (just text and button, animated/each letter fades in)
map fade-off transition at edges:
 - draw +1 tile, lower than base tile/sloped, decoration on it?
 - draw +1 tile & stencil test with particles?

fixme field:
------------
draw enemy health bars at their Z coordinate (correctly handle depth with other characters)
replace raw djikstra with A* in hexmap
visualize pathfinder/flowfield with arrows
end turn only after animations finished? skippable?

