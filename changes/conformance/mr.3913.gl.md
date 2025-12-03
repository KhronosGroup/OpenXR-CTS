- Improvement: XR_EXT_spatial_marker_tracking: If QR code has string data, check that the string
ends with a null terminator.
- Improvement: XR_EXT_spatial_marker_tracking: Replace "REQUIRE" with "CHECK" for marker component
data because we can still continue to render the markers and test the bounded2D component even if
the marker component data is invalid.
- Improvement: XR_EXT_spatial_marker_tracking: Create "update snapshot" for discovered markers so
that we render them at their latest pose.
