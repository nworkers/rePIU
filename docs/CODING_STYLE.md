# 코딩 스타일

## 기본 원칙

* C++ 코드는 C++20을 기준으로 작성한다.
* 기본 기준은 Google C++ Style Guide를 따른다.
* 프로젝트 예외 규칙은 아래 항목으로 명시한다.
* 원본 실행 파일의 동작 보존을 우선하며, 편의를 위한 게임 로직 재작성은 피한다.
* 플랫폼 공용 코어와 플랫폼 종속 구현을 분리한다.
* 큰 구조 변경은 설계 문서에 근거를 남긴다.
* 새 기능에는 최소 검증 절차를 함께 문서화한다.

## Basic Principles

* Write C++ code against C++20.
* Follow the Google C++ Style Guide as the baseline.
* Project-specific exceptions are defined below.
* Prioritize preserving original executable behavior and avoid rewriting game logic for convenience.
* Separate the platform-neutral core from platform-specific implementations.
* Document the rationale for large structural changes in design documents.
* Document a minimum verification procedure with each new feature.

## C++ 기본 기준

* 기본 기준은 Google C++ Style Guide를 따른다.
* 프로젝트 예외 규칙은 아래 항목으로 명시한다.

## C++ Baseline

* Follow the Google C++ Style Guide as the baseline.
* Project-specific exceptions are defined below.

## 프로젝트 예외 규칙

* 여는 중괄호 `{` 는 같은 줄 끝에 두지 않고 다음 줄에 둔다.
* 탭 문자는 사용하지 않는다.
* 들여쓰기는 공백 4칸을 사용한다.
* 함수명은 Google 스타일에 맞춰 `PascalCase`를 사용한다.
* 변수명은 `snake_case`를 사용한다.
* 클래스 멤버 변수는 `snake_case_`를 사용한다.
* C++ 소스 파일 확장자는 `.cpp`를 사용한다.
* C++ 헤더 파일 확장자는 `.h`를 사용한다.

## Project-Specific Exceptions

* Place opening braces `{` on the next line instead of at the end of the current line.
* Do not use tab characters.
* Use four spaces for indentation.
* Use `PascalCase` for function names, following Google style.
* Use `snake_case` for variable names.
* Use `snake_case_` for class member variables.
* Use `.cpp` for C++ source files.
* Use `.h` for C++ header files.

## 적용 범위

* `include/` 아래의 C++ 헤더
* `src/` 아래의 C++ 소스
* 이후 추가되는 모든 C++ 파일

## Scope

* C++ headers under `include/`
* C++ source files under `src/`
* All future C++ files

## 예시

잘못된 예:

```cpp
int main() {
    if (ready) {
        Run();
    }
}
```

올바른 예:

```cpp
int main()
{
    if (ready)
    {
        Run();
    }
}
```

## Example

Incorrect:

```cpp
int main() {
    if (ready) {
        Run();
    }
}
```

Correct:

```cpp
int main()
{
    if (ready)
    {
        Run();
    }
}
```

## 디렉터리 정책

* 플랫폼 공용 로더와 런타임 코어는 `src/` 아래의 공용 영역에 둔다.
* Win32 전용 코드는 `src/platform/win32/` 아래에 둔다.
* Linux 전용 코드는 `src/platform/linux/` 아래에 둔다.
* Web 전용 코드는 `src/platform/web/` 아래에 둔다.

## Directory Policy

* Put platform-neutral loader and runtime core code in shared areas under `src/`.
* Put Win32-specific code under `src/platform/win32/`.
* Put Linux-specific code under `src/platform/linux/`.
* Put Web-specific code under `src/platform/web/`.

## 라이선스 정책

* 프로젝트 기본 라이선스는 `BSD 3-Clause License`를 기준으로 한다.
* GPL, LGPL, AGPL 등 전염성 라이선스의 서드파티 코드는 도입하지 않는다.
* 서드파티 의존성을 추가하기 전에 라이선스를 확인하고 문서화한다.

## License Policy

* Use the `BSD 3-Clause License` as the project license baseline.
* Do not introduce third-party code under copyleft licenses such as GPL, LGPL, or AGPL.
* Check and document third-party dependency licenses before adding them.
