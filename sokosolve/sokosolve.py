from typing import Optional
from _sokosolve import ffi, lib
from dataclasses import dataclass

@dataclass
class Result:
    solved: bool
    actions: Optional[str]
    iterations: int
    limit_exceeded: bool

class SokobanSolver:
    def __init__(self, width: int, height: int, capacity: int) -> None:
        self.context = lib.create_context(width, height, capacity)
        self.problem = lib.allocate_problem(self.context)
    
    def __del__(self):
        lib.free_problem(self.problem)
        lib.free_context(self.context)
    
    def parse_level(self, level_str: str) -> bool:
        arg = ffi.new("char[]", level_str.encode("utf-8"))
        return lib.parse_problem(self.context, self.problem, arg)
    
    def solve_bfs(self, max_iterations: int = 0) -> Result:
        _result = lib.solve_bfs(self.context, self.problem, max_iterations)
        actions = ffi.string(_result.actions) if _result.solved else None
        lib.free_result(_result)
        return Result(_result.solved, actions, _result.iterations, _result.limit_exceeded)
    
    def solve_astar(self, h_factor: float = 1, g_factor: float = 1, max_iterations: int = 0) -> Result:
        _result = lib.solve_astar(self.context, self.problem, h_factor, g_factor, max_iterations)
        actions = ffi.string(_result.actions) if _result.solved else None
        lib.free_result(_result)
        return Result(_result.solved, actions, _result.iterations, _result.limit_exceeded)
