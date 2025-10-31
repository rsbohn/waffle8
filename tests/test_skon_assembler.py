"""Tests for SKON (Skip if Interrupt ON) assembler support."""

import subprocess
import sys
import tempfile
from pathlib import Path
import unittest


REPO_ROOT = Path(__file__).resolve().parents[1]
ASM_TOOL = REPO_ROOT / "tools" / "pdp8_asm.py"


class SKONAssemblerTests(unittest.TestCase):
    """Verify SKON mnemonic assembles to correct opcode (0o6002)."""

    def test_skon_mnemonic_recognizes(self) -> None:
        """SKON should assemble without error."""
        asm_source = """
*0000
    SKON
    HLT
"""
        with tempfile.TemporaryDirectory() as tmpdir:
            src_path = Path(tmpdir) / "test_skon.asm"
            src_path.write_text(asm_source)
            
            result = subprocess.run(
                [sys.executable, str(ASM_TOOL), "--list", "--list-only", str(src_path)],
                capture_output=True,
                text=True,
                check=False,
            )
            
            self.assertEqual(result.returncode, 0, msg=f"Assembly failed: {result.stderr}")
            self.assertIn("0 errors", result.stdout, "Expected successful assembly")

    def test_skon_assembles_to_6002(self) -> None:
        """SKON should assemble to octal 6002."""
        asm_source = """
*0000
    SKON
"""
        with tempfile.TemporaryDirectory() as tmpdir:
            src_path = Path(tmpdir) / "test_skon.asm"
            src_path.write_text(asm_source)
            
            result = subprocess.run(
                [sys.executable, str(ASM_TOOL), "--list", "--list-only", str(src_path)],
                capture_output=True,
                text=True,
                check=False,
            )
            
            self.assertEqual(result.returncode, 0, msg=f"Assembly failed: {result.stderr}")
            # Listing should show "6002" in the WORD column
            self.assertIn("6002  IOT", result.stdout, 
                         "SKON should assemble to 0o6002 (shown as 6002 in octal listing)")

    def test_ion_ioff_skon_together(self) -> None:
        """ION, IOFF, and SKON should work together."""
        asm_source = """
*0000
    IOFF
    ION
    SKON
    HLT
"""
        with tempfile.TemporaryDirectory() as tmpdir:
            src_path = Path(tmpdir) / "test_all.asm"
            src_path.write_text(asm_source)
            
            result = subprocess.run(
                [sys.executable, str(ASM_TOOL), "--list", "--list-only", str(src_path)],
                capture_output=True,
                text=True,
                check=False,
            )
            
            self.assertEqual(result.returncode, 0, msg=f"Assembly failed: {result.stderr}")
            # Check all three instructions
            self.assertIn("7400  IOFF", result.stdout, "IOFF should assemble to 0o7400")
            self.assertIn("7401  ION", result.stdout, "ION should assemble to 0o7401")
            self.assertIn("6002  IOT", result.stdout, "SKON should assemble to 0o6002")

    def test_skon_with_labels(self) -> None:
        """SKON should work with labels and jumps."""
        asm_source = """
*0000
START, ION
    SKON
    JMP LOOP
LOOP, TAD VALUE
    JMP START

*0100
VALUE, 0o0042
"""
        with tempfile.TemporaryDirectory() as tmpdir:
            src_path = Path(tmpdir) / "test_labels.asm"
            src_path.write_text(asm_source)
            
            result = subprocess.run(
                [sys.executable, str(ASM_TOOL), "--list", "--list-only", str(src_path)],
                capture_output=True,
                text=True,
                check=False,
            )
            
            self.assertEqual(result.returncode, 0, msg=f"Assembly failed: {result.stderr}")
            self.assertIn("0 errors", result.stdout)
            # Should show SKON as 6002
            self.assertIn("6002  IOT", result.stdout)


if __name__ == "__main__":
    unittest.main()
