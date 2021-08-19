from setuptools import setup

setup(
    name="sokosolve",
    version="0.0.1",
    description='BFS and AStar Solver for Sokoban implement in C',
    author='Yahia Zakaria',
    packages=["sokosolve"],
    setup_requires=["cffi>=1.0.0"],
    cffi_modules=["sokosolve/build_binding.py:ffibuilder"],
    install_requires=["cffi>=1.0.0"],
)