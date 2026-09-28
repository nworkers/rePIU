"""Release data for the project site (Task 756).

Online, releases come from the GitHub REST API and are joined with the hand-written
notes in docs/release-notes/<tag>.md. Offline, the notes files alone are used, dated
from local git tags, with no downloadable assets.
"""

from __future__ import annotations

import json
import os
import re
import subprocess
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path

from content import LanguagePart, Renderer

VERSION_RE = re.compile(r"^v(\d+)\.(\d+)\.(\d+)$")
NEXT_LINK_RE = re.compile(r'<([^>]+)>;\s*rel="next"')


@dataclass
class Asset:
    name: str
    url: str
    size: int
    sha256: str | None
    kind: str  # package | report | other


@dataclass
class Release:
    tag: str
    date: str
    url: str
    prerelease: bool
    assets: list[Asset] = field(default_factory=list)
    has_notes_file: bool = False
    api_body: str = ""
    parts: dict[str, LanguagePart] = field(default_factory=dict)

    @property
    def package(self) -> Asset | None:
        return next((asset for asset in self.assets if asset.kind == "package"), None)

    @property
    def report(self) -> Asset | None:
        return next((asset for asset in self.assets if asset.kind == "report"), None)


def version_key(tag: str) -> tuple[int, int, int]:
    match = VERSION_RE.match(tag)
    return tuple(int(part) for part in match.groups()) if match else (0, 0, 0)


def asset_kind(name: str) -> str:
    if name.endswith("-win32.zip"):
        return "package"
    if name.startswith("openwatcom-samples-"):
        return "report"
    return "other"


def _get_json(url: str, token: str | None) -> tuple[object, str | None]:
    request = urllib.request.Request(url, headers={
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "repiu-site-build",
    })
    if token:
        request.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(request, timeout=30) as response:
        link = response.headers.get("Link")
        next_match = NEXT_LINK_RE.search(link or "")
        return json.load(response), next_match.group(1) if next_match else None


def fetch_api_releases(repo: str, token: str | None) -> list[dict]:
    url: str | None = f"https://api.github.com/repos/{repo}/releases?per_page=100"
    releases: list[dict] = []
    while url:
        page, url = _get_json(url, token)
        releases.extend(page)
    return [release for release in releases if not release.get("draft")]


def _git_tag_dates(repo_root: Path) -> dict[str, str]:
    """Commit date of every local tag, read in one git call."""
    try:
        result = subprocess.run(
            ["git", "for-each-ref", "refs/tags", "--format=%(refname:short) %(*committerdate:short)%(committerdate:short)"],
            cwd=repo_root, capture_output=True, text=True, check=False,
        )
    except OSError:
        return {}
    dates = {}
    for line in result.stdout.splitlines():
        tag, _, date = line.partition(" ")
        dates[tag] = date[:10]
    return dates


def load_releases(renderer: Renderer, repo_root: Path, repo: str, notes_dir: str,
                  languages: list[str], offline: bool) -> list[Release]:
    notes_root = repo_root / notes_dir
    notes = {path.stem: path for path in notes_root.glob("v*.md")}
    repo_url = f"https://github.com/{repo}"
    releases: list[Release] = []

    if offline:
        tag_dates = _git_tag_dates(repo_root)
        for tag in notes:
            releases.append(Release(
                tag=tag,
                date=tag_dates.get(tag, ""),
                url=f"{repo_url}/releases/tag/{tag}",
                prerelease=False,
            ))
    else:
        token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
        for item in fetch_api_releases(repo, token):
            assets = [
                Asset(
                    name=asset["name"],
                    url=asset["browser_download_url"],
                    size=int(asset.get("size") or 0),
                    sha256=(asset.get("digest") or "").removeprefix("sha256:") or None,
                    kind=asset_kind(asset["name"]),
                )
                for asset in item.get("assets", [])
            ]
            # Package first, then the report, then anything else.
            order = {"package": 0, "report": 1, "other": 2}
            assets.sort(key=lambda asset: (order[asset.kind], asset.name))
            release = Release(
                tag=item["tag_name"],
                date=(item.get("published_at") or item.get("created_at") or "")[:10],
                url=item["html_url"],
                prerelease=bool(item.get("prerelease")),
                assets=assets,
                api_body=item.get("body") or "",
            )
            releases.append(release)

    for release in releases:
        path = notes.get(release.tag)
        if path is not None:
            text = path.read_text(encoding="utf-8")
            source = f"{notes_dir.strip('/')}/{path.name}"
            release.has_notes_file = True
        else:
            text = release.api_body
            source = f"{notes_dir.strip('/')}/{release.tag}.md"
        if text.strip():
            release.parts, _ = renderer.bilingual(text, source, languages, release.tag)

    releases.sort(key=lambda release: (release.date, version_key(release.tag)), reverse=True)
    return releases
