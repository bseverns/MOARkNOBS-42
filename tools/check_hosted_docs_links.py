#!/usr/bin/env python3
"""Check same-origin links on a hosted MkDocs site."""

from __future__ import annotations

import argparse
import html.parser
import sys
import urllib.error
import urllib.parse
import urllib.request
from collections import deque


class LinkParser(html.parser.HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.links: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if tag not in {"a", "link", "script", "img"}:
            return
        for name, value in attrs:
            if name in {"href", "src"} and value:
                self.links.append(value)


def normalize_url(base: str, link: str) -> str | None:
    if link.startswith(("mailto:", "tel:", "javascript:", "data:")):
        return None
    url = urllib.parse.urljoin(base, link)
    parsed = urllib.parse.urlparse(url)
    parsed = parsed._replace(fragment="")
    return urllib.parse.urlunparse(parsed)


def fetch(url: str, timeout: float) -> tuple[int, str]:
    request = urllib.request.Request(url, headers={"User-Agent": "mn42-doc-link-check/1.0"})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        body = response.read().decode("utf-8", errors="replace")
        return response.status, body


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base_url", help="Hosted docs base URL, e.g. https://user.github.io/repo/")
    parser.add_argument("--max-pages", type=int, default=80)
    parser.add_argument("--timeout", type=float, default=10.0)
    args = parser.parse_args()

    base_url = args.base_url.rstrip("/") + "/"
    base = urllib.parse.urlparse(base_url)
    queue: deque[str] = deque([base_url])
    seen: set[str] = set()
    broken: list[tuple[str, str, str]] = []
    raw_markdown_links: list[tuple[str, str]] = []
    stale_repo_links: list[tuple[str, str]] = []

    while queue and len(seen) < args.max_pages:
        page_url = queue.popleft()
        if page_url in seen:
            continue
        seen.add(page_url)
        try:
            status, body = fetch(page_url, args.timeout)
        except Exception as exc:  # noqa: BLE001 - CLI should report exact URL failures.
            broken.append((page_url, page_url, str(exc)))
            continue
        if status >= 400:
            broken.append((page_url, page_url, f"HTTP {status}"))
            continue

        links = LinkParser()
        links.feed(body)
        for link in links.links:
            url = normalize_url(page_url, link)
            if not url:
                continue
            parsed = urllib.parse.urlparse(url)
            same_site = parsed.scheme == base.scheme and parsed.netloc == base.netloc
            same_path = parsed.path.startswith(base.path)
            if same_site and same_path and parsed.path.endswith(".md"):
                raw_markdown_links.append((page_url, link))
            if "github.com/bseverns/benzknober" in url or "/edit/master/" in url:
                stale_repo_links.append((page_url, url))
            if not same_site or not same_path:
                continue
            if parsed.path.endswith((".png", ".jpg", ".jpeg", ".svg", ".css", ".js", ".ico", ".json")):
                continue
            if url not in seen and len(seen) + len(queue) < args.max_pages:
                queue.append(url)

    for page, link in raw_markdown_links:
        print(f"raw markdown href on hosted page: {page} -> {link}")
    for page, link in stale_repo_links:
        print(f"stale repo/edit href on hosted page: {page} -> {link}")
    for page, link, reason in broken:
        print(f"broken hosted link: {page} -> {link} ({reason})")

    print(f"checked {len(seen)} hosted page(s) from {base_url}")
    return 1 if broken or raw_markdown_links or stale_repo_links else 0


if __name__ == "__main__":
    sys.exit(main())
