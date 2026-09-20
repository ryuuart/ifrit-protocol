"""The restricted renderer, the front matter, and the page composition."""

import unittest

from sigil.reference import markdown, pages


def render(text: str, values=None, depth: int = 0) -> str:
    return markdown.render(text, markdown.Links(depth, values or {}))


class FrontMatterIsOneKeyPerLine(unittest.TestCase):
    def test(self) -> None:
        front, body = markdown.front_matter(
            "---\nkind: verb\ncommon_verbs: [fill, ink]\n---\nprose\n"
        )
        self.assertEqual(front["kind"], "verb")
        self.assertEqual(front["common_verbs"], ["fill", "ink"])
        self.assertEqual(body.strip(), "prose")

    def test_a_page_without_it_is_all_body(self) -> None:
        front, body = markdown.front_matter("prose\n")
        self.assertEqual(front, {})
        self.assertEqual(body, "prose\n")


class TheGrammarIsTheOneAPageNeeds(unittest.TestCase):
    def test_headings_carry_an_identifier(self) -> None:
        self.assertEqual(render("## Make one"), '<h2 id="make-one">Make one</h2>')

    def test_a_table(self) -> None:
        out = render("| a | b |\n| --- | --- |\n| 1 | `2` |")
        self.assertIn("<th>a</th>", out)
        self.assertIn("<td><code>2</code></td>", out)

    def test_lists_and_quotes(self) -> None:
        self.assertEqual(render("- one\n- two"), "<ul><li>one</li><li>two</li></ul>")
        self.assertEqual(render("1. one"), "<ol><li>one</li></ol>")
        self.assertIn("<blockquote><p>held</p></blockquote>", render("> held"))

    def test_spans(self) -> None:
        out = render("a **b** and *c* and `d`")
        self.assertEqual(
            out, "<p>a <strong>b</strong> and <em>c</em> and <code>d</code></p>"
        )

    def test_markup_is_escaped(self) -> None:
        self.assertEqual(render("<b>no</b>"), "<p>&lt;b&gt;no&lt;/b&gt;</p>")


class APairOfFencesBecomesOneWidget(unittest.TestCase):
    def test(self) -> None:
        out = render("```cpp\none();\n```\n\n```python\none()\n```")
        self.assertIn('class="languages"', out)
        self.assertEqual(out.count('class="tab"'), 2)
        self.assertIn('data-language="python"', out)

    def test_one_fence_alone_is_a_plain_block(self) -> None:
        out = render("```cpp\none();\n```")
        self.assertIn('<pre class="code" data-language="cpp">', out)
        self.assertNotIn("languages", out)


class ALinkShorthandResolvesFromWhereThePageStands(unittest.TestCase):
    def test(self) -> None:
        values = {"sigil::compose::Fill": "values/sigil.compose.Fill.html"}
        out = render("[F](value:sigil::compose::Fill)", values, depth=3)
        self.assertIn('href="../../../values/sigil.compose.Fill.html"', out)

    def test_a_page_and_a_guide(self) -> None:
        out = render(
            "[a](page:SigilCompose/verbs/fill) [b](guide:first-drawing)", depth=1
        )
        self.assertIn('href="../reference/SigilCompose/verbs/fill.html"', out)
        self.assertIn('href="../guides/first-drawing.html"', out)

    def test_an_absolute_site_path_is_made_relative(self) -> None:
        out = render("[a](/values/x.html)", depth=2)
        self.assertIn('href="../../values/x.html"', out)

    def test_an_unknown_value_is_reported_rather_than_linked_wrongly(self) -> None:
        links = markdown.Links(1, {})
        markdown.render("[F](value:sigil::nowhere::Fill)", links)
        self.assertEqual(links.unresolved, ["value:sigil::nowhere::Fill"])


class ADirectiveIsHandedToTheGenerator(unittest.TestCase):
    def test(self) -> None:
        out = markdown.render(
            "<!-- example: fill_verb -->",
            markdown.Links(0),
            {"example": lambda stem: f"<figure>{stem}</figure>"},
        )
        self.assertEqual(out, "<figure>fill_verb</figure>")


class APageKeepsWhatTheWriterWrote(unittest.TestCase):
    def test_sections_split_on_their_headings(self) -> None:
        lede, sections = pages.sections_of("the lede\n\n## Description\n\nheld\n")
        self.assertEqual(lede, "the lede")
        self.assertEqual(sections["Description"], "held")

    def test_a_first_sentence_stops_at_the_stop(self) -> None:
        self.assertEqual(pages.first_sentence("One. Two."), "One.")
        self.assertEqual(pages.first_sentence("No stop"), "No stop")

    def test_a_template_parameter_is_not_a_value_a_reader_can_make(self) -> None:
        self.assertTrue(pages._is_template_parameter("P &&"))
        self.assertTrue(pages._is_template_parameter("const T &"))
        self.assertFalse(pages._is_template_parameter("Fill"))


if __name__ == "__main__":
    unittest.main()
