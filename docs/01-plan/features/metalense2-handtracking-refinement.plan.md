 # Plan: METALENSE2 Hand Tracking Refinement

## Problems
1.  **Ray Origin**: Currently coming from the palm/middle joint, making it hard to aim. Needs to feel like a "laser pointer" from the index finger.
2.  **Stability**: The ray moves/jitters significantly during pinch gestures, making it hard to click UI elements.
3.  **Position Drift**: Hands are reported at (0,0,0) in logs and don't follow the user consistently.

## Goals
- [ ] Move ray origin to index finger or pinch point.
- [ ] Implement "pinch locking" or stronger filtering to stabilize the ray during interaction.
- [ ] Fix hand positioning so they follow the HMD correctly in AR space.

## Proposed Changes

### 1. Ray Control Improvement
- Change `HAND_JOINT_FOR_AIM` in `OpenXRInputSource.cpp` from `MIDDLE_PROXIMAL` to `INDEX_PROXIMAL` or `INDEX_TIP`.
- In `OpenXRGestureManagerHandJoints::aimPose`, calculate the direction using the vector from `INDEX_PROXIMAL` to `INDEX_TIP`.

### 2. Stability During Pinch
- Introduce a "stable origin" when `pinchFactor` is high.
- Increase filtering (OneEuroFilter parameters) specifically when a pinch is starting.

### 3. Coordinate System Fix
- Investigate why `localSpace` returns (0,0,0). 
- Try to use `head` transform to offset the joints if the runtime is returning head-relative coordinates despite asking for `localSpace`.
- Add logging for all finger tips to see if tracking is partially active.

## Tasks
- [ ] Update `aimPose` logic to use index finger direction.
- [ ] Add smoothing/damping logic for active pinch.
- [ ] Enhance logging to debug the (0,0,0) position issue.
- [ ] Build and verify on device.
