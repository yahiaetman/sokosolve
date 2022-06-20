#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "solver.h"
#include "solver_types.h"

typedef struct {
    len_t width, height;    // The level width and height
    const char* level;      // The tiles of the level ordered row by row
    struct {
        size_t max_iterations; // The capacity will always be 4 * max_iterations
        bool use_astar;
        float h_factor, g_factor;
    } solver_config; // The configuration of the solver
    struct {
        bool compilable;
        bool solvable;
        cost_t solution_length; // Since solutions may vary, we only test for the optimal solution length
    } expected;
} test_case_t;

// Tests whether a string of actions does solve the level or not by applying it to the problem
// returns true if the solution is valid, false otherwise
// WARNING: this function mutate "problem"
bool verify_solution(struct context_t* context, struct problem_t* problem, const action_t* actions){
    while(*actions){
        dir_t direction;
        // Convert the action ascii to a direction
        switch (*actions){
        case 'l': case 'L': direction = -1; break;
        case 'r': case 'R': direction = +1; break;
        case 'u': case 'U': direction = -context->width; break;
        case 'd': case 'D': direction = +context->width; break;
        }
        problem->player += direction; // Move the player
        // Check that the player did not enter a wall
        if(get_bit(problem->walls, problem->player)) return false;
        if(get_bit(problem->crates, problem->player)){ // If the player is trying to push a crate
            if(*actions >= 'a') return false; // The player tries to push a crate but the action does not imply it is a push
            pos_t new_crate_pos = problem->player + direction; // get where the crate should end up
            // Check that the new crate position is not in a wall or another crate
            if(get_bit(problem->walls, new_crate_pos)) return false;
            if(get_bit(problem->crates, new_crate_pos)) return false;
            // Update the crate location
            clear_bit(problem->crates, problem->player);
            set_bit(problem->crates, new_crate_pos);
        }
        ++actions; // Go to the next action
    }
    // Check whether the final state is a solution (each crate is on a goal) or not
    return bitset_equals(problem->crates, problem->goals, context->bitset_size);
}

// Run the test case and return true if it succeeded. Otherwise, returns false and prints an test case failure message.
bool run_test_case(int test_case_number, int line_number, test_case_t *test_case, bool print_level){
    bool success = true;

    // Create the context and the problem, then parse the level
    struct context_t* context = create_context(test_case->width, test_case->height, 4 * test_case->solver_config.max_iterations);
    struct problem_t* problem = allocate_problem(context);
    bool compilable = parse_problem(context, problem, test_case->level);

    if(print_level){
        // Print level (only feasible if it is compilable)
        if(compilable){
            char* formatted_problem = format_problem(context, problem, "\n");
            printf("Level Under Test:\n%s\n", formatted_problem);
            free(formatted_problem);
        } else {
            printf("Level Under Test: Cannot be printed since it is uncompilable.\n");
        }
    }
    
    if(compilable != test_case->expected.compilable){
        // Failure Case: The level compilability result is not as expected
        char *STR_UNC = "uncompilable", *STR_C = "compilable";
        printf("TEST %i (Line %i) FAILED: The level is %s but the solver claims it is %s\n",
            test_case_number, line_number,
            (test_case->expected.compilable?STR_C:STR_UNC), 
            (compilable?STR_C:STR_UNC)
        );
        success = false;
    }

    result_t result = {
        .actions = NULL,
        .solved = false,
        .limit_exceeded = false,
        .iterations =0
    };
    cost_t solution_length = 0;

    // To avoid returning without freeing the memory, we just skip the next checks if success is already false
    if(success && compilable){
        // run the solver
        result = test_case->solver_config.use_astar ? 
            solve_astar(context, problem, test_case->solver_config.h_factor, test_case->solver_config.g_factor, test_case->solver_config.max_iterations) : 
            solve_bfs(context, problem, test_case->solver_config.max_iterations);
        
        if(result.solved != test_case->expected.solvable){
            // Failure Case: The level solvability result is not as expected
            char *STR_UNS = "unsolvable", *STR_S = "solvable";
            printf("TEST %i (Line %i) FAILED: The level is %s but the solver claims it is %s.\n", 
                test_case_number, line_number,
                (test_case->expected.solvable?STR_S:STR_UNS), 
                (result.solved?STR_S:STR_UNS)
            );
            success = false;
        }

        if(success && result.solved){

            solution_length = (cost_t)strnlen_s(result.actions, 1<<(8*sizeof(cost_t)));
            if(test_case->expected.solution_length != 0 && test_case->expected.solution_length != solution_length){
                // Failure Case: The solution length is not as expected
                printf("TEST %i (Line %i) FAILED: The expected solution length is %i but the solver's solution requires %i actions.\n", 
                    test_case_number, line_number, test_case->expected.solution_length, solution_length);
                success = false;
            }

            if(!verify_solution(context, problem, result.actions)){
                // Failure Case: The returned solution does not solve the problem
                printf("TEST %i (Line %i) FAILED: The action string %s does not solve the level.\n", 
                    test_case_number, line_number, result.actions);
                success = false;
            }

        }

    }

    // If the test case succeeded, print a detailed success message.
    if(success) {
        if(!compilable){
            printf("TEST %i (Line %i) SUCCEEDED: Level is uncompilable.\n", test_case_number, line_number);
        } else {
            if(result.solved){
                printf("TEST %i (Line %i) SUCCEEDED: Level solved in %zi iterations. The solution (Length: %i) is %s.\n", test_case_number, line_number, result.iterations, solution_length, result.actions);
            } else {
                if(result.limit_exceeded){
                    printf("TEST %i (Line %i) SUCCEEDED: The solver could not solve the level in %zi iterations.\n", test_case_number, line_number, result.iterations);
                } else {
                    printf("TEST %i (Line %i) SUCCEEDED: The level is unsolvable. Search terminated in %zi iterations.\n", test_case_number, line_number, result.iterations);
                }
            }
        }
    }

    free_problem(problem);
    free_context(context);
    free_result(result);
    return success;
}

// The maximum line length that can be read from a file
#define LINE_LENGTH 1024

// print a parsing error message and exit
void emit_error(int line_number, int char_number, const char* error_msg){
    printf("ERROR:%i:%i: %s", line_number, char_number, error_msg);
    exit(1);
}

// Utility functions to parse the input
bool is_white_space(char c){
    return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}

void skip_white_spaces(char** ptr){
    while(is_white_space(**ptr) && **ptr) ++*ptr;
}

bool is_tile(char c){
    return c == '.' || c == 'w' || c == 'W' || c == 'a' || c == 'A' || c == '0' || c == '1' || c == 'g' || c == 'G' || c == '+';
}

// The state of the parser
typedef enum {
    STATE_UNKNOWN = 0,          // The initial state
    STATE_READING_LEVEL = 1,    // We were currently reading a level from the file
    STATE_READING_ACTION = 2    // We were parsing and applying a test action
} parsing_state_t;

// The main reads a test case file, parses it and runs the test cases
// The main expects an argument containing the path to the test case file
int main(int argc, char const *argv[])
{
    // We need a path to the test case file
    if(argc < 2 || strcmp(argv[1], "-h") == 0) {
        printf("Sokosolve Tester - Runs a set of test cases defined in a text file\n");
        printf("Usage:\ttester path/to/test_cases_file\n");
        exit(1);
    }

    // Open the test case file
    const char *file_name = argv[1];
    FILE *file = 0;
    errno_t err = fopen_s(&file, file_name, "r");
    if(err || !file) {
        printf("ERROR: Failed to open file: %s", file_name);
        exit(1);
    }
    
    char level[255*255 + 1];        // The maximum level size is 255 x 255
    size_t level_append_idx = 0;    // The location of the next empty tile index in the level string
    
    test_case_t test_case = {
        .width = 0, .height = 0,    // The width is 0, when no row of the level has been read yet
        .level = level,
    };
    int test_case_number = 0;
    int succeeded_count = 0;

    parsing_state_t parsing_state = STATE_UNKNOWN;

    // Where we will store lines read from the file
    char line[LINE_LENGTH];
    unsigned int line_number = 0;

    while(!feof(file)){
        fgets(line, LINE_LENGTH, file);
        ++line_number;

        char* line_ptr = line;
        skip_white_spaces(&line_ptr);

        if(*line_ptr == 0 || *line_ptr == '#') continue; // Nothing interesting on this line (either empty or a comment)

        if(*line_ptr == ';'){ // Action lines start with a semi-colon

            // If this is the first action on the current level, print it.
            bool print_level = parsing_state != STATE_READING_ACTION;

            parsing_state = STATE_READING_ACTION;   // Transition to the action reading state
            level[level_append_idx] = 0;            // Terminate the level string
            
            ++line_ptr;
            skip_white_spaces(&line_ptr);
            
            if(strnlen_s(line_ptr, LINE_LENGTH) >= 3 && line_ptr[0] == 'B' && line_ptr[1] == 'F' && line_ptr[2] == 'S'){ // If the action is to run BFS
                test_case.solver_config.use_astar = false;

                line_ptr += 3;
                skip_white_spaces(&line_ptr);

                if(*line_ptr != '(') emit_error(line_number, (int)(line_ptr - line), "Invalid Command Format");
                ++line_ptr;
                
                test_case.solver_config.max_iterations = strtol(line_ptr, &line_ptr, 10); // Read the maximum iterations in base 10
                skip_white_spaces(&line_ptr);
                if(*line_ptr != ')') emit_error(line_number, (int)(line_ptr - line), "Invalid Command Format");
                ++line_ptr;

            } else if(strnlen_s(line_ptr, LINE_LENGTH) >= 2 && line_ptr[0] == 'A' && line_ptr[1] == '*'){
                test_case.solver_config.use_astar = true;
                
                line_ptr += 2;
                skip_white_spaces(&line_ptr);
                if(*line_ptr != '(') emit_error(line_number, (int)(line_ptr - line), "Invalid Command Format");
                ++line_ptr;

                test_case.solver_config.h_factor = strtof(line_ptr, &line_ptr);     // Read the h_factor
                skip_white_spaces(&line_ptr);
                if(*line_ptr != ',') emit_error(line_number, (int)(line_ptr - line), "Invalid Command Format");
                ++line_ptr;

                test_case.solver_config.g_factor = strtof(line_ptr, &line_ptr);     // Read the g_factor
                skip_white_spaces(&line_ptr);
                if(*line_ptr != ',') emit_error(line_number, (int)(line_ptr - line), "Invalid Command Format");
                ++line_ptr;
                
                test_case.solver_config.max_iterations = strtol(line_ptr, &line_ptr, 10); // Read the maximum iterations in base 10
                skip_white_spaces(&line_ptr);
                if(*line_ptr != ')') emit_error(line_number, (int)(line_ptr - line), "Invalid Command Format");
                ++line_ptr;

            } else {
                emit_error(line_number, (int)(line_ptr - line), "Invalid Command Format");
            }

            skip_white_spaces(&line_ptr);
            if(*line_ptr != '=') emit_error(line_number, (int)(line_ptr - line), "Invalid Command Format");
            ++line_ptr;
            skip_white_spaces(&line_ptr);

            // Check the expected results
            switch (*line_ptr){
            case 'U':
            case 'u':
                test_case.expected.compilable = false;
                test_case.expected.solvable = false;
                test_case.expected.solution_length = 0;
                break;
            case 'C':
            case 'c':
                test_case.expected.compilable = true;
                test_case.expected.solvable = false;
                test_case.expected.solution_length = 0;
                break;
            case 'S':
            case 's':
                test_case.expected.compilable = true;
                test_case.expected.solvable = true;
                test_case.expected.solution_length = 0;
                ++line_ptr;
                skip_white_spaces(&line_ptr);
                if(*line_ptr != '(') emit_error(line_number, (int)(line_ptr - line), "Invalid Command Format");
                ++line_ptr;
                if(*line_ptr != '_'){
                    test_case.expected.solution_length = (cost_t)strtol(line_ptr, &line_ptr, 10); // Read the solution length in base 10
                } else ++line_ptr;
                skip_white_spaces(&line_ptr);
                if(*line_ptr != ')') emit_error(line_number, (int)(line_ptr - line), "Invalid Command Format");
                ++line_ptr;
                break;
            default:
                break;
            }

            if(run_test_case(++test_case_number, line_number, &test_case, print_level)) ++succeeded_count;

        } else {
            // If we were not reading a level then this could be the start of a new level
            if(parsing_state != STATE_READING_LEVEL){
                // Clear the previous level data
                level_append_idx = 0;
                test_case.width = 0;
                test_case.height = 0;
                parsing_state = STATE_READING_LEVEL;
            }
            // Read a row of level tiles
            len_t width = 0;
            while(*line_ptr != 0){
                // Ignore any character that is not a tile
                if(is_tile(*line_ptr)){
                    ++width;
                    level[level_append_idx++] = *line_ptr;
                } else if(*line_ptr == '#') break; // If we have a comment, skip the rest of the line
                ++line_ptr;
            }
            if(width == 0) continue; // If the line is empty, skip it
            if(test_case.width == 0){ // If the level is empty, this is the first row
                test_case.width = width;
                test_case.height = 1;
            } else if(test_case.width == width){ // If the level was not empty, check the the row widths do not mismatch
                ++test_case.height;
            } else {
                emit_error(line_number, 0, "Level Width Mismatch");
            }
        }

    }
    fclose(file);

    if(succeeded_count == test_case_number){
        printf("SUCCESS: All %i testcases passed\n", succeeded_count);
        return 0;
    } else {
        printf("FAILURE: Only %i/%i testcases passed\n", succeeded_count, test_case_number);
        return 1;
    }
}