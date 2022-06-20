#ifndef _SOKOSOLVE_SOLVER_TYPES_H
#define _SOKOSOLVE_SOLVER_TYPES_H

#include "solver.h"
#include "bitset.h"

typedef unsigned short count_t; // The datatype for counting objects (crates, goals, etc)
typedef unsigned char len_t;    // The datatype to store either the width or height of the level (currently, we assume it will never be more than 255)
typedef unsigned short cost_t;  // The datatype to store the path cost in the level (currently, we assume that it will never be more than 65535)
typedef int index_t;            // The datatype to store an index into an array
typedef short dir_t;            // The datatype to store a direction (offset in pos_t)

typedef struct _state_t {
    float priority;             // The priority of the node in the frontier (lower value => higher priority)
    cost_t heuristic, cost;     // The heuristic value of the node and the path cost from the initial state to the current node
    struct _state_t* parent;    // The parent node from which this node was expanded (NULL if this node is the root)
    index_t heap_index;         // The index of the node in the heap frontier (-1 if it is not in the frontier)
    action_t action;            // The action that would transition from the parent node to the current node
    pos_t player;               // The position of the player in the current state (it is stored as player.x + width * player.y)
    bits_t* crates;             // The bitset representing the crate positions in the current state
} state_t;

struct context_t {
    len_t width, height;    // The supported width and height for the levels (it is 2 more than the actual width and height of the level to store a border of walls)
    pos_t area;             // The area of the level (width * height). We cache it since we use it alot.
    size_t bitset_size, bitset_stride, state_count; // bitset_size: The bitset size (in bits_t) require to store object on the whole level
                                                    // bitset_stride: the same as bitset_size but in bytes (used for allocation)
                                                    // state_count: the maximum number of states that can be stored at once (capacity + 1)
    state_t* state_cache;   // A pre-allocated cache of all the states that we should ever need
    bits_t* bitset_cache;   // A pre-allocated cache of all the bitsets that we should ever need 
    struct hashmap* map;    // A hashmap to store the explored nodes.
    state_t** min_heap;     // A min-heap to store the frontier for A* search
};

struct problem_t {
    count_t goal_count; // The number of goals in the problem (and also the number of crates)
    pos_t player;       // The initial position of the player (it is stored as player.x + width * player.y)
    bits_t* walls;      // A bitset storing the locations of the walls (each set bit represents a wall)
    bits_t* goals;      // A bitset storing the locations of the goals (each set bit represents a goal)
    bits_t* crates;     // A bitset storing the initial locations of the crates (each set bit represents a crate)
    bits_t* deadlocks;  // A bitset storing the locations where crate have no hope to reach a goal (assuming no other crate in the map and that the player can be anywhere)
    cost_t* heuristics; // An array store the cost to reach the nearest goal from each location on the level
    bool compilable, potentially_solvable;  // Whether a level is compilable and potentially solvable or not.
                                            // A level is considered compilable if:
                                            //  - There is one and only one player in the level.
                                            //  - The number of crate equals the number of goals.
                                            //  - At least one crate is not on a goal.
                                            // A level is considered potentially solvable if:
                                            //  - Every crate is in a position from which it can potentially reach a goal.
                                            //  - The player can reach any free crate or goal (crates and goals that are not on the same tile).
};

#endif