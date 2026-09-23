"""Semantic document components styled by the native inherited stylesheet.

A prose component returns the native text leaf it sets, and one that gathers
prose returns the plain element it gathers into. Headings and prose carry
roles; a stylesheet rule with the same name styles that role throughout its
subtree. Explicit classes and fluent font/paragraph declarations take
precedence.
"""
