# sokosolve
A small C solver for Sokoban with a Python Binding using CFFI

It was created to be used in experiments for the paper:

> **Procedural Level Generation for Sokoban via Deep Learning: An Experimental Study**
>Yahia Zakaria, Magda Fayek, Mayada Hadhoud
 
Paper Link: [https://ieeexplore.ieee.org/abstract/document/9779063](https://ieeexplore.ieee.org/abstract/document/9779063)

## Purpose

Originally, this solver was written to be a faster alternative to the Sokoban solver provided in [gym-pcgrl](https://github.com/amidos2006/gym-pcgrl). This allows training PCGRL to generate larger Sokoban levels as shown in [our paper](https://ieeexplore.ieee.org/abstract/document/9779063).

**Advantages of `sokosolve`**:
* It is fast (at least relative to the python solver in gym-pcgrl).
* It is written in `c` so it should be easy to make bindings for it in other languages.
  
**Disadvantages of `sokosolve`**:
* It preallocate most of the memory it needs before running. So it can consume a significant portion of memory when the maximum iterations is set to a high value.
* A capacity and a maximum iteration limit must be given to the solver.
* The heuristic is quite simple and not tight.

## Installation

```sh
git clone https://github.com/yahiaetman/sokosolve.git
cd sokosolve
pip install -e .
```

This package has been tested on:
* Windows 10/11
* Linux (Ubuntu 20.04 LTS).

## Usage

```python
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
```


## Acknowledgments

* [gym-pcgrl](https://github.com/amidos2006/gym-pcgrl)
* [cffi](https://cffi.readthedocs.io/en/latest/)
* [hashmap.c](https://github.com/tidwall/hashmap.c)