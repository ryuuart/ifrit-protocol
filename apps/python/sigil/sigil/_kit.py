"""Keyword adaptation shared by the two native kit libraries."""


def specification(record_type, value, properties):
    """Copy a native props value and apply snake_case keyword fields."""
    if value is None:
        result = record_type()
    elif isinstance(value, record_type):
        result = value.copy()
    else:
        raise TypeError(f"expected {record_type.__name__}, got {type(value).__name__}")
    for name, item in properties.items():
        first, *rest = name.split("_")
        field = first + "".join(part.title() for part in rest)
        descriptor = getattr(record_type, field, None)
        if not isinstance(descriptor, property):
            raise TypeError(f"unknown {record_type.__name__} property {name!r}")
        setattr(result, field, item)
    return result
