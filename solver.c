#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hashmap.h"

#include "solver.h"
#include "bitset.h"

typedef unsigned short count_t;
typedef unsigned char len_t;
typedef unsigned short cost_t;
typedef int index_t;

typedef struct _state_t {
    float priority;
    cost_t heuristic, cost;
    struct _state_t* parent;
    index_t heap_index;
    action_t action;
    pos_t player;
    bits_t* crates;
} state_t;

struct context_t {
    len_t width, height;
    pos_t area;
    size_t bitset_size, bitset_stride, state_count;
    state_t* state_cache;
    bits_t* bitset_cache;
    struct hashmap* map;
    state_t** min_heap;
};

struct problem_t {
    count_t goal_count;
    pos_t player;
    bits_t* walls;
    bits_t* goals;
    bits_t* crates;
    bits_t* deadlocks;
    cost_t* heuristics;
    bool compilable, potentially_solvable;
};

int state_compare(const void *a, const void *b, void *udata){
    const state_t *sa = *(state_t**)a;
    const state_t *sb = *(state_t**)b;
    int player_difference = (int)sa->player - (int)sb->player;
    if(player_difference == 0) return bitset_cmp(sa->crates, sb->crates, *(size_t*)udata);
    else return player_difference;
}

uint64_t state_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const state_t *state = *(state_t**)item;
    uint64_t player_hash = hashmap_sip(&(state->player), sizeof(pos_t), seed0, seed1);
    uint64_t crates_hash = hashmap_sip(state->crates, seed0, seed0, seed1);
    return player_hash ^ (crates_hash << 1);
}

void heapify_bottomup(state_t** min_heap, size_t node_index, size_t size){
    state_t* last = min_heap[node_index];
    while(node_index){
        size_t parent_index = node_index >> 1;
        state_t* parent = min_heap[parent_index];
        if(last->priority < parent->priority){
            last->heap_index = parent_index;
            parent->heap_index = node_index;
            min_heap[parent_index] = last;
            min_heap[node_index] = parent;
            node_index = parent_index;
        } else {
            break;
        }
    }
}

void heapify_topdown(state_t** min_heap, size_t root_index, size_t size){
    state_t* root = min_heap[root_index];
    for(;;){
        size_t child1_index = root_index << 1;
        if(child1_index >= size) break;
        size_t min_child_index = child1_index;
        size_t child2_index = child1_index + 1;
        if(child2_index < size && min_heap[child2_index]->priority < min_heap[child1_index]->priority){
            min_child_index = child2_index;
        }
        state_t* child = min_heap[min_child_index];
        if(child->priority < root->priority){
            child->heap_index = root_index;
            root->heap_index = min_child_index;
            min_heap[root_index] = child;
            min_heap[min_child_index] = root;
            root_index = min_child_index;
        } else {
            break;
        }
    }
}

void heap_insert(state_t** min_heap, state_t* element, size_t *size){
    size_t index = *size;
    element->heap_index = index;
    min_heap[index] = element;
    if(index > 0) heapify_bottomup(min_heap, index, ++(*size));
    else ++(*size);
}


state_t* heap_pop(state_t** min_heap, size_t *size){
    size_t last_index = --(*size);
    if(last_index == 0) return min_heap[0];
    state_t* last = min_heap[last_index];
    state_t* root = min_heap[0];
    last->heap_index = 0;
    min_heap[0] = last;
    heapify_topdown(min_heap, 0, last_index);
    return root;
}

struct context_t* create_context(unsigned char width, unsigned char height, size_t capacity) {
    size_t bitset_size = ((width + 2) * (height + 2) + BITS_CNT - 1)/BITS_CNT;
    struct context_t* context = (struct context_t*)malloc(sizeof(struct context_t));
    context->width = width + 2;
    context->height = height + 2;
    context->area = context->width * context->height;
    context->bitset_size = bitset_size;
    context->state_count = capacity + 1;
    context->bitset_stride = context->bitset_size * sizeof(bits_t);
    context->state_cache = (state_t*)malloc(context->state_count * sizeof(state_t));
    context->bitset_cache = (bits_t*)malloc(context->state_count * bitset_size * sizeof(bits_t));
    context->map = hashmap_new(sizeof(state_t*), context->state_count, context->bitset_stride, 420, state_hash, state_compare, &(context->bitset_size));
    context->min_heap = (state_t**)malloc(context->state_count * sizeof(state_t*));
    return context;
}

void free_context(struct context_t* context){
    free(context->min_heap);
    hashmap_free(context->map);
    free(context->bitset_cache);
    free(context->state_cache);
    free(context);
}

struct problem_t* allocate_problem(struct context_t* context){
   struct  problem_t* problem = (struct problem_t*)malloc(sizeof(struct problem_t));
    problem->walls = (bits_t*)malloc(context->bitset_stride);
    problem->goals = (bits_t*)malloc(context->bitset_stride);
    problem->crates = (bits_t*)malloc(context->bitset_stride);
    problem->deadlocks = (bits_t*)malloc(context->bitset_stride);
    problem->heuristics = (cost_t*)malloc(context->width * context->height * sizeof(cost_t));
    problem->compilable = problem->potentially_solvable = false;
    return problem;
}

void free_problem(struct problem_t* problem){
    free(problem->heuristics);
    free(problem->deadlocks);
    free(problem->crates);
    free(problem->goals);
    free(problem->walls);
    free(problem);
}

inline
bool check_reachability(struct context_t *context, bits_t *crates, bits_t *goals, bits_t *walls, pos_t player){
    bits_t* reach = (bits_t*)malloc(context->bitset_stride);
    memset(reach, 0, context->bitset_stride);
    unsigned int end = context->area;
    unsigned int* stack = (unsigned int*)malloc(end * sizeof(unsigned int));
    unsigned int top = 0;
    int directions[] = {-1, 1, context->width, -context->width};
    for(unsigned int position = 0; position < end; ++position){
        if(get_bit(goals, position) && !get_bit(reach, position)){
            stack[top++] = position;
            while(top){
                unsigned int current = stack[--top];
                set_bit(reach, current);
                for(unsigned int index = 0; index < 4; index++){
                    int direction = directions[index];
                    unsigned int next = current + direction;
                    if(get_bit(walls, next) || get_bit(reach, next)) continue;
                    stack[top++] = next;
                }
            }
        }
    }
    free(stack);
    bool result = bitset_covers_all(crates, reach, context->bitset_size) && bitset_covers_all(goals, reach, context->bitset_size);
    free(reach);
    return result;
}


inline
void generate_deadlock_map(struct context_t *context, struct problem_t* problem) {
    pos_t area = context->area;
    memset(problem->deadlocks, ~0, context->bitset_stride);
    for(unsigned int position = 0; position < area; ++position) problem->heuristics[position] = area;
    unsigned int* queue = (unsigned int*)malloc(area * sizeof(unsigned int));
    int directions[] = {-1, 1, context->width, -context->width};
    for(unsigned int position = 0; position < area; ++position){
        if(get_bit(problem->goals, position)){
            unsigned int front = 0, back = 0;
            queue[back++] = position;
            clear_bit(problem->deadlocks, position);
            problem->heuristics[position] = 0;
            while(front < back){
                unsigned int current = queue[front++];
                cost_t cost = problem->heuristics[current] + 1;
                for(unsigned int index = 0; index < 4; index++){
                    int direction = directions[index];
                    unsigned int next = current + direction;
                    if(get_bit(problem->walls, next)) continue;
                    if(!get_bit(problem->deadlocks, next) && problem->heuristics[next] <= cost) continue;
                    unsigned int beyond = next + direction;
                    if(get_bit(problem->walls, beyond)) continue;
                    queue[back++] = next;
                    clear_bit(problem->deadlocks, next);
                    problem->heuristics[next] = cost;
                }
            }
        }
    }
    free(queue);
}

inline
bool check_single_2x2_deadlock(struct context_t* context, struct problem_t* problem, bits_t *crates, pos_t position, int direction){
    int orthos[2];
    orthos[0] = context->width + 1 - abs(direction);
    orthos[1] = -orthos[0];
    int unsafe = get_bit(problem->goals, position) ? 0 : 1;
    pos_t p10 = position + direction;
    bool c10 = get_bit(crates, p10), w10 = get_bit(problem->walls, p10);
    if(!(c10 || w10)) return false;
    if(c10 && !get_bit(problem->goals, p10)) ++unsafe;
    for(int index = 0; index < 2; ++index){
        int ortho = orthos[index];
        int unsafe_internal = unsafe;
        pos_t p01 = position + ortho;
        bool c01 = get_bit(crates, p01), w01 = get_bit(problem->walls, p01);
        if(!(c01 || w01)) continue;
        if(c01 && !get_bit(problem->goals, p01)) ++unsafe_internal;
        pos_t p11 = p10 + ortho;
        bool c11 = get_bit(crates, p11), w11 = get_bit(problem->walls, p11);
        if(!(c11 || w11)) continue;
        if(c11 && !get_bit(problem->goals, p11)) ++unsafe_internal;
        if(unsafe_internal) return true;
    }
    return false;
}

void show_level(struct context_t* context_t, bits_t *crates, bits_t *goals, bits_t *walls, pos_t player){
    pos_t position = 0;
    for(len_t y = 0; y < context_t->height; ++y){
        for(len_t x = 0; x < context_t->width; ++x){
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

bool parse_problem(struct context_t* context, struct problem_t* problem, char* level_str){
    memset(problem->walls, ~0, context->bitset_stride);
    memset(problem->goals, 0, context->bitset_stride);
    memset(problem->crates, 0, context->bitset_stride);
    pos_t position = context->width + 1;
    unsigned int index = 0;
    count_t goal_count = 0, crate_count = 0, player_count = 0;
    for(int j = 2; j < context->height; ++j){
        for(int i = 2; i < context->width; ++i){
            char level_char = level_str[index++];
            if(level_char == 0) goto loop_end;
            if(level_char != 'W') { clear_bit(problem->walls, position); }
            switch (level_char)
            {
            case '0':
                set_bit(problem->goals, position); ++goal_count;
                break;
            case '1':
                set_bit(problem->crates, position); ++crate_count;
                break;
            case 'A':
                problem->player = position; ++player_count;
                break;
            case 'g':
                set_bit(problem->goals, position); ++goal_count;
                set_bit(problem->crates, position); ++crate_count;
                break;
            case '+':
                set_bit(problem->goals, position); ++goal_count;
                problem->player = position; ++player_count;
                break;
            }
            ++position;
        }
        position += 2;
    }
    loop_end:
    problem->goal_count = goal_count;
    bool valid = player_count == 1 && (goal_count == crate_count) && !bitset_equals(problem->crates, problem->goals, context->bitset_size);
    problem->compilable = valid;
    if(valid){
        generate_deadlock_map(context, problem);
        valid = !bitset_covers_any(problem->crates, problem->deadlocks, context->bitset_size);
    }
    if(valid){
        valid = check_reachability(context, problem->crates, problem->goals, problem->walls, problem->player);
    }
    problem->potentially_solvable = valid;
    return problem->compilable;
}

const action_t* ACTIONS = "lrduLRDU";

inline result_t create_result(bool solved, action_t* actions, size_t iterations, bool limit_exceeded){
    result_t result;
    result.solved = solved;
    result.actions = actions;
    result.iterations = iterations;
    result.limit_exceeded = limit_exceeded;
    return result;
}

result_t solve_bfs(struct context_t* context, struct problem_t* problem, size_t max_iterations){
    state_t* free_state = context->state_cache;
    state_t* free_state_end = free_state + context->state_count;
    bits_t* free_bits = context->bitset_cache;

    if(!problem->potentially_solvable)
        return create_result(false, NULL, 0, false);

    // // This is not needed since levels where no actions are needed is considered uncompilable
    // if(bitset_equals(problem->crates, problem->goals, context->bitset_size)){
    //     action_t* solution = (action_t*)malloc(sizeof(action_t));
    //     solution[0] = 0;
    //     return create_result(true, solution, 0, false);
    // }
    
    state_t* current = free_state; ++free_state;
    current->parent = 0;
    current->player = problem->player;
    current->crates = problem->crates;
    current->cost = 0;

    hashmap_clear(context->map, false);
    hashmap_set(context->map, &current);

    int directions[] = {-1, 1, context->width, -context->width};

    size_t iterations = 0;
    while(current != free_state){
        if(max_iterations > 0 && iterations >= max_iterations) return create_result(false, 0, iterations, true);
        ++iterations;
        state_t* parent = current++;
        cost_t cost = parent->cost + 1;
        for(int index=0; index<4; index++){
            int direction = directions[index];
            pos_t player = parent->player + direction;
            if(get_bit(problem->walls, player)) continue;
            action_t action = ACTIONS[index];
            bool changed = false;
            bits_t* crates = parent->crates;
            if(get_bit(parent->crates, player)){
                pos_t next = player + direction;
                if(get_bit(problem->walls, next) || get_bit(parent->crates, next) || get_bit(problem->deadlocks, next)) continue;
                if(check_single_2x2_deadlock(context, problem, crates, next, direction)) continue;
                crates = free_bits; free_bits += context->bitset_size; 
                bitset_copy(parent->crates, crates, context->bitset_size);
                set_bit(crates, next);
                clear_bit(crates, player);
                action = ACTIONS[index+4];
                changed = true;
            }
            //if(changed) show_level(context, crates, goals, walls, player);
            if(changed && bitset_equals(crates, problem->goals, context->bitset_size)){
                action_t* solution = (action_t*)malloc((cost + 1) * sizeof(action_t));
                solution[cost] = 0;
                int index = cost - 1;
                solution[index--] = action;
                while (index >= 0)
                {
                    solution[index--] = parent->action;
                    parent = parent->parent;
                }
                return create_result(true, solution, iterations, false);
            }
            state_t* child = free_state;
            child->action = action;
            child->cost = cost;
            child->parent = parent;
            child->player = player;
            child->crates = crates;
            if(hashmap_get(context->map, &child) == NULL){
                ++free_state;
                hashmap_set(context->map, &child);
                if(free_state == free_state_end) {
                    return create_result(false, 0, iterations, true);
                }
            } else if(changed){
                free_bits -= context->bitset_size; 
            }
        }
    }
    return create_result(false, 0, iterations, false);
}

inline
cost_t compute_heuristic(struct context_t* context, struct problem_t* problem, state_t* state){
    cost_t h = 0;
    for(unsigned int position = 0; position < context->area; ++position){
        if(get_bit(problem->crates, position)) h += problem->heuristics[position];
    }
    return h;
}

bool check(const void* item, void* udata){
    const state_t* s = *(const state_t**)item;
    size_t size = *(size_t*)udata;
    if(s->heap_index != -1 && s->heap_index >= size){
        printf("panic");
    }
    return true;
}

result_t solve_astar(struct context_t* context, struct problem_t* problem, float h_factor, float g_factor, size_t max_iterations){
    state_t* free_state = context->state_cache;
    state_t* free_state_end = free_state + context->state_count;
    bits_t* free_bits = context->bitset_cache;

    if(!problem->potentially_solvable)
        return create_result(false, NULL, 0, false);

    // // This is not needed since levels where no actions are needed is considered uncompilable
    // if(bitset_equals(problem->crates, problem->goals, context->bitset_size)){
    //     action_t* solution = (action_t*)malloc(sizeof(action_t));
    //     solution[0] = 0;
    //     return create_result(true, solution, 0, false);
    // }
    
    state_t* current = free_state; ++free_state;
    current->parent = 0;
    current->player = problem->player;
    current->crates = problem->crates;
    current->cost = 0;
    current->heuristic = compute_heuristic(context, problem, current);
    current->priority = h_factor * current->heuristic;

    hashmap_clear(context->map, false);
    hashmap_set(context->map, &current);

    size_t heap_size = 0;
    heap_insert(context->min_heap, current, &heap_size);
    //hashmap_scan(context->map, check, &heap_size);

    int directions[] = {-1, 1, context->width, -context->width};

    size_t iterations = 0;
    while(heap_size){
        ++iterations;
        if(max_iterations > 0 && iterations >= max_iterations) return create_result(false, 0, iterations, true);
        state_t* parent = heap_pop(context->min_heap, &heap_size);
        parent->heap_index = -1;
        //hashmap_scan(context->map, check, &heap_size);
        cost_t cost = parent->cost + 1;
        for(int index=0; index<4; index++){
            int direction = directions[index];
            pos_t player = parent->player + direction;
            if(get_bit(problem->walls, player)) continue;
            action_t action = ACTIONS[index];
            bool changed = false;
            bits_t* crates = parent->crates;
            if(get_bit(parent->crates, player)){
                pos_t next = player + direction;
                if(get_bit(problem->walls, next) || get_bit(parent->crates, next) || get_bit(problem->deadlocks, next)) continue;
                if(check_single_2x2_deadlock(context, problem, crates, next, direction)) continue;
                crates = free_bits; free_bits += context->bitset_size; 
                bitset_copy(parent->crates, crates, context->bitset_size);
                set_bit(crates, next);
                clear_bit(crates, player);
                action = ACTIONS[index+4];
                changed = true;
            }
            //if(changed) show_level(context, crates, goals, walls, player);
            if(changed && bitset_equals(crates, problem->goals, context->bitset_size)){
                action_t* solution = (action_t*)malloc((cost + 1) * sizeof(action_t));
                solution[cost] = 0;
                int index = cost - 1;
                solution[index--] = action;
                while (index >= 0)
                {
                    solution[index--] = parent->action;
                    parent = parent->parent;
                }
                return create_result(true, solution, iterations, false);
            }
            state_t* child = free_state;
            child->action = action;
            child->cost = cost;
            child->parent = parent;
            child->player = player;
            child->crates = crates;
            void* search_result = hashmap_get(context->map, &child);
            if(search_result == NULL){
                child->heuristic = changed ? compute_heuristic(context, problem, child) : parent->heuristic;
                child->priority = g_factor * cost + h_factor * child->heuristic;
                ++free_state;
                hashmap_set(context->map, &child);
                heap_insert(context->min_heap, child, &heap_size);
                //hashmap_scan(context->map, check, &heap_size);
                if(free_state == free_state_end) {
                    return create_result(false, 0, iterations, true);
                }
            } else { 
                if(changed){
                    free_bits -= context->bitset_size;
                }
                state_t* twin = *(state_t**)search_result;
                if(twin->heap_index >= 0 && twin->cost > child->cost){
                    twin->cost = child->cost;
                    twin->priority = h_factor * twin->heuristic + g_factor * twin->cost;
                    heapify_bottomup(context->min_heap, twin->heap_index, heap_size);
                    //hashmap_scan(context->map, check, &heap_size);
                } 
            }
        }
    }
    return create_result(false, 0, iterations, false);
}

void free_result(result_t result){
    if(result.actions) free(result.actions);
}