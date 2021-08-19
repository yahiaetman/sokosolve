#ifndef SOKOBAN_SOLVER_H
#define SOKOBAN_SOLVER_H

#include <stdbool.h>
#include <stdlib.h>

typedef char action_t;

struct context_t;
struct problem_t;

typedef struct {
    bool solved;
    action_t* actions;
    size_t iterations;
    bool limit_exceeded;
} result_t;


struct context_t* create_context(unsigned char width, unsigned char height, size_t capacity);
void free_context(struct context_t* context);

struct problem_t* allocate_problem(struct context_t* context);
bool parse_problem(struct context_t* context, struct problem_t* problem, char* level_str);
void free_problem(struct problem_t* problem);


result_t solve_bfs(struct context_t* context, struct problem_t* problem, size_t max_iterations);
result_t solve_astar(struct context_t* context, struct problem_t* problem, float h_factor, float g_factor, size_t max_iterations);
void free_result(result_t result);


#endif