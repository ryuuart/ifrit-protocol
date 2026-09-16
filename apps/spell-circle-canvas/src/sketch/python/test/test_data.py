"""Native data conversion, missing cells, query ownership and asset snapshots."""

import builtins
import gc
import sqlite3
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

from sigil import data
from sigil.native import data as native
from sigil.sketch import render_file


class Data(unittest.TestCase):
    def test_json_preserves_native_order_duplicates_and_owned_members(self):
        self.assertIs(data.Json, native.Json)
        document = data.decodeJson(
            '{"name":"fern","values":[1,null,true],"name":"moss"}'
        )
        self.assertEqual(
            [name for name, _ in document.fields()], ["name", "values", "name"]
        )
        self.assertEqual(document["name"].text(), "fern")
        self.assertTrue(document["absent"]["deeper"].null())
        self.assertEqual(document["name"].number(8), 8)
        values = document["values"].items()
        encoded = data.encodeJson(document)
        self.assertEqual(data.decodeJson(encoded), document)
        self.assertEqual(
            document.to_python(), {"name": "fern", "values": [1, None, True]}
        )
        del document
        gc.collect()
        self.assertEqual([value.to_python() for value in values], [1, None, True])
        duplicate = data.Json.object([("x", 2), ("x", 4)])
        self.assertEqual(data.encodeJson(duplicate), '{"x":2,"x":4}')
        self.assertIsNone(data.decodeJson("not json"))
        repeated = [1, {"x": 2}]
        self.assertEqual(data.Json([repeated, repeated]).size(), 2)
        cycle = []
        cycle.append(cycle)
        with self.assertRaisesRegex(ValueError, "recursive"):
            data.Json(cycle)
        with self.assertRaises(TypeError):
            data.Json({1: "key must be text"})

    def test_decoded_json_respects_python_conversion_recursion_limit(self):
        document = data.decodeJson("[" * 180 + "0" + "]" * 180)
        self.assertIsNotNone(document)
        nested = 0
        for _ in range(180):
            nested = [nested]
        previous = sys.getrecursionlimit()
        try:
            sys.setrecursionlimit(100)
            with self.assertRaises(RecursionError):
                document.to_python()
            with self.assertRaises(RecursionError):
                data.Json(nested)
        finally:
            sys.setrecursionlimit(previous)

    def test_table_reshaping_keeps_native_types_and_missing_cells(self):
        table = data.decodeCsv(
            "region,count,open,date\nwest,4,true,2026-09-01\neast,,false,2026-09-02\nwest,2,true,2026-09-03\n"
        )
        self.assertIsInstance(table, native.Table)
        self.assertEqual(table.column("count").type(), data.ColumnType.Number)
        self.assertEqual(table.column("count").values(), [4, None, 2])
        self.assertEqual(table.column("open").values(), [True, False, True])
        self.assertIsInstance(table.cell("date", 0), data.Instant)
        self.assertEqual(
            table.sort("count", data.Order.Descending).column("count").values(),
            [4, 2, None],
        )
        self.assertEqual(
            table.filter(lambda row, table=table: table.cell("region", row) == "west")
            .column("count")
            .values(),
            [4, 2],
        )
        groups = table.group("region")
        self.assertEqual(
            [(group.key, group.rows) for group in groups],
            [("west", [0, 2]), ("east", [1])],
        )
        self.assertEqual(table.take(groups[0].rows).size(), 2)
        selected = table.select(["date", "open"])
        self.assertEqual(
            [column.name() for column in selected.columns()], ["date", "open"]
        )
        held = table.column("count")
        table.remove("count")
        del table
        gc.collect()
        self.assertEqual(held.values(), [4, None, 2])
        self.assertTrue(held.missing(100))
        self.assertIsNone(held[100])

    def test_columns_and_derived_cells_copy_python_storage(self):
        values = [3, None, 7]
        column = data.Column("n", values)
        values[0] = 99
        self.assertEqual(column.values(), [3, None, 7])
        table = data.Table([column, data.Column("enabled", [True, False, None])])
        table.derive(
            "twice", lambda row: None if column.missing(row) else column.at(row) * 2
        )
        self.assertEqual(table.column("twice").values(), [6, None, 14])
        table.add("when", [data.Instant(0), None, data.Instant(60)])
        self.assertEqual(table.column("when").type(), data.ColumnType.Time)
        self.assertEqual(
            table.row(1), {"n": None, "enabled": False, "twice": None, "when": None}
        )
        rectangular = data.tableFromJson([{"x": 1, "y": "a"}, {"x": 2}])
        self.assertEqual(rectangular.column("y").values(), ["a", None])
        self.assertIsNone(data.tableFromJson(4))
        self.assertEqual(
            data.decodeCsv("a;b\n1;2", data.CsvOptions(delimiter=";")).cell("b", 0), 2
        )

    def test_native_scale_mapping_ticks_and_embedded_interval_lifetime(self):
        self.assertIs(data.Scale, native.Scale)
        scale = data.Scale(domain=(0, 100), range=(20, 420))
        self.assertEqual(scale(25), 120)
        self.assertEqual(scale.invert(120), 25)
        self.assertEqual(
            scale.through(50, lambda position: (position, 1 - position)), (0.5, 0.5)
        )
        self.assertEqual(scale.ticks(5), [0, 20, 40, 60, 80, 100])
        self.assertEqual(scale.tickStep(5), 20)
        logarithmic = data.Scale(domain=(1, 100), transform=data.Transform.Log)
        self.assertAlmostEqual(logarithmic(10), 0.5)
        bands = data.Scale(range=(0, 120), transform=data.Transform.Band, steps=3)
        self.assertEqual([bands(index) for index in range(3)], [0, 40, 80])
        self.assertEqual(bands.bandwidth(), 40)
        self.assertEqual(bands.slot(100), 2)
        held = scale.domain
        scale.domain = (10, 110)
        del scale
        gc.collect()
        self.assertEqual((held.low, held.high), (10, 110))
        nice = data.Scale(domain=(2.3, 17.6)).nice(5)
        self.assertLessEqual(nice.domain.low, 2.3)
        self.assertGreaterEqual(nice.domain.high, 17.6)

    def test_database_queries_return_independent_tables(self):
        with self.assertRaises(RuntimeError):
            data.Database.fromBytes(b"")
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "field.sqlite"
            with sqlite3.connect(path) as store:
                store.execute(
                    "CREATE TABLE plants (name TEXT, height REAL, watered BOOL)"
                )
                store.executemany(
                    "INSERT INTO plants VALUES (?, ?, ?)",
                    [("fern", 42, True), ("moss", None, False)],
                )
            query = data.Database.fromBytes(path.read_bytes())
            table = query.query("SELECT * FROM plants ORDER BY name")
            self.assertEqual(query.engine(), data.Engine.Sqlite)
            self.assertEqual(table.column("height").values(), [42, None])
            self.assertEqual(table.column("watered").values(), [True, False])
            with self.assertRaises(RuntimeError):
                query.query("SELECT missing FROM plants")
            del query
            gc.collect()
            self.assertEqual(table.column("name").values(), ["fern", "moss"])
            self.assertEqual(
                data.Database.open(path)
                .query("SELECT count(*) AS count FROM plants")
                .cell("count", 0),
                2,
            )
        self.assertEqual(table.size(), 2)

    def test_asset_json_tables_and_queries_outlive_the_session(self):
        self.addCleanup(lambda: builtins.__dict__.pop("_sigil_data", None))
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "config.json").write_text(
                '{"title":"field measurements","units":"cm"}'
            )
            (root / "rows.csv").write_text("species,height\nfern,42\nmoss,3\n")
            with sqlite3.connect(root / "field.sqlite") as store:
                store.execute("CREATE TABLE totals (count REAL)")
                store.execute("INSERT INTO totals VALUES (2)")
            entry = root / "scene.py"
            entry.write_text(
                textwrap.dedent("""
                import builtins
                from sigil.compose import box
                from sigil.sketch import sketch
                @sketch(size=(24, 24), capture_at=0)
                class Resources:
                    def setup(self, ctx):
                        assets = ctx.assets
                        document = assets.json(ctx.local("config.json"))
                        table = assets.table(ctx.local("rows.csv"))
                        database = assets.database(ctx.local("field.sqlite"))
                        assert assets.json(ctx.local("missing.json")) is None
                        assert assets.table(ctx.local("missing.csv")) is None
                        assert assets.database(ctx.local("missing.sqlite")) is None
                        builtins._sigil_data = (assets, document, table, database)
                        ctx.render(box(width=24, height=24, fill="#45827b"))
            """)
            )
            render_file(entry, root / "frame.png", at=0)
            assets, document, table, database = builtins._sigil_data
            gc.collect()
            with self.assertRaisesRegex(RuntimeError, "session"):
                assets.json("anything")
            self.assertEqual(document["title"].text(), "field measurements")
            self.assertEqual(table.column("height").values(), [42, 3])
            self.assertEqual(
                database.query("SELECT count FROM totals").cell("count", 0), 2
            )
            table.remove("height")
        self.assertEqual(document["units"].text(), "cm")


if __name__ == "__main__":
    unittest.main()
