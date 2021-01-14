---
- issue.1490.gl
---
Fix: The test assumed that X and Y components of a vector2 action would have exactly the same timestamp. Changed that to check that the vector2 action would have the most recent of those two timestamps instead.
