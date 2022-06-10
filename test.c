#include <stdio.h>
#include "solver.h"

typedef struct {
    unsigned char width, height;
    size_t max_iterations;
    const char** levels;
} test_case_set_t;

void test_bfs(struct context_t* context, struct problem_t* problem, size_t max_iterations){
    printf("Running Breadth First Search (Iteration Limit = %zi)...\n", max_iterations);

    result_t result = solve_bfs(context, problem, max_iterations);
    if(!result.solved){
        printf("No Solution Found in %lld iterations!!!\n", result.iterations);
        if(result.limit_exceeded) printf("ITERATION LIMIT EXCEEDED\n");
    } else {
        int solution_length = 0;
        for(;result.actions[solution_length]!=0;++solution_length);
        printf("Solution Found with length=%i in %lld iterations!!!\n", solution_length, result.iterations);
        printf("Solution: %s\n", result.actions);
    }
    free_result(result);
}

void test_astar(struct context_t* context, struct problem_t* problem, float h_factor, float g_factor, size_t max_iterations){
    printf("Running A* Search (H-Factor = %f, G-Factor = %f, Iteration Limit = %zi)...\n", h_factor, g_factor, max_iterations);

    result_t result = solve_astar(context, problem, h_factor, g_factor, max_iterations);
    if(!result.solved){
        printf("No Solution Found in %lld iterations!!!\n", result.iterations);
        if(result.limit_exceeded) printf("ITERATION LIMIT EXCEEDED\n");
    } else {
        int solution_length = 0;
        for(;result.actions[solution_length]!=0;++solution_length);
        printf("Solution Found with length=%i in %lld iterations!!!\n", solution_length, result.iterations);
        printf("Solution: %s\n", result.actions);
    }
    free_result(result);
}

void run_test_case_set(const test_case_set_t* set){
    struct context_t* context = create_context(set->width, set->height, 4 * set->max_iterations);
    struct problem_t* problem = allocate_problem(context);
    
    size_t level_count = 0;
    for(const char** ptr = set->levels; *ptr; ++ptr, ++level_count);

    for(size_t index = 0; index < level_count; ++index){
        printf("Level %zi:\n", index+1);
        bool valid = parse_problem(context, problem, set->levels[index]);
        if(valid){
            char* formatted_problem = format_problem(context, problem, "\n");
            printf("%s\n", formatted_problem);
            free(formatted_problem);

            // printf("\n");
            // test_bfs(context, problem, set->max_iterations);
            // printf("\n");
            // test_astar(context, problem, 0.0f, 1.0f, set->max_iterations);
            // printf("\n");
            // test_astar(context, problem, 1.0f, 1.0f, set->max_iterations);
            printf("\n");
            test_astar(context, problem, 1.0f, 0.0f, set->max_iterations);
            printf("\n");

        } else {
            printf("Invalid Level\n\n");
        }
    }
    free_problem(problem);
    free_context(context);
}

int main(){
    printf("Starting Tests...\n\n");

    /////////////////////////////////

    printf("Testing levels of size 4x4...\n\n");

    char* levels_4x4[] = {
        // "...."
        // "..+."
        // ".11."
        // "....",

        // "..0."
        // "..+."
        // ".1.1"
        // ".WW.",

        "..0."
        "..+."
        ".11."
        "....",

        // ".Wg."
        // "gW.."
        // ".WWW"
        // "A.10",

        NULL
    };

    test_case_set_t set_4x4 = {
        .width = 4,
        .height = 4,
        .max_iterations = 10'000'000,
        .levels = levels_4x4
    };

    run_test_case_set(&set_4x4);

    // /////////////////////////////////

    // printf("Testing levels of size 7x7...\n\n");

    // char* levels_7x7[] = {
    //     ".W.....\n"
    //     ".W.....\n"
    //     ".W.....\n"
    //     "AW.1.0.\n"
    //     ".W.....\n"
    //     ".W.....\n"
    //     ".W.....",

    //     "0......"
    //     "..0...0"
    //     "..1.W.1"
    //     "....W0."
    //     "..11WWW"
    //     "A...1.W"
    //     "......0",

    //     NULL
    // };

    // test_case_set_t set_7x7 = {
    //     .width = 7,
    //     .height = 7,
    //     .max_iterations = 10'000'000,
    //     .levels = levels_7x7
    // };

    // run_test_case_set(&set_7x7);

    // /////////////////////////////////

    // printf("Testing levels of size 9x9...\n\n");

    // char* levels_9x9[] = {
    //     "WWgWW.g.."
    //     ".W..g...."
    //     ".....1.W."
    //     "..g..10W."
    //     "g...W..0."
    //     "W..WW.W.."
    //     "W.g.WA..."
    //     ".g...1g0."
    //     "W........",

    //     NULL
    // };

    // test_case_set_t set_9x9 = {
    //     .width = 9,
    //     .height = 9,
    //     .max_iterations = 10'000'000,
    //     .levels = levels_9x9
    // };

    // run_test_case_set(&set_9x9);
}