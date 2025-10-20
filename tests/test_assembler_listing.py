import subprocess
import sys
import tempfile
from pathlib import Path
import unittest


REPO_ROOT = Path(__file__).resolve().parents[1]
ASM_TOOL = REPO_ROOT / "tools" / "pdp8_asm.py"
DEMO_SOURCE = REPO_ROOT / "demo" / "core.asm"


class PDP8AssemblerListingTests(unittest.TestCase):
    def test_listing_contains_expected_lines(self) -> None:
        result = subprocess.run(
            [sys.executable, str(ASM_TOOL), "--list", "--list-only", str(DEMO_SOURCE)],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn("PDP-8 Assembly Listing", result.stdout)
        self.assertIn("7000  7300  CLA", result.stdout)
        self.assertIn("Totals: 48 words, 0 errors", result.stdout)

    def test_list_with_output_path_writes_srec(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            out_path = Path(tmpdir) / "core.srec"
            result = subprocess.run(
                [
                    sys.executable,
                    str(ASM_TOOL),
                    "--list",
                    str(DEMO_SOURCE),
                    "--output",
                    str(out_path),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, msg=result.stderr)
            self.assertIn("PDP-8 Assembly Listing", result.stdout)
            self.assertTrue(out_path.exists(), "S-record output not created")
            self.assertTrue(out_path.read_text().startswith("S1"))


if __name__ == "__main__":
    unittest.main()
