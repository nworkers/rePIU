# 20260726-317 Complete 100% Glide Ordinal Switch Refactoring / Design: Complete 100% Glide Ordinal Switch

## 한국어

### 개요

`linexe_glide_boundary.cpp` 내 630~674행의 로깅 체크 및 850~2460행 하단에 독립된 `if` 문으로 남아있던 `_GRDRAWTRIANGLE@12` (52), `_GRDRAWPLANARPOLYGON@12` (53), `_GRDRAWPOLYGON@12` (55), `_GRDRAWPLANARPOLYGONVERTEXLIST@8` (54), `_GRCONSTANTCOLORVALUE@4` (66), `_GRLFBLOCK@24` (47), `_GRLFBUNLOCK@8` (48), `_GRLFBWRITEREGION@32` (71), `_GRLFBREADREGION@28` (72), `_GRLFBCONSTANTALPHA@4` (73), `_GRLFBCONSTANTDEPTH@4` (74), `_GRLFBWRITECOLORSWIZZLE@8` (75) 등의 문자열 비교 구문을 단 1개도 남김없이 전면 삭제하고, **단일 `switch (glide_export->ordinal)` Direct Jump Table로 100% 흡수/전환**합니다.

---

### 대상 항목 및 Ordinal 매핑 테이블

| Glide API Name | Ordinal |
|---|---|
| `_GRGLIDEINIT@0` | 1 |
| `_GRBUFFERCLEAR@12` | 2 |
| `_GRBUFFERSWAP@4` | 3 |
| `_GRBUFFERNUMPENDING@0` | 4 |
| `_GRSSTQUERYHARDWARE@4` | 6 |
| `_GRSSTSELECT@4` | 7 |
| `_GRSSTWINCLOSE@0` | 8 |
| `_GRSSTWINOPEN@28` | 9 |
| `_GRSSTSCREENWIDTH@0` | 10 |
| `_GRSSTSCREENHEIGHT@0` | 11 |
| `_GRTEXMINADDRESS@4` | 12 |
| `_GRTEXMAXADDRESS@4` | 13 |
| `_GUFOGGENERATEEXP@8` | 16 |
| `_GRHINTS@8` | 31 |
| `_GRCOLORMASK@8` | 32 |
| `_GRRENDERBUFFER@4` | 33 |
| `_GRDEPTHMASK@4` | 34 |
| `_GRDEPTHBIASLEVEL@4` | 35 |
| `_GRDEPTHBUFFERMODE@4` | 36 |
| `_GRLFBWRITECOLORFORMAT@4` | 37 |
| `_GRALPHACOMBINE@20` | 38 |
| `_GRCOLORCOMBINE@20` | 39 |
| `_GRALPHABLENDFUNCTION@16` | 40 |
| `_GRALPHATESTFUNCTION@4` | 41 |
| `_GRALPHATESTREFERENCEVALUE@4` | 42 |
| `_GRDEPTHBUFFERFUNCTION@4` | 43 |
| `_GRCLIPWINDOW@16` | 45 |
| `_GRTEXTEXTUREMEMREQUIRED@8` | 46 |
| `_GRGLIDEGETSTATE@4` | 47 |
| `_GRGLIDESETSTATE@4` | 48 |
| `_GRTEXDOWNLOADMIPMAPLEVEL@32` | 49 |
| `_GRDRAWLINE@8` | 50 |
| `_GRDRAWPOINT@4` | 51 |
| `_GRDRAWTRIANGLE@12` | 52 |
| `_GRDRAWPLANARPOLYGON@12` | 53 |
| `_GRDRAWPLANARPOLYGONVERTEXLIST@8` | 54 |
| `_GRDRAWPOLYGON@12` | 55 |
| `_GRCONSTANTCOLORVALUE@4` | 66 |
| `_GRLFBLOCK@24` | 70 |
| `_GRLFBUNLOCK@8` | 71 |
| `_GRLFBWRITEREGION@32` | 72 |
| `_GRLFBREADREGION@28` | 73 |
| `_GRLFBCONSTANTALPHA@4` | 74 |
| `_GRLFBCONSTANTDEPTH@4` | 75 |
| `_GRTEXDOWNLOADTABLE@12` | 76 |
| `_GRLFBWRITECOLORSWIZZLE@8` | 77 |
| `_GRCULLMODE@4` | 99 |
| `_GRDITHERMODE@4` | 100 |
| `_GRFOGMODE@4` | 101 |
| `_GRFOGCOLORVALUE@4` | 102 |
| `_GRFOGTABLE@4` | 103 |
| `_GRTEXCLAMPMODE@12` | 131 |
| `_GRTEXCOMBINE@28` | 132 |
| `_GRTEXFILTERMODE@12` | 134 |
| `_GRTEXMIPMAPMODE@12` | 136 |
| `_GRTEXSOURCE@16` | 138 |

---

## English

### Overview

Eliminates all remaining string comparison logic (`glide_export->name == "_GR..."`) across lines 630-674 and lines 850-2460 in `linexe_glide_boundary.cpp`, incorporating every single Glide API entry (triangle, polygon, LFB, hints, texture min/max, constant color) into the single unified `switch (glide_export->ordinal)` O(1) Jump Table.
