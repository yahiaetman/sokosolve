#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hashmap.h"

#include "solver_types.h"

// Compare 2 states based on the player and crate positions (used by the hashmap)
int state_compare(const void *a, const void *b, void *udata){
    const state_t *sa = *(state_t**)a;
    const state_t *sb = *(state_t**)b;
    int player_difference = (int)sa->player - (int)sb->player;
    if(player_difference == 0) return bitset_cmp(sa->crates, sb->crates, *(size_t*)udata);
    else return player_difference;
}

// Hash a state based on the player and crate positions (used by the hashmap)
uint64_t state_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const state_t *state = *(state_t**)item;
    uint64_t player_hash = hashmap_sip(&(state->player), sizeof(pos_t), seed0, seed1);
    uint64_t crates_hash = hashmap_sip(state->crates, seed0, seed0, seed1);
    return player_hash ^ (crates_hash << 1);
}

// Heapify the heap in a bottom up fashion starting from a certain node index
// Used for heap insert to bubble up the new node
// Each node will store an index to its location in the heap that is maintained during the heapify process
void heapify_bottomup(state_t** min_heap, size_t node_index, size_t size){
    state_t* last = min_heap[node_index]; // Starting from the current node
    while(node_index){  // While we are not the root
        size_t parent_index = node_index >> 1;  // Get the parent index (parent index = node index / 2)
        state_t* parent = min_heap[parent_index];
        if(last->priority < parent->priority){  // If the node has more priority (less in value) then the parent, swap them
            // Update the nodes' indices
            last->heap_index = (index_t)parent_index;
            parent->heap_index = (index_t)node_index;
            // swap the nodes
            min_heap[parent_index] = last;
            min_heap[node_index] = parent;
            // then go up
            node_index = parent_index;
        } else { // If the node has less (or equal) priority (>= in value) then the parent, then we are done
            break; 
        }
    }
}

// Heapify the heap in a top down fashion starting from a certain node index
// Used for heap pop to bubble down the leaf node that is swapped with the root
// Each node will store an index to its location in the heap that is maintained during the heapify process
void heapify_topdown(state_t** min_heap, size_t root_index, size_t size){
    state_t* root = min_heap[root_index]; // Starting from the given root index
    for(;;){
        // Check the 2 childern (if they exist) and pick the one with the maximum priority
        size_t child1_index = root_index << 1; // index of 1st child = nodex index * 2
        if(child1_index >= size) break; // If no child exist, we are done
        size_t min_child_index = child1_index;
        size_t child2_index = child1_index + 1;  // index of 2nd child = nodex index * 2 + 1
        if(child2_index < size && min_heap[child2_index]->priority < min_heap[child1_index]->priority){ // Only compare 2 children if the 2nd child exist
            min_child_index = child2_index;
        }
        state_t* child = min_heap[min_child_index];
        if(child->priority < root->priority){ // If the picked child has more priority than its parent, we swap them
            // Update the nodes' indices
            child->heap_index = (index_t)root_index;
            root->heap_index = (index_t)min_child_index;
            // swap the nodes
            min_heap[root_index] = child;
            min_heap[min_child_index] = root;
            // then go down
            root_index = min_child_index;
        } else { // Otherwise, we are done
            break;
        }
    }
}

// Insert a node into the heap.
// Each node will store an index to its location in the heap that is maintained during the insertion and heapify process
void heap_insert(state_t** min_heap, state_t* element, size_t *size){
    // Add the node as the last leaf of the tree
    size_t index = *size;
    element->heap_index = (index_t)index;
    min_heap[index] = element;
    // If there are more than one node, heapify the heap in a bottom up fashion starting from the new node
    if(index > 0) heapify_bottomup(min_heap, index, ++(*size));
    else ++(*size);
}


// Insert a pop from the top of the heap.
// Each node will store an index to its location in the heap that is maintained during the pop and heapify process
state_t* heap_pop(state_t** min_heap, size_t *size){
    // Get the last leaf
    size_t last_index = --(*size);
    // If the last leaf is also the root, we don't need to do anything other than return it.
    if(last_index == 0) return min_heap[0];
    // Otherwise, Swap the root with the last leaf
    state_t* last = min_heap[last_index];
    state_t* root = min_heap[0];
    last->heap_index = 0; // Now the last leaf became the root so its index is 0
    min_heap[0] = last;
    // Heapify the tree in a top down fashion starting from the new root.
    heapify_topdown(min_heap, 0, last_index);
    return root;
}

// Create a context which stores pre-allocated memory to hold states with the given capacity
struct context_t* create_context(unsigned char width, unsigned char height, size_t capacity) {
    struct context_t* context = (struct context_t*)malloc(sizeof(struct context_t));
    if(!context) return NULL;
    context->width = width + 2; // We add 2 more columns to store a wall border
    context->height = height + 2; // We add 2 more rows to store a wall border
    context->area = context->width * context->height;
    // The bitset size is the level area rounded up to the nearest multiple of the bitset_t size in bits
    context->bitset_size = ((width + 2) * (height + 2) + BITS_CNT - 1)/BITS_CNT;
    context->state_count = capacity + 1; // We add 1 to the capacity to store the initial state
    context->bitset_stride = context->bitset_size * sizeof(bits_t);
    // The other data structures and caches will be allocated when needed 
    context->state_cache = NULL;
    context->bitset_cache = NULL;
    context->map = NULL;
    context->min_heap = NULL;
    return context;
}

// Allocate the data structures and caches in the context if they have not been allocated yet
// Since only A* needs the heap, the function receives a flag to know whether it is needed or not 
bool allocate_context_memory(struct context_t* context, bool allocate_heap){
    if(!context->state_cache){
        context->state_cache = (state_t*)malloc(context->state_count * sizeof(state_t));    // Pre-allocate all the states we should ever need
        if(!context->state_cache) return false;
    }
    if(!context->bitset_cache){
        context->bitset_cache = (bits_t*)malloc(context->state_count * context->bitset_size * sizeof(bits_t));    // Pre-allocate all the bitsets we should ever need
        if(!context->bitset_cache) return false;
    }
    if(!context->map){
        // Create a hashmap to that will be used to store the expanded nodes
        context->map = hashmap_new(sizeof(state_t*), context->state_count, context->bitset_stride, 420, state_hash, state_compare, &(context->bitset_size));
        if(!context->map) return false;
    }
    if(allocate_heap && !context->min_heap){
        // Pre-allocate the frontier for A* search.
        context->min_heap = (state_t**)malloc(context->state_count * sizeof(state_t*));
        if(!context->min_heap) return false;
    }
    return true;
}

// Free the context memory
void free_context(struct context_t* context){
    if(!context) return;
    if(context->min_heap) free(context->min_heap);
    if(context->map) hashmap_free(context->map);
    if(context->bitset_cache) free(context->bitset_cache);
    if(context->state_cache) free(context->state_cache);
    free(context);
}

// Allocate memory for a problem
struct problem_t* allocate_problem(struct context_t* context){
    struct  problem_t* problem = (struct problem_t*)malloc(sizeof(struct problem_t));
    if(!problem) return NULL;
    // Allocate a bitset to store the wall locations
    problem->walls = (bits_t*)malloc(context->bitset_stride);
    if(!problem->walls){ free_problem(problem); return NULL; }
    // Allocate a bitset to store the goal locations
    problem->goals = (bits_t*)malloc(context->bitset_stride);
    if(!problem->goals){ free_problem(problem); return NULL; }
    // Allocate a bitset to store the initial crate locations
    problem->crates = (bits_t*)malloc(context->bitset_stride);
    if(!problem->crates){ free_problem(problem); return NULL; }
    // Allocate a bitset to store the crate deadlock locations (where crates can no longer reach any goal)
    problem->deadlocks = (bits_t*)malloc(context->bitset_stride);
    if(!problem->deadlocks){ free_problem(problem); return NULL; }
    // Allocate a 2D array to store the distance to the nearest goal in steps
    problem->heuristics = (cost_t*)malloc(context->width * context->height * sizeof(cost_t));
    if(!problem->heuristics){ free_problem(problem); return NULL; }
    problem->compilable = problem->potentially_solvable = false;
    return problem;
}

// Free the problem memory
void free_problem(struct problem_t* problem){
    if(!problem) return;
    if(problem->heuristics) free(problem->heuristics);
    if(problem->deadlocks) free(problem->deadlocks);
    if(problem->crates) free(problem->crates);
    if(problem->goals) free(problem->goals);
    if(problem->walls) free(problem->walls);
    free(problem);
}

// Check if the player can reach any free crate or goal (crates and goals that are not on the same tile)
// returns true if all of them are reachable
inline
bool check_reachability(struct context_t *context, bits_t *crates, bits_t *goals, bits_t *walls, pos_t player){
    // Allocate the reach map (A bitset of locations from which a crate can reach a goal)
    bits_t* reach = (bits_t*)malloc(context->bitset_stride);
    if(!reach) return false;
    // Initially, all locations are unreachable
    memset(reach, 0, context->bitset_stride);
    // We will fill the reach map in a depth first fashion using a stack structure
    pos_t end = context->area; // The maximum stack storage
    pos_t* stack = (pos_t*)malloc(end * sizeof(pos_t));
    if(!stack) { free(reach); return false; }
    pos_t top = 1; // The top of the stack
    stack[0] = player;
    // The position offset in all 4 directions
    dir_t directions[] = {-1, 1, context->width, -context->width};
    
    while(top){ // While the stack is not empty
        pos_t current = stack[--top]; // Pop a position from the stack
        set_bit(reach, current); // Set this location as reachable
        for(unsigned char index = 0; index < 4; index++){ // For each adjacent location
            dir_t direction = directions[index];
            pos_t next = current + direction;
            // If it is a wall or has been marked as reachable, we don't need to push it to the stack
            if(get_bit(walls, next) || get_bit(reach, next)) continue;
            stack[top++] = next; // push location to stack
        }
    }
     
    free(stack);
    // the result is true if every crate location is within reach of a goal
    bits_t* free_objects = (bits_t*)malloc(context->bitset_stride);
    if(!free_objects) { free(reach); return false; }
    // Use XOR to know the locations of every crate and goal that are not together in the same tile
    bitset_xor(crates, goals, context->bitset_size, free_objects);
    // Check that the player can reach every crate and goal that are not together in the same tile
    bool result = bitset_covers_all(free_objects, reach, context->bitset_size);
    free(free_objects);
    free(reach);
    return result;
}

// Create the deadlock map (location from which no crate can reach any goal)
// Also, compute a path cost from each location to the nearest goal (used for the heuristic function)
inline
void generate_deadlock_map(struct context_t *context, struct problem_t* problem) {
    pos_t area = context->area;
    memset(problem->deadlocks, ~0, context->bitset_stride); // Initially all the locations are deadlocks
    // Initially, the path cost from each location is equal to the nearest goal is the map area (an upper bound used as infinity) 
    for(pos_t position = 0; position < area; ++position) problem->heuristics[position] = area;
    // We will use Breadth first search to fill the deadlock and heuristic map so we allocate a queue
    // It is pre-allocated to store all the positions on the map at once
    pos_t* queue = (pos_t*)malloc(area * sizeof(pos_t));
    if(!queue) return;
    
    // The position offset in all 4 directions
    dir_t directions[] = {-1, 1, context->width, -context->width};

    for(pos_t position = 0; position < area; ++position){
        // For each goal position in the level, start clearing the deadlock map and compute the heuristic
        if(get_bit(problem->goals, position)){
            pos_t front = 0, back = 0; // The front and back of the queue
            queue[back++] = position; // Enqueue goal position
            clear_bit(problem->deadlocks, position); // and clear its location from the deadlock map
            problem->heuristics[position] = 0; // It takes 0 steps from a goal location to itself
            while(front < back){ // While the queue is not empty
                pos_t current = queue[front++]; // Dequeue the front position
                cost_t cost = problem->heuristics[current] + 1; // The cost for the children
                for(unsigned char index = 0; index < 4; index++){ // For every adjacent location
                    dir_t direction = directions[index];
                    pos_t next = current + direction;
                    if(get_bit(problem->walls, next)) continue; // If it is a wall, skip it
                    // If it has been visited in less steps, skip it
                    if(!get_bit(problem->deadlocks, next) && problem->heuristics[next] <= cost) continue;
                    pos_t beyond = next + direction;
                    if(get_bit(problem->walls, beyond)) continue;   // If one more step in the same direction lead to a wall,
                                                                    // then we can push a crate from here in the reverse direction back to the goal
                                                                    // so skip it  
                    queue[back++] = next;   // Add the child to the queue
                    clear_bit(problem->deadlocks, next); // clear its location from the deadlock map
                    problem->heuristics[next] = cost;   // And store the number of steps to reach from the current goal
                }
            }
        }
    }
    free(queue);
}

// Check that pushing a crate to a specific location do not lead to a deadlock with the adjacent walls or crates
inline
bool check_single_2x2_deadlock(struct context_t* context, struct problem_t* problem, bits_t *crates, pos_t position, dir_t direction){
    // Compute and store the directions orthogonal to "direction"
    dir_t orthos[2];
    orthos[0] = context->width + 1 - abs(direction);
    orthos[1] = -orthos[0];
    // If the crate is on goal, then it is not unsafe. We still need to check its neighboring crates
    count_t unsafe = get_bit(problem->goals, position) ? 0 : 1;
    pos_t p10 = position + direction; // Move one more step in same direction and see if it contains a wall or a crate
    bool c10 = get_bit(crates, p10), w10 = get_bit(problem->walls, p10);
    if(!(c10 || w10)) return false; // If there is no wall or a crate in the same direction
                                    // we can push the crate at least one step, so we are probably safe. So return true.
    if(c10 && !get_bit(problem->goals, p10)) ++unsafe;  // But if that adjacent location has a crate without a goal,
                                                        // one more crate is potentially unsafe
    for(int index = 0; index < 2; ++index){ // Now lets look in the orthogonal directions
        dir_t ortho = orthos[index];
        count_t unsafe_internal = unsafe;   // Store a copy of unsafe since each orthogonal direction will have its independent calculations
        pos_t p01 = position + ortho;
        bool c01 = get_bit(crates, p01), w01 = get_bit(problem->walls, p01); // Check the adjacent tile in the orthogonal direction 
        if(!(c01 || w01)) continue; // If it has no wall or crate, then we are probably able to push the crate orthogonally
                                    // so no need to continue checking this direction  
        if(c01 && !get_bit(problem->goals, p01)) ++unsafe_internal; // If it is a crate without a goal, one more crate is potentially unsafe
        pos_t p11 = p10 + ortho;  
        bool c11 = get_bit(crates, p11), w11 = get_bit(problem->walls, p11); // Check the 4th tile in the 2x2 pattern
        if(!(c11 || w11)) continue; // If it has no wall or crate, then may be we are able to break the pattern
                                    // so no need to continue checking this direction
        if(c11 && !get_bit(problem->goals, p11)) ++unsafe_internal; // If it is a crate without a goal, one more crate is potentially unsafe
        // If the 2x2 pattern is complete, and it contains crates that are not on a goal, we can no longer salvage this crate. So it is a deadlock.
        if(unsafe_internal) return true;
    }
    // If no deadlock in any of the 2 orthogonal directions, we are probably safe.
    return false;
}

// Check that in the initial state, no crate is stuck in a 2x2 deadlock with the adjacent walls or crates
inline
bool check_all_2x2_deadlock(struct context_t* context, struct problem_t* problem){
    // Compute and store the directions to all the 2x2 neighborhood
    dir_t neighborhood[4];
    neighborhood[0] = 0;
    neighborhood[1] = 1;
    neighborhood[2] = context->width;
    neighborhood[3] = context->width + 1;

    // loop over all 2x2 windows   
    pos_t position = 0;
    for(pos_t y = 0, y_end = context->height - 1; y < y_end; ++y){
        for(pos_t x = 0, x_end = context->width - 1; x < x_end; ++x){
            count_t unsafe = 0; // Count the number of crates that would be stuck if the 2x2 pattern is complete
            // Loop over the 2x2 neighborhood
            for(int index = 0; index < 4; ++index){
                pos_t neighbor = position + neighborhood[index];
                bool has_crate = get_bit(problem->crates, neighbor), has_wall = get_bit(problem->walls, neighbor);
                // If this tile is empty (no wall or crate), we are safe (for now)
                if(!(has_crate || has_wall)) { unsafe = 0; break; }
                // If there is a crate but without a goal, it is unsafe
                if(has_crate && !get_bit(problem->goals, neighbor)) ++unsafe;
            }
            // If the 2x2 pattern is complete and it contains unsafe crates, the level cannot be solved
            if(unsafe) return true;        
            ++position;
        }
        ++position; // Skip the end of row
    }
    return false;
}

// Print a state onto the console
// Used for debugging.
void show_level(struct context_t* context, bits_t *crates, bits_t *goals, bits_t *walls, pos_t player){
    pos_t position = 0;
    for(len_t y = 0; y < context->height; ++y){
        for(len_t x = 0; x < context->width; ++x){
            char c = '.';
            if(get_bit(walls, position)) c = 'W';
            else if(get_bit(goals, position)){
                c = '0';
                if(player == position) c = '+';
                else if(get_bit(crates, position)) c = 'g';
            } else {
                if(player == position) c = 'A';
                else if(get_bit(crates, position)) c = '1';
            }
            printf("%c", c);
            ++position;
        }
        printf("\n");
    }
}

// Print a bit pattern onto a consolve (0 => '.', 1 => '#')
// Used for debugging.
void show_bits(struct context_t* context_t, bits_t *bits){
    pos_t position = 0;
    for(len_t y = 0; y < context_t->height; ++y){
        for(len_t x = 0; x < context_t->width; ++x){
            char c = get_bit(bits, position) ? '#' : '.';
            printf("%c", c);
            ++position;
        }
        printf("\n");
    }
}

// Format a problem as null-delimited string where a separator pattern is added between rows.
// If the separator is NULL, it is treated as an empty string.
// You have to free the returned string on your own.
// Used for debugging.
char* format_problem(struct context_t* context, struct problem_t* problem, const char* separator){
    size_t sep_len = separator ? strlen(separator) : 0;
    size_t total_len = context->height * (context->width + sep_len) - sep_len + 1;
    char* result = (char*)malloc(total_len * sizeof(char));
    pos_t position = 0;
    size_t index = 0;
    for(len_t y = 0; y < context->height; ++y){
        if(y) for(size_t i = 0; i < sep_len; ++i) result[index++] = separator[i];
        for(len_t x = 0; x < context->width; ++x){
            char c = '.';
            if(get_bit(problem->walls, position)) c = 'W';
            else if(get_bit(problem->goals, position)){
                c = '0';
                if(problem->player == position) c = '+';
                else if(get_bit(problem->crates, position)) c = 'g';
            } else {
                if(problem->player == position) c = 'A';
                else if(get_bit(problem->crates, position)) c = '1';
            }
            result[index++] = c;
            ++position;
        }
    }
    result[index] = 0;
    return result;
}

// Parse a null-delimited level string and store its data into the problem
// returns true iff the level is compilable (see problem_t.compilable for more details) 
bool parse_problem(struct context_t* context, struct problem_t* problem, const char* level_str){
    memset(problem->walls, ~0, context->bitset_stride); // Initially, every location is considered to contain a wall
                                                        //  to make sure that the border contain walls
    memset(problem->goals, 0, context->bitset_stride);  // Initially, no location is considered to contain a goal
    memset(problem->crates, 0, context->bitset_stride); // Initially, no location is considered to contain a crate
    pos_t position = context->width + 1;    // We will start adding tiles at location (1, 1) (translated to 1 + 1 * width)
                                            // This is because, the outer border will only contain walls
    unsigned int index = 0;
    count_t goal_count = 0, crate_count = 0, player_count = 0;
    for(int j = 2; j < context->height; ++j){ // We skip the first and last row (borders)
        for(int i = 2; i < context->width; ++i){ // We skip the first and last column (borders)
            bool failed = false; // Did we find a tile in the current character of the string.
            do {
                failed = false;
                char level_char = level_str[index++];
                if(level_char == 0) goto loop_end; // If this is a NULL character, we stop parsing
                switch (level_char)
                {
                case 'W':
                case 'w':
                    // Wall
                    break;
                case '.':
                    // Empty
                    clear_bit(problem->walls, position);
                    break;
                case '0':
                    // Goal
                    clear_bit(problem->walls, position);
                    set_bit(problem->goals, position); ++goal_count;
                    break;
                case '1':
                    // Crate
                    clear_bit(problem->walls, position);
                    set_bit(problem->crates, position); ++crate_count;
                    break;
                case 'A':
                case 'a':
                    // Agent
                    clear_bit(problem->walls, position);
                    problem->player = position; ++player_count;
                    break;
                case 'g':
                case 'G':
                    // Crate on a goal
                    clear_bit(problem->walls, position);
                    set_bit(problem->goals, position); ++goal_count;
                    set_bit(problem->crates, position); ++crate_count;
                    break;
                case '+':
                    // Player on a goal
                    clear_bit(problem->walls, position);
                    set_bit(problem->goals, position); ++goal_count;
                    problem->player = position; ++player_count;
                    break;
                default:
                    // If the character is unexpected, we skip it and try again
                    failed = true;
                }
            } while(failed);
            ++position;
        }
        position += 2; // Skip 2 positions to jump over the right and left borders
    }
    loop_end:
    problem->goal_count = goal_count;
    // Check that the level is compilable
    bool valid = player_count == 1 && (goal_count == crate_count) && !bitset_equals(problem->crates, problem->goals, context->bitset_size);
    problem->compilable = valid;
    // Check that no 2x2 deadlock patterns exist
    if(valid){
        valid = !check_all_2x2_deadlock(context, problem);
    }
    // Compute the deadlock map (and heuristic map) and check that all the crates can potentially reach a goal
    if(valid){
        generate_deadlock_map(context, problem);
        valid = !bitset_covers_any(problem->crates, problem->deadlocks, context->bitset_size);
    }
    // Check that the player can reach all the unpaired crates and goals
    if(valid){
        valid = check_reachability(context, problem->crates, problem->goals, problem->walls, problem->player);
    }
    problem->potentially_solvable = valid;
    return problem->compilable; // We only return whether the level is compilable or not (regardless of whether it is potentially solvable or not)
}

// A string of all the action (used to populate the solution)
const action_t* ACTIONS = "lrduLRDU";

// A utility function to create a result
inline result_t create_result(bool solved, action_t* actions, size_t iterations, bool limit_exceeded){
    result_t result;
    result.solved = solved;
    result.actions = actions;
    result.iterations = iterations;
    result.limit_exceeded = limit_exceeded;
    return result;
}

// Run BFS to attempt solving a problem
// if "max_iterations" > 0 , The solver can only expand "max_iterations" nodes
result_t solve_bfs(struct context_t* context, struct problem_t* problem, size_t max_iterations){
    // If we already know that it is impossible to solve, we return a failure result
    if(!problem->potentially_solvable)
        return create_result(false, NULL, 0, false);

    // We do not need to check if the problem is already solved
    // since levels where no actions are needed are considered uncompilable

    // Allocate the context memory (if not allocated already) 
    if(!allocate_context_memory(context, false))
        // If the allocation fails, we return a failure result
        return create_result(false, NULL, 0, true);

    state_t* free_state = context->state_cache; // A pointer in the state cache for the next free state to use
    state_t* free_state_end = free_state + context->state_count; // A pointer to the end of the cache (to know if we consumed all of the cache)
    bits_t* free_bits = context->bitset_cache; // A pointer in the bitset cache for the next free bitset to use
    
    // Allocate the first state from the cache and store the initial state in it
    state_t* current = free_state; ++free_state;
    current->parent = 0;
    current->player = problem->player;
    current->crates = problem->crates;
    current->cost = 0;

    hashmap_clear(context->map, false); // Make sure the hashmap is empty
    hashmap_set(context->map, &current); // Add the initial state to the hashmap

    // The position offset in all 4 directions
    dir_t directions[] = {-1, 1, context->width, -context->width};

    size_t iterations = 0;

    // Since the BFS frontier works in a FIFO manner, we don't need to a separate queue,
    // We will use the state cache as a queue since it will already store the state in expansion order
    // So the queue is empty if the pointer to the current state points to the free state
    while(current != free_state){
        // Check if we exceeded the iteration limit
        if(max_iterations > 0 && iterations >= max_iterations) return create_result(false, 0, iterations, true);
        ++iterations;
        state_t* parent = current++; // Get the next node from the queue and increment the queue pointer.
        cost_t cost = parent->cost + 1; // Compute the child cost (every step costs 1)
        for(unsigned char index=0; index<4; index++){ // For each direction
            dir_t direction = directions[index];
            pos_t player = parent->player + direction; // Get the new player location
            if(get_bit(problem->walls, player)) continue; // If the new location is in a wall, we skip this action
            action_t action = ACTIONS[index]; // Get an action ASCII assuming that there was not crates pushed
            bool changed = false;   // Store whether the crates bitset has changed
            bits_t* crates = parent->crates;
            if(get_bit(parent->crates, player)){ // If the player trys to push a crate
                pos_t next = player + direction;
                // If the new crate position will be in a wall, another crate, a deadlock position or in a location that will cause a 2x2 deadlock, skip this action
                if(get_bit(problem->walls, next) || get_bit(parent->crates, next) || get_bit(problem->deadlocks, next)) continue;
                if(check_single_2x2_deadlock(context, problem, crates, next, direction)) continue;
                // Allocate a new crates bitset from the bitset cache
                crates = free_bits; free_bits += context->bitset_size;
                // Copy the parent's crate bitset and apply the changes caused by the action
                bitset_copy(parent->crates, crates, context->bitset_size);
                set_bit(crates, next);
                clear_bit(crates, player);
                action = ACTIONS[index+4]; // Change the action ASCII to denote a crate push
                changed = true; // The crate bitset has changed
            }
            // If the crate bitset changed, check if we have already reached a goal
            if(changed && bitset_equals(crates, problem->goals, context->bitset_size)){
                // Since BFS is optimal and in Sokoban, each step costs 1,
                // The solution length is equal the computed path cost
                action_t* solution = (action_t*)malloc((cost + 1) * sizeof(action_t));
                solution[cost] = 0; // The solution is null-delimited
                int index = cost - 1;
                // Backtrack from the current node to the initial state and store the actions along the path
                solution[index--] = action;
                while (index >= 0)
                {
                    solution[index--] = parent->action;
                    parent = parent->parent;
                }
                // Return the success result alongside the solution
                return create_result(true, solution, iterations, false);
            }
            // Create a child state
            state_t* child = free_state;
            child->action = action;
            child->cost = cost;
            child->parent = parent;
            child->player = player;
            child->crates = crates;
            // Check if it already exists
            if(hashmap_get(context->map, &child) == NULL){ // If it did not already exist
                ++free_state; // Increment the free state pointer (the back of the queue)
                hashmap_set(context->map, &child); // Add the child to the hashmap
                if(free_state == free_state_end) { // If we have no more states to use in the cache, we return a failure result 
                    return create_result(false, 0, iterations, true);
                }
            } else if(changed){ // If the child state already exist, we skip it and if it consumed a new bitset, we return that bitset to the cache
                free_bits -= context->bitset_size; 
            }
        }
    }
    // If no solution was found, we return a failure result
    return create_result(false, 0, iterations, false);
}

// Compute the heuristic for a state
inline
cost_t compute_heuristic(struct context_t* context, struct problem_t* problem, state_t* state){
    cost_t h = 0;
    // For each crate, find the path cost to the nearest goal and add it to the heuristic
    for(unsigned int position = 0; position < context->area; ++position){
        if(get_bit(state->crates, position)) h += problem->heuristics[position];
    }
    return h;
}

// Run BFS to attempt solving a problem
// if "max_iterations" > 0 , The solver can only expand "max_iterations" nodes
// The node priority is equal to h_factor * heursitic of node + g_factor * path cost from the initial state to the node
result_t solve_astar(struct context_t* context, struct problem_t* problem, float h_factor, float g_factor, size_t max_iterations){
    // If we already know that it is impossible to solve, we return a failure result
    if(!problem->potentially_solvable)
        return create_result(false, NULL, 0, false);

    // We do not need to check if the problem is already solved
    // since levels where no actions are needed are considered uncompilable

    // Allocate the context memory (if not allocated already) 
    if(!allocate_context_memory(context, true))
        // If the allocation fails, we return a failure result
        return create_result(false, NULL, 0, true);
    
    state_t* free_state = context->state_cache; // A pointer in the state cache for the next free state to use
    state_t* free_state_end = free_state + context->state_count; // A pointer to the end of the cache (to know if we consumed all of the cache)
    bits_t* free_bits = context->bitset_cache; // A pointer in the bitset cache for the next free bitset to use
    
    // Allocate the first state from the cache and store the initial state in it
    state_t* current = free_state; ++free_state;
    current->parent = 0;
    current->player = problem->player;
    current->crates = problem->crates;
    current->cost = 0;
    // Compute node heuristic and priority
    current->heuristic = compute_heuristic(context, problem, current);
    current->priority = h_factor * current->heuristic; // The cost is 0, so we skip computing g_factor * current->cost

    hashmap_clear(context->map, false);  // Make sure the hashmap is empty
    hashmap_set(context->map, &current); // Add the initial state to the hashmap

    size_t heap_size = 0; // The current heap size (It is not stored in the context since we don't need to persist outside the search function)
    heap_insert(context->min_heap, current, &heap_size);

    // The position offset in all 4 directions
    int directions[] = {-1, 1, context->width, -context->width};

    size_t iterations = 0;

    // While there are more nodes in the heap
    while(heap_size){
        // Check if we exceeded the iteration limit
        if(max_iterations > 0 && iterations >= max_iterations) return create_result(false, 0, iterations, true);
        ++iterations;
        state_t* parent = heap_pop(context->min_heap, &heap_size); // Get the current node from the heap front
        parent->heap_index = -1; // Set heap index to -1 since it is no longer in the heap
        cost_t cost = parent->cost + 1; // Compute the child cost (every step costs 1)
        for(int index=0; index<4; index++){ // For each direction
            int direction = directions[index];
            pos_t player = parent->player + direction; // Get the new player location
            if(get_bit(problem->walls, player)) continue; // If the new location is in a wall, we skip this action
            action_t action = ACTIONS[index]; // Get an action ASCII assuming that there was not crates pushed
            bool changed = false;   // Store whether the crates bitset has changed
            bits_t* crates = parent->crates;
            if(get_bit(parent->crates, player)){ // If the player trys to push a crate
                pos_t next = player + direction;
                // If the new crate position will be in a wall, another crate, a deadlock position or in a location that will cause a 2x2 deadlock, skip this action
                if(get_bit(problem->walls, next) || get_bit(parent->crates, next) || get_bit(problem->deadlocks, next)) continue;
                if(check_single_2x2_deadlock(context, problem, crates, next, direction)) continue;
                // Allocate a new crates bitset from the bitset cache
                crates = free_bits; free_bits += context->bitset_size; 
                // Copy the parent's crate bitset and apply the changes caused by the action
                bitset_copy(parent->crates, crates, context->bitset_size);
                set_bit(crates, next);
                clear_bit(crates, player);
                action = ACTIONS[index+4]; // Change the action ASCII to denote a crate push
                changed = true; // The crate bitset has changed
            }
            // If the crate bitset changed, check if we have already reached a goal
            // Since each step is of equal cost, we don't need to wait for the state to exit the heap to do the goal test
           if(changed && bitset_equals(crates, problem->goals, context->bitset_size)){
                // Since BFS is optimal and in Sokoban, each step costs 1,
                // The solution length is equal the computed path cost
                action_t* solution = (action_t*)malloc((cost + 1) * sizeof(action_t));
                solution[cost] = 0;
                int index = cost - 1;
                // Backtrack from the current node to the initial state and store the actions along the path
                solution[index--] = action;
                while (index >= 0)
                {
                    solution[index--] = parent->action;
                    parent = parent->parent;
                }
                // Return the success result alongside the solution
                return create_result(true, solution, iterations, false);
            }
            // Create a child state
            state_t* child = free_state;
            child->action = action;
            child->cost = cost;
            child->parent = parent;
            child->player = player;
            child->crates = crates;
            // Check if it already exists
            void* search_result = hashmap_get(context->map, &child);
            if(search_result == NULL){ // If it did not already exist
                // The heuristic only depends on the crate positions so we only need to recompute it when a crate moves
                child->heuristic = changed ? compute_heuristic(context, problem, child) : parent->heuristic;
                child->priority = g_factor * cost + h_factor * child->heuristic;
                ++free_state;  // Increment the free state pointer (a cached state is consumed)
                hashmap_set(context->map, &child); // Add the child to the hashmap
                heap_insert(context->min_heap, child, &heap_size); // Add the child to the heap
                if(free_state == free_state_end) {  // If we have no more states to use in the cache, we return a failure result
                    return create_result(false, 0, iterations, true);
                }
            } else {  // If the child state already exist, we skip it
                //  if the child consumed a new bitset, we return that bitset to the cache
                if(changed){
                    free_bits -= context->bitset_size;
                }
                state_t* twin = *(state_t**)search_result;
                // Check on the similar node that was already expanded
                // If it is still in the heap and its cost was higher than the new child, we update it
                if(twin->heap_index >= 0 && twin->cost > child->cost){
                    // Copy the parent, action, cost from the new child
                    twin->parent = child->parent;
                    twin->action = child->action;
                    twin->cost = child->cost;
                    // Recompute the priority
                    twin->priority = h_factor * twin->heuristic + g_factor * twin->cost;
                    // Heapify bottomup from the its current index to find its new location in the heap
                    heapify_bottomup(context->min_heap, twin->heap_index, heap_size);
                } 
            }
        }
    }
    // If no solution was found, we return a failure result
    return create_result(false, 0, iterations, false);
}

// Free the result
void free_result(result_t result){
    if(result.actions) free(result.actions);
}