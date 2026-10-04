# 스크린샷 / Screenshots

README와 프로젝트 사이트의 소개에 쓰는 화면입니다. 원본 실행 파일이 rePIU 위에서 그린 화면을 캡처했으며, 원본 ROM·CHD·실행 파일·게임 데이터는 저장소에 포함하지 않습니다.

*Screens used in the README and on the project site's introduction, captured from what the original executables draw on rePIU. The original ROMs, CHDs, executables and game data are not part of the repository.*

| 파일 | 롬셋 | 장면 |
| --- | --- | --- |
| `pumpit1-title.jpg` | `pumpit1` | 타이틀 / title |
| `pumpit2a-title.jpg` | `pumpit2a` | 타이틀 / title |
| `pumpit3a-title.jpg` | `pumpit3a` | 타이틀 / title |
| `pumpito-title.jpg` | `pumpito` | 타이틀 / title |
| `pumpitc-title.jpg` | `pumpitc` | 타이틀 / title |
| `pumpitpc-title.jpg` | `pumpitpc` | 타이틀 / title |
| `pumpitpr-title.jpg` | `pumpitpr` | 타이틀 / title |
| `pumpitpru-demo-play.jpg` | `pumpitpru` | 데모 플레이 / demo play |
| `pumpitea-title.jpg` | `pumpitea` | 타이틀 / title |
| `pumpitpx-title.jpg` | `pumpitpx` | 타이틀 / title |
| `pumpit8-title.jpg` | `pumpit8` | 타이틀 / title |
| `pumpitp2-title.jpg` | `pumpitp2` | 타이틀 / title |
| `pumpipx2-title.jpg` | `pumpipx2` | 타이틀 / title |
| `pumpipx2p-title.jpg` | `pumpipx2p` | 타이틀 / title |
| `pumpitp3-title.jpg` | `pumpitp3` | 타이틀 / title |
| `pumpitp3-demo-play.jpg` | `pumpitp3` | 데모 플레이 / demo play |
| `pumpipx3-title.jpg` | `pumpipx3` | 타이틀 / title |
| `shaders/` | `pumpit8` | 화면 후처리 shader 비교 / post-processing shader comparison |

## 캡처 방법 / How they were taken

- v0.0.200(Task 770)의 Win32 x86 Release 빌드로 `repiu <롬셋>`을 기본 창(2배, 1280x960)으로 실행했습니다.
- 입력 없이 어트랙트 화면을 90초 동안 3초마다 창의 클라이언트 영역에서 캡처하고(DPI 인식 프로세스에서 `PrintWindow`의 `PW_CLIENTONLY | PW_RENDERFULLCONTENT`), 대조표에서 한 장을 골랐습니다. `pumpitpru`는 타이틀이 `pumpitpr`과 같아 데모 플레이를 골랐습니다.
- 640x480으로 줄여 JPEG(품질 90)로 저장했습니다.
- `shaders/`는 `pumpit8`을 입력 스크립트(`scripts/input_scripts/pumpit8_play.txt`)로 실행해 `none`·`crt`·`scanline`에서 같은 장면을 2초마다 캡처한 것입니다. **주사선이 출력 픽셀 단위라 줄이지 않고 1280x960 그대로** JPEG(품질 92)로 두었습니다.
- 사이트 빌드(`scripts/site/build_site.py`)는 이 디렉터리를 산출물의 `screenshots/`로 복사합니다. 사이트에 보일 목록과 순서는 `docs/sites/site.toml`의 `[[screenshots]]`, 설명은 `docs/sites/i18n/*.toml`의 `[screenshots.captions]`에 있습니다.

*Taken with the Win32 x86 Release build of v0.0.200 (Task 770), running `repiu <rom set>` in the default 2x window (1280x960). The window's client area was captured every 3 s for 90 s of attract screens with no input (`PrintWindow` with `PW_CLIENTONLY | PW_RENDERFULLCONTENT` in a DPI-aware process) and one frame chosen from contact sheets; `pumpitpru` shares `pumpitpr`'s title, so a demo-play frame was chosen for it. Frames were scaled to 640x480 and saved as JPEG (quality 90). `shaders/` holds `pumpit8` run with an input script (`scripts/input_scripts/pumpit8_play.txt`) under `none`, `crt` and `scanline`, the same scenes captured every 2 s and **kept at 1280x960, because the scanlines are drawn per output pixel**, as JPEG (quality 92). The site build (`scripts/site/build_site.py`) copies this directory to `screenshots/` in its output; the list and order shown on the site are in `[[screenshots]]` of `docs/sites/site.toml`, and the captions in `[screenshots.captions]` of `docs/sites/i18n/*.toml`.*
