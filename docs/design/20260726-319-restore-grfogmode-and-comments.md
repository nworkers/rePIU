# 20260726-319 _GRFOGMODE@4 백엔드 연동 및 삭제된 주석 완전 복구 설계 / Design: Restore _GRFOGMODE Backend Integration & Comments

## 한국어

### 개요

이전 마이그레이션 과정에서 누락된 `_GRFOGMODE@4` (`go::kGrFogMode`)의 백엔드 연동 로직 (`context->glide_backend.SetFogMode(mode)` 및 `decline_gate("fog-mode-backend-failure")`)을 완전하게 복구하고, `_GRHINTS@8` 등 마이그레이션 도중 소실된 주요 의도 설명 주석을 원상태로 복원합니다.

---

### 복구 세부 사항

1. **`_GRFOGMODE@4` 백엔드 호출 복구:**
   ```cpp
   case go::kGrFogMode: // _GRFOGMODE@4
   {
       const std::uint32_t mode = context->glide_gate_stack[1];
       if (!context->glide_backend.SetFogMode(mode))
       {
           context->glide_backend_message =
               context->glide_backend.message();
           return decline_gate("fog-mode-backend-failure");
       }
       context->glide_state.fog_mode = mode;
       context->glide_backend_message = context->glide_backend.message();
       ++context->glide_gate_handled_count;
       win32_context->Eip = return_address;
       win32_context->Esp += 2U * sizeof(std::uint32_t);
       return true;
   }
   ```

2. **`_GRHINTS@8` 의도 설명 주석 복구:**
   ```cpp
   case go::kGrHints: // _GRHINTS@8
       // Glide documents this call as optimization advice. Preserve the
       // observed stdcall ABI while the renderer has no verified hint policy.
       RecordGlideImplementationIssue(
           win32_context,
           context,
           *glide_export,
           repiu::hle::GlideImplementationIssueKind::
               kUnimplementedFunction,
           "optimization-hint-noop",
           "hint accepted without implementation",
           "continue");
       ++context->glide_gate_handled_count;
       win32_context->Eip = return_address;
       win32_context->Esp += 3U * sizeof(std::uint32_t);
       return true;
   ```

---

## English

### Overview

Restores missing `_GRFOGMODE@4` (`go::kGrFogMode`) backend integration logic (`context->glide_backend.SetFogMode(mode)` and error handling) lost during earlier refactoring, and reinstates technical rationale comments (such as `_GRHINTS@8` optimization advice note).
