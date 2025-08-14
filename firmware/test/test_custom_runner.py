"""Custom PlatformIO test runner for MOARkNOBS-42.

Parses Unity results coming over the project's Serial1-based transport.
"""

from __future__ import annotations

import re
import click
from platformio.public import TestRunnerBase
from platformio.test.result import TestCase, TestCaseSource, TestStatus


class CustomTestRunner(TestRunnerBase):
    """Minimal custom runner wired for Unity output.

    Only overrides the line handler to convert Unity's TAP-ish output into
    PlatformIO's :class:`TestCase` objects.
    """

    TESTCASE_PARSE_RE = re.compile(
        r"(?P<source_file>[^:]+):(?P<source_line>\d+):(?P<name>[^\s]+):"
        r"(?P<status>PASS|IGNORE|FAIL)(:\s*(?P<message>.+)$)?"
    )

    def stage_uploading(self) -> None:
        """Hook where you could flash a remote board or simulator.

        The default behaviour (calling `platformio run -t upload`) works for
        local hardware, so we just defer to the parent implementation. This
        makes it obvious where to hijack the pipeline if CI ever needs to push
        binaries somewhere else.
        """

        return super().stage_uploading()

    def on_testing_line_output(self, line: str) -> None:
        """Parse Unity output lines and feed cases into the test suite."""

        if self.options and self.options.verbose:
            click.echo(line, nl=False)

        cleaned = (line or "").strip()
        if not cleaned:
            return

        match = self.TESTCASE_PARSE_RE.search(cleaned)
        if match:
            data = match.groupdict()
            source = TestCaseSource(
                filename=data["source_file"],
                line=int(data["source_line"]),
            )
            case = TestCase(
                name=data["name"],
                status=TestStatus.from_string(data["status"]),
                message=(data.get("message") or "").strip() or None,
                stdout=cleaned,
                source=source,
            )
            self.test_suite.add_case(case)
            if not self.options.verbose:
                click.echo(case.humanize())
        elif all(tok in cleaned for tok in ("Tests", "Failures", "Ignored")):
            self.test_suite.on_finish()
        else:
            click.echo(cleaned)
