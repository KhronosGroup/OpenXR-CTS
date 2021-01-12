Fix: `CopyRGBAImage` was using the wrong array slice when setting image barriers. This broke the "Subimage Tests" case on some hardware/drivers.
