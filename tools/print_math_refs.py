#!/usr/bin/env python3
"""Print reference math values for PrimeStruct doctests.

Usage:
  python3 tools/print_math_refs.py
  python3 tools/print_math_refs.py --values sin=0,0.5,1.0 cos=0,1.0

This script uses Python's `math` module as a source-of-truth to help
populate golden values in doctests. Record Python version when copying
numbers into tests.
"""

from __future__ import annotations

import argparse
import math
import sys
from typing import Callable, Dict, List, Tuple

DEFAULT_INPUTS = [
    0.0,
    0.5,
    1.0,
    -0.5,
    -1.0,
    math.pi / 6.0,
    math.pi / 4.0,
    math.pi / 2.0,
    math.pi,
    3.0 * math.pi / 2.0,
    2.0 * math.pi,
    10.0,
    20.0,
    100.0,
]

UNARY_FUNCS: Dict[str, Callable[[float], float]] = {
    "sin": math.sin,
    "cos": math.cos,
    "tan": math.tan,
    "asin": math.asin,
    "acos": math.acos,
    "atan": math.atan,
    "sinh": math.sinh,
    "cosh": math.cosh,
    "tanh": math.tanh,
    "asinh": math.asinh,
    "acosh": math.acosh,
    "atanh": math.atanh,
    "exp": math.exp,
    "exp2": math.exp2,
    "log": math.log,
    "log2": math.log2,
    "log10": math.log10,
    "sqrt": math.sqrt,
}

if hasattr(math, "cbrt"):
    UNARY_FUNCS["cbrt"] = math.cbrt  # Python 3.11+

BINARY_FUNCS: Dict[str, Callable[[float, float], float]] = {
    "atan2": math.atan2,
    "hypot": math.hypot,
    "pow": math.pow,
    "copysign": math.copysign,
    "fma": math.fma,
}


def parse_values_arg(raw: str) -> Dict[str, List[float]]:
    """Parse --values like: sin=0,0.5,1.0 cos=0,1.0"""
    result: Dict[str, List[float]] = {}
    for chunk in raw.split():
        if "=" not in chunk:
            raise ValueError(f"Invalid --values entry: {chunk}")
        name, values = chunk.split("=", 1)
        if not values:
            raise ValueError(f"No values for {name}")
        result[name] = [float(v) for v in values.split(",")]
    return result


def print_unary(name: str, func: Callable[[float], float], inputs: List[float]) -> None:
    print(f"{name}:")
    for value in inputs:
        try:
            out = func(value)
        except ValueError:
            out = float("nan")
        print(f"  {value:.12g} -> {out:.17g}")


def print_binary(
    name: str, func: Callable[[float, float], float], pairs: List[Tuple[float, float]]
) -> None:
    print(f"{name}:")
    for a, b in pairs:
        try:
            out = func(a, b)
        except ValueError:
            out = float("nan")
        print(f"  ({a:.12g}, {b:.12g}) -> {out:.17g}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--values",
        help="Override default inputs: e.g. 'sin=0,0.5 cos=0,1.0'",
    )
    args = parser.parse_args()

    if args.values:
        override = parse_values_arg(args.values)
    else:
        override = {}

    print("# Python", sys.version.split()[0])
    print("# math module reference values")

    for name, func in sorted(UNARY_FUNCS.items()):
        inputs = override.get(name, DEFAULT_INPUTS)
        print_unary(name, func, inputs)
        print()

    default_pairs = [(0.0, 1.0), (1.0, 1.0), (-1.0, 1.0), (1.0, -1.0), (3.0, 4.0)]
    for name, func in sorted(BINARY_FUNCS.items()):
        pairs_list = default_pairs
        print_binary(name, func, pairs_list)
        print()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
