---
- mr.4139.gl
---
Fix: Ensure OpenGL context is only bound on current thread during `xrDestroySession`, per the OpenXR spec threading requirements.
