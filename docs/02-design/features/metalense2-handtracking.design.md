# Design: METALENSE2 Hand Tracking Fix

## 1. Ray Orientation Fix
We will modify `OpenXRGestureManagerHandJoints::aimPose` to incorporate the hand's actual orientation. 
Currently, it uses `lookAt(hand_pos, shoulder_pos)`. We want something like `hand_orientation * offset_rotation`.

**Proposed Logic:**
```cpp
auto aimPose = mHandJoints[XR_HAND_JOINT_MIDDLE_PROXIMAL_EXT].pose;
// Instead of lookAt:
// aimPose.orientation = mHandJoints[XR_HAND_JOINT_PALM_EXT].pose.orientation;
// And maybe apply a 45-degree pitch offset to make it point "forward" from the palm.
```

## 2. Hand Position Fix
The issue is likely that `locateInfo.baseSpace = localSpace` in `GetHandTrackingInfo` returns world-fixed coordinates, but the rendering pipeline expects something else or the offsets are not being applied correctly.

**Option A: Use Head-Relative Tracking**
In `OpenXRInputSource::Update`, we can pass the `viewSpace` (if available) or use the current `head` transform to offset the joints.

**Option B: Fix Offset Application**
In `PopulateHandJointLocations`, ensure that if `SPACES` is defined, we handle the `kAverageHeight` and any other device-specific offsets consistently with how the `head` is handled in `DeviceDelegateOpenXR::StartFrame`.

```cpp
#if defined(SPACES)
    // For METALENSE2/LenovoA3, we might need to ensure joints are relative to the same origin as the head.
#endif
```

## 3. Extension Detection
Investigate why `XR_FB_hand_tracking_aim` was reported as supported but not used. If it works, using it would be the best fix for ray orientation.
Check `OpenXRInputSource.cpp` line ~135:
```cpp
mSupportsFBHandTrackingAim = OpenXRExtensions::IsExtensionSupported(XR_FB_HAND_TRACKING_AIM_EXTENSION_NAME);
```
Ensure this extension is correctly loaded and not erased in `OpenXRExtensions::Initialize`.
