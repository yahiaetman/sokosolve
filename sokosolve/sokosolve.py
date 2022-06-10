"""Sokosolve: BFS and AStar Solvers for Sokoban implemented in C.

The class SokobanSolver provides 2 search algorithms:
    - Breadth First Search.
    - A* Search which can also be confgured to run as Uniform Cost Search and Greedy Best First Search.

Example:
--------

    >>> solver = SokobanSolver(width, height, capacity)
    >>> valid = solver.parse_level(level_string)
    >>> if valid:
    >>>     result = solver.solve_astar(1.0, 1.0)

"""

from typing import Optional
from _sokosolve import ffi, lib
from dataclasses import dataclass

@dataclass
class Result:
    """The solver returns an object of this data class after it finishes

    ...
    Attributes
    ----------
    solved: bool
        Was the solver able to solve the problem?
    actions: Optional[str]
        A string of actions containing the solution (or None if no solution was found).
        Each character represents an action which can be:
        'r', 'l', 'u', 'd' for moves without pushing a crate and 'R', 'L', 'U', 'D' for moves while pushing a crate.
    iterations: int
        The number of iterations (expanded nodes) before the solver returned. 
    limit_exceeded: bool
        Has the solver failed due to exceeding the limits (the number of iterations or memory capacity)?
    """
    solved: bool
    actions: Optional[str] 
    iterations: int
    limit_exceeded: bool



class SokobanSolver:
    """A Solver for sokoban levels

    ...
    Methods
    -------
    parse_level(level_str: str)  -> bool
        Parses a level to be later solved by either 'solve_bfs' or 'solve_astar'.
    
    solve_bfs(max_iterations: int = 0) -> Result
        Attempt to solve the level using Breadth First Search.
    
    solve_astar(h_factor: float = 1, g_factor: float = 1, max_iterations: int = 0) -> Result
        Attempt to solve the level using A* Search.
    """

    def __init__(self, width: int, height: int, capacity: int) -> None:
        """Initialize the solver

        Parameters
        ----------
        width : int
            The supported level width
        height : int
            The supported level height
        capacity : int
            The maximum number of states that the solver can generate
        """
        self.__context = lib.create_context(width, height, capacity)
        self.__problem = lib.allocate_problem(self.__context)
    
    def __del__(self):
        """Delete the solver
        """
        lib.free_problem(self.__problem)
        lib.free_context(self.__context)
    
    def parse_level(self, level_str: str) -> bool:
        """Parses a level to be later solved by either 'solve_bfs' or 'solve_astar'

        Parameters
        ----------
        level_str : str
            A string containing the level tiles row by row, each character represents a tile.
            The tileset is as follows:
                Empty: .
                Wall: W or w
                Player: A or a
                Crate: 1
                Goal: 0
                Player on Goal: +
                Crate on Goal: g
            If the level string contains more tiles than the level area, the extra tiles are ignored. 
            Any character that is not a member of the tileset is ignored (including white space characters).
            The function will automatically add walls around the level.

        Returns
        -------
        bool
            True if the level satisfies the following conditions:
                - There is one and only one player.
                - The number of crates are equal to the number of goals.
                - At least one crate is not on a goal. 
        """
        arg = ffi.new("char[]", level_str.encode("utf-8"))
        return lib.parse_problem(self.__context, self.__problem, arg)
    
    def solve_bfs(self, max_iterations: int = 0) -> Result:
        """Attempt to solve the level using Breadth First Search

        Parameters
        ----------
        max_iterations : int, optional
            The maximum number of nodes to be expanded. If 0, the solver can expand any number of nodes 
            as long as it does not generate more nodes than the solver capacity, by default 0

        Returns
        -------
        Result
            The search result
        """
        _result = lib.solve_bfs(self.__context, self.__problem, max_iterations)
        actions = ffi.string(_result.actions) if _result.solved else None
        lib.free_result(_result)
        return Result(_result.solved, actions, _result.iterations, _result.limit_exceeded)
    
    def solve_astar(self, h_factor: float = 1, g_factor: float = 1, max_iterations: int = 0) -> Result:
        """Attempt to solve the level using A* Search.

        If h_factor = 0 & g_factor = 1, the solver will run Uniform Cost Search.
        
        If h_factor = 1 & g_factor = 1, the solver will run A* Search.
        
        If h_factor = 1 & g_factor = 0, the solver will run Greedy Best First Search. 

        Parameters
        ----------
        h_factor : float, optional
            The weight of the heuristic function in the node priority. Must be non-negative, by default 1
        g_factor : float, optional
            The weight of the path cost in the node priority. Must be non-negative, by default 1
        max_iterations : int, optional
            The maximum number of nodes to be expanded. If 0, the solver can expand any number of nodes 
            as long as it does not generate more nodes than the solver capacity, by default 0

        Returns
        -------
        Result
            The search result
        """
        _result = lib.solve_astar(self.__context, self.__problem, h_factor, g_factor, max_iterations)
        actions = ffi.string(_result.actions) if _result.solved else None
        lib.free_result(_result)
        return Result(_result.solved, actions, _result.iterations, _result.limit_exceeded)
