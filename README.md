# sokosolve
A small C solver for Sokoban with a Python Binding using CFFI

## Installation

    git clone https://github.com/yahiaetman/sokosolve.git
    cd sokosolve
    pip install -e .

## How to Use

    from sokosolve import SokobanSolver

    max_iterations = 10_000_000

    # Create a solver for levels of size (width x height)
    # Since the solver preallocates a chunk of memory to store all the states,
    # We need to specify the maximum number of states to preallocate
    solver = SokobanSolver(
                width = 7,
                height = 7, 
                capacity = 4 * max_iterations
            )

    # The level string should contain the tiles row by row
    # The tile set:
    #   Empty:  .
    #   Wall:   W
    #   Agent:  A
    #   Crate:  1
    #   Goal:   0
    #   Agent on Goal:  +
    #   Crate on Goal:  g

    level = \
        "...0..." + \
        "......." + \
        ".W.1W.A" + \
        ".W.1..W" + \
        "...10.." + \
        "......." + \
        "..010.."

    # send the level string to the solver
    # the function returns whether the level is valid or not
    # The validity conditions:
    #   -   There is one and only one player.
    #   -   The number of crates are equal to the number of goals.
    #   -   At least one crate is not on a goal.

    valid = solver.parse_level(level)

    if not valid:
        print("The level is not valid.")
        exit()

    # run breadth first search
    # the solver will fail if the number of iterations exceeds the limit
    
    result = solver.solve_bfs(
                max_iterations=max_iterations
            )

    print("Breadth First Search Results:")
    print(" - Solved:", result.solved)
    print(" - Search Iteration:", result.iterations)
    print(" - Solution:", result.actions)
    print(" - Iteration Limit Exceeded?", result.limit_exceeded)

    # result.actions would either contain None if no solution was found.
    # or a bytes object containing the sequence of actions to solve the level.
    # Each character represents an action:
    #   - Move Right: r
    #   - Move Left: l
    #   - Move Up: u
    #   - Move Down: d
    #   - Move Right while pushing a crate: R
    #   - Move Left while pushing a crate: L
    #   - Move Up while pushing a crate: U
    #   - Move Down while pushing a crate: D

    # run greedy best first search

    result = solver.solve_astar(
                h_factor=1.0,
                g_factor=0.0,
                max_iterations=max_iterations
            )

    # The h_factor and g_factor controls the effect of the heuristic function
    # and the path cost function on the node priority in the frontier.
    # The heuristic is admissible so some common choices would be:
    #   - Uniform Cost Search:      h_factor = 0.0, g_factor = 1.0
    #   - A* Search:                h_factor = 1.0, g_factor = 1.0
    #   - Greedy Best First Search: h_factor = 1.0, g_factor = 0.0

    print("Greedy Best First Search Results:")
    print(" - Solved:", result.solved)
    print(" - Search Iteration:", result.iterations)
    print(" - Solution:", result.actions)
    print(" - Iteration Limit Exceeded?", result.limit_exceeded)



## Used Libraries

* [hashmap.c](https://github.com/tidwall/hashmap.c) by [tidwall](https://github.com/tidwall).