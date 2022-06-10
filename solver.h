/*************************************************************************
 * SokoSolve
 * BFS and AStar Solvers for Sokoban implemented in C
 *------------------------------------------------------------------------
 * MIT License
 * 
 * Copyright (c) 2021 Yahia Zakaria
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *************************************************************************/

#ifndef _SOKOSOLVE_SOLVER_H
#define _SOKOSOLVE_SOLVER_H

#include <stdbool.h>
#include <stdlib.h>

// This type represents an action.
// It can be"
//      'r', 'l', 'u', 'd' for moves without pushing crates.
//      'R', 'L', 'U', 'D' for moves while pushing crates.
typedef char action_t;

// The context contains the pre-allocated data structures required by the solver (e.g., the frontier, the open/closed set, caches)
struct context_t;
// The problem contains the details of the level (wall & goal locations, the initial player & crates locations, etc)
struct problem_t;

// The solver returns a result structure after it finishes
typedef struct {
    bool solved; // Was the solver able to solve the problem?
    action_t* actions; // A null-delimited string of actions containing the solution (or null if no solution was found). 
    size_t iterations; // The number of iterations (expanded nodes) before the solver returned. 
    bool limit_exceeded; // Has the solver failed due to exceeding the limits (the number of iterations or memory capacity)?
} result_t;

// Allocate a context. We must specify the level width, height and the maximum number of states (capacity) to be created by the solver.
struct context_t* create_context(unsigned char width, unsigned char height, size_t capacity);
// Free the context. This function does not handle double-free.
void free_context(struct context_t* context);

// Allocate memory for a problem (the wall, crate, goal & player locations, data used by the heuristic, etc.)
struct problem_t* allocate_problem(struct context_t* context);

// Given a string representing a level row by row, each character represents a tile.
// The tileset is as follows:
//      Empty: .
//      Wall: W or w
//      Player: A or a
//      Crate: 1
//      Goal: 0
//      Player on Goal: +
//      Crate on Goal: g
// The level string must be null-delimited. Any character after the NULL character is ignored.
// If the level string contains more tiles than the level area, the extra tiles are ignored. 
// Any character that is neither NULL or a member of the tileset is ignored (including white space characters).
// The function will automatically add walls around the level so a level of size WxH will be presented as
// a (W+2)x(H+2) level with the first and last columns being walls.
// The function returns a boolean which is true if it satisfies the following conditions:
//      - There is one and only one player.
//      - The number of crates are equal to the number of goals.
//      - At least one crate is not on a goal. 
// NOTE: you could reuse the problem to parse another level later.
bool parse_problem(struct context_t* context, struct problem_t* problem, const char* level_str);
// This will convert the problem to string. The formatted string will automatically add walls around the level.
// so a level of size WxH will be presented as a (W+2)x(H+2) level with the first and last columns being walls. 
char* format_problem(struct context_t* context, struct problem_t* problem, const char* separator);
// Free the problem. This function does not handle double-free.
void free_problem(struct problem_t* problem);

// Attempt to solve the level using Breadth first search in no more than the given maximum iteration limit.
// If the maximum iteration is 0, then the only restriction is that the solver cannot create more nodes than the context capacity.
// You could use a problem with any context that has the same width and height as the context used to allocate the problem. 
result_t solve_bfs(struct context_t* context, struct problem_t* problem, size_t max_iterations);
// Attempt to solve the level using A* search in no more than the given maximum iteration limit.
// If the maximum iteration is 0, then the only restriction is that the solver cannot create more nodes than the context capacity.
// The function allows you to control the weight of the heuristic function and the path cost in the node priority.
//      priority = h_factor * heuristic(state) + g_factor * path_cost(state)
// So by setting h_factor = 1 & g_factor = 1, you get A* search.
// So by setting h_factor = 0 & g_factor = 1, you get Uniform cost search.
// So by setting h_factor = 1 & g_factor = 0, you get Greedy best first search.
// You could use a problem with any context that has the same width and height as the context used to allocate the problem.
result_t solve_astar(struct context_t* context, struct problem_t* problem, float h_factor, float g_factor, size_t max_iterations);
// Free the search result.
void free_result(result_t result);

// NOTE: you could run multiple search functions on the same problem and context
// without needing to reallocate them or the re-parse the level. 

// A common usage pattern:
//
// context_t* context = create_context(width, height, capacity);
// problem_t* problem = allocate_problem(context);
// bool valid = parse_problem(context, problem, level_str);
// if(valid){
//      result_t result = solve_astar(context, problem, 1.0f, 1.0f, 0);
//      // do what you want with the results ....
//      free_result(result);
// }
// free_problem(problem);
// free_context(context);


#endif