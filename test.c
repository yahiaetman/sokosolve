#include <stdio.h>
#include "solver.h"

int main(){
    printf("Hello World from C!\n");
    struct context_t* context = create_context(7, 7, 40000000);
    char level_str[] = 
    "0......"
    "..0...0"
    "..1.W.1"
    "....W0."
    "..11WWW"
    "A...1.W"
    "......0";
    // char level_str[] = 
    // "......."
    // "......."
    // "......."
    // ".A.1.0."
    // "......."
    // "......."
    // ".......";
    struct problem_t* problem = allocate_problem(context);
    bool valid = parse_problem(context, problem, level_str);
    if(valid){
        result_t result = solve_bfs(context, problem, 10000000);
        if(!result.solved){
            printf("No Solution Found in %lld iterations!!!\n", result.iterations);
            if(result.limit_exceeded) printf("LIMIT EXCEEDED\n");
        } else {
            int solution_length = 0;
            for(;result.actions[solution_length]!=0;++solution_length);
            printf("Solution Found with length=%i in %lld iterations!!!\n", solution_length, result.iterations);
            printf("%s", result.actions);
            free_result(result);
        }
    } else {
        printf("Invalid Level");
    }
    free_problem(problem);
    free_context(context);
    return 0;
}