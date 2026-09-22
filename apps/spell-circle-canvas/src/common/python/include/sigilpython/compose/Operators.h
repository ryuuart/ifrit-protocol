#pragma once

/** @file
 * Binding the operator seam: the readings that take a fact's value and
 * an operator from the shapes Python spells them as, so the verbs every
 * node states can be bound over them in the file that states the rest
 * of that vocabulary.
 */

#include <pybind11/pybind11.h>
#include <sigilcompose/core/Attributes.h>
#include <sigilcompose/core/Operator.h>

#include <string_view>
#include <vector>

namespace sigil::python {

/** States @p value under @p name on @p facts, in the type Python spells
 *  it as: a bool, a whole number, a number, a string, a list of
 *  strings, a list of numbers, or a Skia point. A SEQUENCE OF NUMBERS
 *  IS A LIST OF NUMBERS whatever its length, so a fact meant as a point
 *  is stated as a point; an empty list is a list of strings, since a
 *  fact's type is part of the fact and an empty one has to pick. */
void stateFact(compose::Attributes& facts, std::string_view name,
               pybind11::handle value);
/** The fact under @p name, in whichever of those types it was stated,
 *  or None where no fact of that name stands in one of them. A fact
 *  stated natively in another type reads as None: the type is part of
 *  the fact. */
pybind11::object factOf(const compose::Attributes& facts,
                        std::string_view name);
/** An operator read from @p value: the operator itself, a stock
 *  arranging value, a connecting record, or a Python object that
 *  arranges or adds. */
compose::Operator operatorValue(pybind11::handle value);
/** Every operator in @p value, a sequence of what `operatorValue`
 *  reads. */
std::vector<compose::Operator> operatorList(pybind11::handle value);
/** Whether @p value is a Python object an operator is built from: one
 *  carrying `arrange` or `add`. */
bool namesOperator(pybind11::handle value);

}  // namespace sigil::python
