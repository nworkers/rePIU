"""Markdown content for the project site (Task 756).

Posts and release notes are written as a full Korean document, a `---` line, then the
full English document. This module splits them, renders them with GitHub-compatible
Markdown, rewrites repository-relative links and reads the ROM set catalog.
"""

from __future__ import annotations

import html
import posixpath
import re
from dataclasses import dataclass, field
from pathlib import Path

from markdown_it import MarkdownIt
from mdit_py_plugins.anchors import anchors_plugin

FENCE_RE = re.compile(r"^ {0,3}(`{3,}|~{3,})")
HEADING_RE = re.compile(r"^(#{1,6})\s+(.*?)\s*#*\s*$")
POST_NAME_RE = re.compile(r"^(\d{4}-\d{2}-\d{2})-(\d{6})-(.+)\.md$")
SCHEME_RE = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.-]*:")

# Marker for links that point inside the site; replaced per page with its root prefix.
SITE_ROOT_MARKER = "@site-root@/"

EXCERPT_MIN_CHARS = 60


@dataclass
class LanguagePart:
    title_text: str
    title_html: str
    body_html: str
    excerpt: str
    summary_html: str
    has_mermaid: bool


@dataclass
class Post:
    slug: str
    date: str
    source: str
    split: bool
    parts: dict[str, LanguagePart] = field(default_factory=dict)


@dataclass
class Target:
    id: str
    parent: str
    title: str

    @property
    def clone_of(self) -> str | None:
        # MAME's BIOS parent "pumpitup" is not a clone relationship worth showing.
        return None if self.parent == "pumpitup" else self.parent


def _fence_states(lines: list[str]) -> list[bool]:
    """Return, per line, whether that line is inside (or opens/closes) a code fence."""
    inside: list[bool] = []
    opener: str | None = None
    for line in lines:
        match = FENCE_RE.match(line)
        if opener is None:
            if match:
                opener = match.group(1)
                inside.append(True)
            else:
                inside.append(False)
        else:
            inside.append(True)
            if match and match.group(1)[0] == opener[0] and len(match.group(1)) >= len(opener) \
                    and line.strip() == match.group(1):
                opener = None
    return inside


def split_languages(text: str) -> tuple[str, str | None]:
    """Split "Korean --- English" at the first rule followed by a same-level heading."""
    lines = text.splitlines()
    inside = _fence_states(lines)

    first_level = None
    for index, line in enumerate(lines):
        if inside[index]:
            continue
        match = HEADING_RE.match(line)
        if match:
            first_level = len(match.group(1))
            break
    if first_level is None:
        return text, None

    for index, line in enumerate(lines):
        if inside[index] or line.strip() != "---":
            continue
        for following in range(index + 1, len(lines)):
            if not lines[following].strip():
                continue
            match = HEADING_RE.match(lines[following])
            if match and len(match.group(1)) == first_level and not inside[following]:
                korean = "\n".join(lines[:index]).rstrip() + "\n"
                english = "\n".join(lines[index + 1:]).lstrip("\n")
                return korean, english
            break
    return text, None


def take_title(text: str) -> tuple[str | None, str]:
    """Remove and return the first heading when it is the first non-blank line."""
    lines = text.splitlines()
    for index, line in enumerate(lines):
        if not line.strip():
            continue
        match = HEADING_RE.match(line)
        if match:
            return match.group(2), "\n".join(lines[index + 1:]).lstrip("\n")
        break
    return None, text


class Renderer:
    """markdown-it configured like GitHub, with site-specific link and block rules."""

    def __init__(self, repo: str, branch: str, repo_root: Path, posts_dir: str):
        self.repo_url = f"https://github.com/{repo}"
        self.branch = branch
        self.repo_root = repo_root
        self.posts_dir = posts_dir.strip("/")
        self.md = (
            MarkdownIt("commonmark", {"html": True})
            .enable(["table", "strikethrough"])
            .use(anchors_plugin, min_level=1, max_level=6)
        )
        self.md.core.ruler.push("site_links", self._rewrite_links)
        default_fence = self.md.renderer.rules.get("fence")

        def fence(renderer, tokens, index, options, env):
            token = tokens[index]
            if token.info.strip().split(" ")[0] == "mermaid":
                env["has_mermaid"] = True
                return f'<pre class="mermaid">{html.escape(token.content)}</pre>\n'
            return default_fence(tokens, index, options, env)

        def table_open(renderer, tokens, index, options, env):
            return '<div class="table-wrap"><table>\n'

        def table_close(renderer, tokens, index, options, env):
            return "</table></div>\n"

        self.md.add_render_rule("fence", fence)
        self.md.add_render_rule("table_open", table_open)
        self.md.add_render_rule("table_close", table_close)

    # -- links ------------------------------------------------------------------------

    def resolve_link(self, href: str, source: str) -> str:
        if not href or href.startswith("#") or href.startswith("//") or SCHEME_RE.match(href):
            return href
        path, _, fragment = href.partition("#")
        path = path.split("?", 1)[0]
        if not path:
            return href
        if path.startswith("/"):
            resolved = posixpath.normpath(path.lstrip("/"))
        else:
            resolved = posixpath.normpath(posixpath.join(posixpath.dirname(source), path))
        suffix = f"#{fragment}" if fragment else ""

        post_match = POST_NAME_RE.match(posixpath.basename(resolved))
        if posixpath.dirname(resolved) == self.posts_dir and post_match:
            slug = f"{post_match.group(1)}-{post_match.group(3)}"
            return f"{SITE_ROOT_MARKER}posts/{slug}.html{suffix}"

        kind = "tree" if (self.repo_root / resolved).is_dir() else "blob"
        return f"{self.repo_url}/{kind}/{self.branch}/{resolved}{suffix}"

    def _rewrite_links(self, state) -> None:
        source = state.env.get("source", "")
        for token in state.tokens:
            for child in token.children or []:
                if child.type == "link_open":
                    child.attrSet("href", self.resolve_link(child.attrGet("href") or "", source))
                elif child.type == "image":
                    src = self.resolve_link(child.attrGet("src") or "", source)
                    if "/blob/" in src:
                        src = src.replace("/blob/", "/raw/", 1)
                    child.attrSet("src", src)

    # -- rendering --------------------------------------------------------------------

    def render(self, text: str, source: str) -> tuple[str, bool]:
        env = {"source": source, "has_mermaid": False}
        return self.md.render(text, env), env["has_mermaid"]

    def render_inline(self, text: str, source: str) -> str:
        return self.md.renderInline(text, {"source": source})

    def plain_text(self, text: str) -> str:
        tokens = self.md.parseInline(text, {})
        parts: list[str] = []
        for token in tokens:
            for child in token.children or []:
                if child.type in ("text", "code_inline"):
                    parts.append(child.content)
                elif child.type in ("softbreak", "hardbreak"):
                    parts.append(" ")
        return re.sub(r"\s+", " ", "".join(parts)).strip()

    def paragraphs(self, text: str) -> list[str]:
        """Source text of the top-level paragraphs, in order."""
        tokens = self.md.parse(text, {})
        found = []
        for index, token in enumerate(tokens):
            if token.type == "paragraph_open" and token.level == 0:
                found.append(tokens[index + 1].content)
        return found

    def language_part(self, text: str, source: str, fallback_title: str) -> LanguagePart:
        title, body = take_title(text)
        title = title or fallback_title
        body_html, has_mermaid = self.render(body, source)
        paragraphs = self.paragraphs(body)
        excerpt = ""
        for paragraph in paragraphs:
            plain = self.plain_text(paragraph)
            if len(plain) >= EXCERPT_MIN_CHARS:
                excerpt = plain
                break
        summary_html = self.render_inline(paragraphs[0], source) if paragraphs else ""
        return LanguagePart(
            title_text=self.plain_text(title),
            title_html=self.render_inline(title, source),
            body_html=body_html,
            excerpt=excerpt,
            summary_html=summary_html,
            has_mermaid=has_mermaid,
        )

    def bilingual(self, text: str, source: str, languages: list[str],
                  fallback_title: str) -> tuple[dict[str, LanguagePart], bool]:
        """Render a Korean-then-English document into one part per language."""
        korean, english = split_languages(text)
        per_language = {"ko": korean, "en": english if english is not None else korean}
        parts = {
            lang: self.language_part(per_language.get(lang, korean), source, fallback_title)
            for lang in languages
        }
        return parts, english is not None


def load_posts(renderer: Renderer, repo_root: Path, posts_dir: str,
               languages: list[str]) -> list[Post]:
    posts: list[Post] = []
    for path in sorted((repo_root / posts_dir).glob("*.md")):
        match = POST_NAME_RE.match(path.name)
        if not match:
            continue  # README.md and anything not named like a post
        date, _, title_slug = match.groups()
        source = f"{posts_dir.strip('/')}/{path.name}"
        text = path.read_text(encoding="utf-8")
        parts, split = renderer.bilingual(text, source, languages, title_slug.replace("-", " "))
        posts.append(Post(slug=f"{date}-{title_slug}", date=date, source=source, split=split,
                          parts=parts))
    posts.sort(key=lambda post: post.source, reverse=True)
    return posts


CATALOG_RE = re.compile(
    r'MakePiuTargetProfile\(\s*"(?P<id>[^"]+)",\s*"(?P<parent>[^"]+)",\s*(?P<title>(?:"[^"]*"\s*)+),',
    re.MULTILINE,
)


def load_targets(catalog_path: Path) -> list[Target]:
    """Read the MAME PIU profiles from the built-in catalog source."""
    text = catalog_path.read_text(encoding="utf-8")
    targets = []
    for match in CATALOG_RE.finditer(text):
        title = "".join(re.findall(r'"([^"]*)"', match.group("title")))
        targets.append(Target(id=match.group("id"), parent=match.group("parent"), title=title))
    return targets
