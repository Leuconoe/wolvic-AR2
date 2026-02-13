# Design: METALENSE2 Hand Tracking Refinement

## 1. Laser Pointer Aim
We will change the aim origin to the index finger proximal joint and the direction to point towards the index tip.

**Logic in `aimPose`**:
```cpp
vrb::Vector origin = index_proximal_pos;
vrb::Vector target = index_tip_pos;
vrb::Vector forward = (target - origin).Normalize();
aimPose.position = origin;
aimPose.orientation = lookAt(origin, origin + forward);
```

## 2. Pinch Stability
We will store the "aim direction" when the pinch starts and blend it with the current direction based on the pinch strength. This prevents the "click" action from moving the cursor.

## 3. Position Debugging
If joints are (0,0,0), it's possible they are actually in "head space" but the runtime is weird.
We will log:
- `XR_HAND_JOINT_PALM_EXT`
- `XR_HAND_JOINT_INDEX_TIP_EXT`
- `XR_HAND_JOINT_MIDDLE_TIP_EXT`
If they all return (0,0,0), the tracker is likely not producing data. If they return small values, they might be head-relative.
