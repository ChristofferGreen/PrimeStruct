# say hello
def greet(name: str) -> str:
    """Docstring with 'quotes'."""
    count = 3.5
    if name is None:
        return 'nobody'
    return f"hello {name}"
