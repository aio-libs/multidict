import gc

import objgraph  # type: ignore[import-untyped]

from multidict import CIMultiDict, MultiDict


class SubtractionValue:
    pass


class ReflectedUnionValue:
    pass


class SubtractionOperandValue:
    pass


class ReflectedUnionOperandValue:
    pass


def _run_isolated_case() -> None:
    for _ in range(100):
        subtraction_value = SubtractionValue()
        subtraction_md = MultiDict([("key", subtraction_value)])
        subtraction_result = subtraction_md.items() - set()
        del subtraction_result, subtraction_md, subtraction_value

    for _ in range(100):
        reflected_union_value = ReflectedUnionValue()
        reflected_union_md = MultiDict([("key", reflected_union_value)])
        reflected_union_result = set() | reflected_union_md.items()
        del reflected_union_result, reflected_union_md, reflected_union_value

    # Non-empty operands exercise the loop parsing the operand's items.
    for md_cls in (MultiDict, CIMultiDict):
        md = md_cls([("key", "value")])
        for i in range(100):
            sub_operand = [(f"k{i}", SubtractionOperandValue())]
            sub_result = md.items() - sub_operand
            union_operand = [(f"k{i}", ReflectedUnionOperandValue())]
            union_result = union_operand | md.items()
            del sub_operand, sub_result, union_operand, union_result

    gc.collect()
    leaked_subtraction = len(objgraph.by_type("SubtractionValue"))
    leaked_reflected_union = len(objgraph.by_type("ReflectedUnionValue"))
    leaked_subtraction_operand = len(objgraph.by_type("SubtractionOperandValue"))
    leaked_reflected_union_operand = len(objgraph.by_type("ReflectedUnionOperandValue"))
    assert leaked_subtraction == 0, (
        f"{leaked_subtraction} subtraction values not collected by GC"
    )
    assert leaked_reflected_union == 0, (
        f"{leaked_reflected_union} reflected union values not collected by GC"
    )
    assert leaked_subtraction_operand == 0, (
        f"{leaked_subtraction_operand} subtraction operand values not collected by GC"
    )
    assert leaked_reflected_union_operand == 0, (
        f"{leaked_reflected_union_operand} reflected union operand values "
        "not collected by GC"
    )


if __name__ == "__main__":
    _run_isolated_case()
