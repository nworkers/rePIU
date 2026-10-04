#!/usr/bin/env python3
"""Build the rePIU project site into a static directory (Task 756).

Usage:
    python scripts/site/build_site.py [--out build/site] [--site-url URL] [--offline]

Sources are docs/sites/ (templates, strings, static files), docs/post/,
docs/release-notes/ and the GitHub Releases API. The build fails when any page links
to a file that is not in the output.
"""

from __future__ import annotations

import argparse
import datetime
import posixpath
import shutil
import sys
import tomllib
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit

from jinja2 import Environment, FileSystemLoader, StrictUndefined, select_autoescape

from content import SITE_ROOT_MARKER, Renderer, load_posts, load_targets
from releases import load_releases

REPO_ROOT = Path(__file__).resolve().parents[2]
SITE_DIR = REPO_ROOT / "docs" / "sites"
PAGES = ("index", "wip", "download", "credits")
CREDIT_GROUPS = ("runtime", "dev", "site")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", type=Path, default=REPO_ROOT / "build" / "site")
    parser.add_argument("--site-url", default=None,
                        help="Public base URL, e.g. https://nworkers.github.io/rePIU/")
    parser.add_argument("--offline", action="store_true",
                        help="Do not call the GitHub API; use release-notes files only")
    return parser.parse_args()


def load_toml(path: Path) -> dict:
    with path.open("rb") as handle:
        return tomllib.load(handle)


def rel(target: str, current: str) -> str:
    """Relative URL from the page at `current` to the site path `target`."""
    start = posixpath.dirname(current) or "."
    return posixpath.relpath(target, start)


def root_of(current: str) -> str:
    depth = current.count("/")
    return "../" * depth


def dir_prefix(target_dir: str, current: str) -> str:
    """Prefix that turns a file name in `target_dir` into a URL relative to `current`."""
    relative = posixpath.relpath(target_dir or ".", posixpath.dirname(current) or ".")
    return "" if relative == "." else relative + "/"


class Site:
    def __init__(self, args: argparse.Namespace):
        self.config = load_toml(SITE_DIR / "site.toml")
        site = self.config["site"]
        self.languages: list[str] = site["languages"]
        self.default_lang: str = site["default_lang"]
        self.repo: str = site["repo"]
        self.branch: str = site["branch"]
        self.repo_url = f"https://github.com/{self.repo}"
        self.site_url = (args.site_url or site["default_url"]).rstrip("/") + "/"
        self.out: Path = args.out.resolve()
        self.strings = {lang: load_toml(SITE_DIR / "i18n" / f"{lang}.toml")
                        for lang in self.languages}
        self.build_date = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d")

        sources = self.config["sources"]
        self.renderer = Renderer(self.repo, self.branch, REPO_ROOT, sources["posts"])
        self.posts = load_posts(self.renderer, REPO_ROOT, sources["posts"], self.languages)
        self.targets = load_targets(REPO_ROOT / sources["target_catalog"])
        self.releases = load_releases(self.renderer, REPO_ROOT, self.repo,
                                      sources["release_notes"], self.languages, args.offline)
        stable = [release for release in self.releases if not release.prerelease]
        self.latest = stable[0] if stable else None
        version_file = (REPO_ROOT / "VERSION").read_text(encoding="utf-8").strip()
        self.latest_version = self.latest.tag.lstrip("v") if self.latest else version_file

        self.env = Environment(
            loader=FileSystemLoader(SITE_DIR / "templates"),
            autoescape=select_autoescape(["html"]),
            undefined=StrictUndefined,
            trim_blocks=True,
            lstrip_blocks=True,
        )

    # -- paths ------------------------------------------------------------------------

    def prefix(self, lang: str) -> str:
        return "" if lang == self.default_lang else f"{lang}/"

    def page_path(self, lang: str, name: str) -> str:
        return f"{self.prefix(lang)}{name}"

    def public_url(self, path: str) -> str:
        return self.site_url + (path[:-len("index.html")] if path.endswith("index.html") else path)

    # -- rendering --------------------------------------------------------------------

    def base_context(self, lang: str, path: str, page: str, counterpart: str) -> dict:
        """`counterpart` is the page path without the language prefix."""
        t = self.strings[lang]
        root = root_of(path)
        lang_links = []
        alternates = []
        for other in self.languages:
            other_path = self.page_path(other, counterpart)
            lang_links.append({
                "lang": other,
                "short": self.strings[other]["meta"]["short"],
                "label": self.strings[other]["meta"]["label"],
                "href": rel(other_path, path),
                "current": other == lang,
            })
            alternates.append({"hreflang": other, "href": self.public_url(other_path)})
        alternates.append({"hreflang": "x-default",
                           "href": self.public_url(self.page_path(self.default_lang, counterpart))})
        redirect = None
        if lang == self.default_lang and "en" in self.languages and lang != "en":
            redirect = rel(self.page_path("en", counterpart), path)
        return {
            "t": t,
            "lang": lang,
            "page": page,
            "root": root,
            "home": dir_prefix(self.prefix(lang), path),
            "static": f"{root}static/",
            "repo_url": self.repo_url,
            "branch": self.branch,
            "lang_links": lang_links,
            "alternates": alternates,
            "canonical": self.public_url(path),
            "first_visit_redirect": redirect,
            "description": t["meta"]["description"],
            "build_date": self.build_date,
            "latest_version": self.latest_version,
            "has_mermaid": False,
            "mermaid_url": self.config["mermaid"]["url"],
        }

    def release_view(self, release, lang: str, root: str) -> dict:
        part = release.parts.get(lang)
        return {
            "tag": release.tag,
            "date": release.date,
            "url": release.url,
            "assets": release.assets,
            "package": release.package,
            "report": release.report,
            "summary_html": part.summary_html.replace(SITE_ROOT_MARKER, root) if part else "",
            "body_html": part.body_html.replace(SITE_ROOT_MARKER, root) if part else "",
            "has_mermaid": part.has_mermaid if part else False,
        }

    def write(self, path: str, template: str, context: dict) -> None:
        html = self.env.get_template(template).render(**context)
        target = self.out / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(html, encoding="utf-8", newline="\n")

    def build(self) -> None:
        if self.out.exists():
            if self.out in (REPO_ROOT, REPO_ROOT / "docs") or SITE_DIR.is_relative_to(self.out):
                raise SystemExit(f"Refusing to clear {self.out}: it contains sources.")
            shutil.rmtree(self.out)
        self.out.mkdir(parents=True)
        shutil.copytree(SITE_DIR / "static", self.out / "static")
        # Task 770: the README's screenshots, shared rather than copied into static/.
        screenshots = REPO_ROOT / "docs" / "screenshots"
        if screenshots.is_dir():
            shutil.copytree(screenshots, self.out / "screenshots")

        for lang in self.languages:
            t = self.strings[lang]
            for page in PAGES:
                path = self.page_path(lang, f"{page}.html")
                context = self.base_context(lang, path, page, f"{page}.html")
                root = context["root"]
                releases = [self.release_view(release, lang, root) for release in self.releases]
                titles = {"index": "rePIU", "wip": f"{t['wip']['title']} — rePIU",
                          "download": f"{t['download']['title']} — rePIU",
                          "credits": f"{t['credits']['title']} — rePIU"}
                context["page_title"] = titles[page]
                if page == "index":
                    context["targets"] = self.targets
                    context["platforms"] = self.config["platforms"]
                    context["screenshots"] = self.config.get("screenshots", [])
                elif page == "wip":
                    context["posts"] = [self.post_view(post, lang) for post in self.posts]
                    context["releases"] = releases
                    context["has_mermaid"] = any(release["has_mermaid"] for release in releases)
                elif page == "download":
                    latest = next((view for view in releases
                                   if self.latest and view["tag"] == self.latest.tag), None)
                    context["latest"] = latest
                    context["older"] = [view for view in releases if view is not latest]
                    context["has_mermaid"] = bool(latest and latest["has_mermaid"])
                elif page == "credits":
                    entries = self.config.get("credits", [])
                    context["credit_groups"] = [
                        {"key": key, "items": items}
                        for key in CREDIT_GROUPS
                        if (items := [entry for entry in entries if entry["group"] == key])
                    ]
                self.write(path, f"{page}.html", context)

            views = [self.post_view(post, lang) for post in self.posts]
            for index, post in enumerate(self.posts):
                name = f"posts/{post.slug}.html"
                path = self.page_path(lang, name)
                context = self.base_context(lang, path, "post", name)
                view = views[index]
                view["body_html"] = post.parts[lang].body_html.replace(SITE_ROOT_MARKER,
                                                                       context["root"])
                context.update({
                    "page_title": f"{view['title_text']} — rePIU",
                    "description": (view["excerpt"][:157] + "...") if len(view["excerpt"]) > 160
                                   else (view["excerpt"] or context["description"]),
                    "post": view,
                    "newer": views[index - 1] if index > 0 else None,
                    "older": views[index + 1] if index + 1 < len(views) else None,
                    "has_mermaid": post.parts[lang].has_mermaid,
                })
                self.write(path, "post.html", context)

        self.build_404()

    def post_view(self, post, lang: str) -> dict:
        part = post.parts[lang]
        return {
            "slug": post.slug,
            "date": post.date,
            "title_text": part.title_text,
            "title_html": part.title_html,
            "excerpt": part.excerpt,
            "source_url": f"{self.repo_url}/blob/{self.branch}/{post.source}",
        }

    def build_404(self) -> None:
        # GitHub Pages serves 404.html for any missing path at any depth: absolute URLs only.
        lang = self.default_lang
        context = self.base_context(lang, "404.html", "404", "index.html")
        context.update({
            "page_title": "404 — rePIU",
            "static": f"{self.site_url}static/",
            "home": self.site_url,
            "canonical": None,
            "alternates": [],
            "first_visit_redirect": None,
            "lang_links": [
                {"lang": other, "short": self.strings[other]["meta"]["short"],
                 "label": self.strings[other]["meta"]["label"],
                 "href": self.site_url + self.prefix(other), "current": other == lang}
                for other in self.languages
            ],
            "notfound_langs": [
                {"t": self.strings[other], "home": self.site_url + self.prefix(other)}
                for other in self.languages
            ],
        })
        self.write("404.html", "404.html", context)


class LinkCollector(HTMLParser):
    def __init__(self):
        super().__init__()
        self.links: list[str] = []

    def handle_starttag(self, tag, attrs):
        for name, value in attrs:
            if name in ("href", "src") and value:
                self.links.append(value)


def check_links(out: Path) -> list[str]:
    """Every relative href/src must name a file in the output."""
    broken = []
    for page in sorted(out.rglob("*.html")):
        collector = LinkCollector()
        collector.feed(page.read_text(encoding="utf-8"))
        for link in collector.links:
            parts = urlsplit(link)
            if parts.scheme or link.startswith("//") or link.startswith("#") or not parts.path:
                continue
            target = (page.parent / unquote(parts.path)).resolve()
            if parts.path.endswith("/"):
                target = target / "index.html"
            if not target.is_file():
                broken.append(f"{page.relative_to(out).as_posix()}: {link}")
    return broken


def main() -> int:
    args = parse_args()
    site = Site(args)
    site.build()

    print(f"site: {site.out}")
    print(f"site url: {site.site_url}")
    print(f"targets: {len(site.targets)}")
    print(f"posts: {len(site.posts)}")
    for post in site.posts:
        state = "ko+en" if post.split else "single"
        print(f"  {post.slug} [{state}] ko='{post.parts['ko'].title_text}' "
              f"en='{post.parts['en'].title_text}'")
    with_notes = sum(1 for release in site.releases if release.has_notes_file)
    print(f"releases: {len(site.releases)} ({with_notes} with notes files), "
          f"latest {site.latest.tag if site.latest else 'none'}")
    for release in site.releases:
        if not release.parts:
            print(f"  {release.tag}: no notes")

    if not site.targets:
        print("error: no ROM set profiles were read from the catalog", file=sys.stderr)
        return 1
    broken = check_links(site.out)
    if broken:
        print(f"error: {len(broken)} broken internal link(s):", file=sys.stderr)
        for line in broken:
            print(f"  {line}", file=sys.stderr)
        return 1
    print("internal links: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
