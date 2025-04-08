---
- issue.2384.gl
---
New test: Validate that the `XrFovf` values returned from `xrLocateViews` are valid, plus warn if left == right or up == down are equal (0 FOV in either direction) as that is permitted but likely to lead to undefined behavior when applications compute their projection matrix.
