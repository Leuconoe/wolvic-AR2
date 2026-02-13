# Plan: METALENSE2 Hand Tracking Fix

## Problem Description
1. **Ray Rotation Issue**: The hand tracking ray behaves like a 3DoF joystick. It originates from the palm but doesn't rotate with the hand. This is likely because `OpenXRGestureManagerHandJoints` calculates aim by looking at a "shoulder" point from the hand's position, ignoring the hand's actual orientation.
2. **Hand Position Mismatch**: When the user moves (HMD moves), the hands remain at the initial "virtual" position instead of following the user. This suggests a coordinate space mismatch or lack of proper offset application when locating hand joints in `SPACES` AR mode.

## Goals
- [ ] Fix hand ray orientation to follow hand rotation.
- [ ] Ensure hand joints follow the HMD as the user moves in the real/virtual world.

## Proposed Changes

### 1. Fix Ray Orientation
- Modify `OpenXRGestureManagerHandJoints::aimPose` in `OpenXRGestureManager.cpp`.
- Instead of calculating a "look-at shoulder" rotation, combine the hand's palm orientation with a slight offset or use the index finger direction.
- Alternatively, check if `XR_FB_hand_tracking_aim` can be enabled/fixed for METALENSE2.

### 2. Fix Hand Positioning
- In `OpenXRInputSource::GetHandTrackingInfo`, investigate using a different base space or applying the same world-to-local offsets used for the head.
- Check if `SPACES` requires specific handling for the `LOCAL` reference space when tracking hands.
- In `DeviceDelegateOpenXR.cpp`, verify if `localSpace` is correctly updated or if we should use `viewSpace` for hand tracking if it's meant to be head-relative.

## Tasks
- [ ] Research: Verify if `XR_FB_hand_tracking_aim` is truly unavailable on METALENSE2.
- [ ] Implementation: Update `OpenXRGestureManagerHandJoints::aimPose` to include hand rotation.
- [ ] Implementation: Adjust `OpenXRInputSource::GetHandTrackingInfo` or `PopulateHandJointLocations` to fix hand drift.
- [ ] Verification: Build and run on device, check logs and behavior.

## Success Criteria
- Hand ray rotates naturally with the hand (6DoF behavior).
- Hands stay attached to the user even after moving several meters in the physical space.
