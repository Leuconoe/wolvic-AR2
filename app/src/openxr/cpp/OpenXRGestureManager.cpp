//
// Created by sergio on 05/07/23.
//

#include <cstring>
#include "OpenXRGestureManager.h"

namespace crow {

// Distance threshold to consider that two hand joints touch
// Used to detect pinch events between thumb-tip joint and the
// rest of the finger tips.
const float kPinchThreshold = 0.019;

// These two are used to measure a pinch factor between [0,1]
// between the thumb and the index fingertips, where 0 is no
// pinch at all and 1.0 means fingers are touching. These
// value is used to give a visual cue to the user (e.g, size
// of pointer target).
const float kPinchStart = 0.055;
const float kPinchRange = kPinchStart - kPinchThreshold;

// We apply a exponential smoothing filter to the measured distance between index and thumb so we
// avoid erroneous click and release events. This constant is the smoothing factor of said filter.
const double kSmoothFactor = 0.5;

bool
OpenXRGestureManager::handFacesHead(const vrb::Matrix &hand, const vrb::Matrix &head) {
    // For the hand we take the Y axis because that corresponds to head's Z axis when
    // the hand is in upright position facing head (the gesture we want to detect).
    auto handDirection = hand.MultiplyDirection({0, 1, 0}).Normalize();
#ifdef PICOXR
    // Axis are inverted in Pico system versions prior to 5.7.1
    if (CompareBuildIdString(kPicoVersionHandTrackingUpdate))
        handDirection = hand.MultiplyDirection({0, 0, -1});
#endif
    auto headDirection = head.MultiplyDirection({0, 0, -1}).Normalize();

    // First check that vector directions align
    const float kHandHeadDirectionAlignment = 0.8;
    if (handDirection.Dot(headDirection) <= kHandHeadDirectionAlignment)
        return false;

    // Then check that vectors are not too far away
    const float kHandHeadDistanceThreshold = 0.10;
    auto handToHead = hand.GetTranslation() - head.GetTranslation();
    auto handProjectedIntoHeadPlane = headDirection * (handToHead.Dot(headDirection));
    auto inPlaneDistance = handToHead - handProjectedIntoHeadPlane;

    return abs(inPlaneDistance.x()) < kHandHeadDistanceThreshold &&
        abs(inPlaneDistance.y()) < kHandHeadDistanceThreshold;
}

double
GetDistanceBetweenJoints (const HandJointsArray& handJoints, XrHandJointEXT jointA, XrHandJointEXT jointB) {
    XrVector3f jointAPosXr = handJoints[jointA].pose.position;
    vrb::Vector jointAPos = vrb::Vector(jointAPosXr.x, jointAPosXr.y, jointAPosXr.z);

    XrVector3f jointBPosXr = handJoints[jointB].pose.position;
    vrb::Vector jointBPos = vrb::Vector(jointBPosXr.x, jointBPosXr.y, jointBPosXr.z);

    return vrb::Vector(jointAPos - jointBPos).Magnitude();
}

void
OpenXRGestureManager::getTriggerPinchStatusAndFactor(const HandJointsArray& handJoints,
                                                     bool& isPinching, double& pinchFactor) {
    isPinching = false;
    pinchFactor = 1.0;

    if (!IsHandJointPositionValid(XR_HAND_JOINT_THUMB_TIP_EXT, handJoints) ||
        !IsHandJointPositionValid(XR_HAND_JOINT_INDEX_TIP_EXT, handJoints)) {
        return;
    }

    const double indexThumbDistance = GetDistanceBetweenJoints(handJoints, XR_HAND_JOINT_THUMB_TIP_EXT,
                                                               XR_HAND_JOINT_INDEX_TIP_EXT);

    // Apply a smoothing filter to reduce the number of phantom events.
    mSmoothIndexThumbDistance =
        kSmoothFactor * indexThumbDistance + (1 - kSmoothFactor) * mSmoothIndexThumbDistance;

    pinchFactor = 1.0 -
                  std::clamp((mSmoothIndexThumbDistance - kPinchThreshold) / kPinchRange, 0.0,
                             1.0);
    isPinching = mSmoothIndexThumbDistance < kPinchThreshold;
}

void
OpenXRGestureManagerFBHandTrackingAim::populateNextStructureIfNeeded(XrHandJointLocationsEXT& handJointLocations) {
    memset(&mFBAimState, 0, sizeof(XrHandTrackingAimStateFB));
    mFBAimState = { XR_TYPE_HAND_TRACKING_AIM_STATE_FB, handJointLocations.next, 0  };
    handJointLocations.next = &mFBAimState;
}

bool
OpenXRGestureManagerFBHandTrackingAim::hasAim() const {
    return mFBAimState.status & XR_HAND_TRACKING_AIM_VALID_BIT_FB;
}

XrPosef
OpenXRGestureManagerFBHandTrackingAim::aimPose(const XrTime predictedDisplayTime, const OpenXRHandFlags handeness, const vrb::Matrix& head) const {
    ASSERT(hasAim());
    return mFBAimState.aimPose;
}

bool
OpenXRGestureManagerFBHandTrackingAim::systemGestureDetected(const vrb::Matrix& hand, const vrb::Matrix& head) const {
#if LYNX
    // Lynx does not correctly implement the SYSTEM_GESTURE_BIT_FB and XR_HAND_TRACKING_AIM_VALID_BIT_FB flags (it's always active)
    return handFacesHead(hand, head);
#else
    return mFBAimState.status & XR_HAND_TRACKING_AIM_SYSTEM_GESTURE_BIT_FB;
#endif
}

void
OpenXRGestureManagerFBHandTrackingAim::getTriggerPinchStatusAndFactor(const HandJointsArray& handJoints,
                                                                         bool& isPinching, double& pinchFactor) {
    isPinching = mFBAimState.status & XR_HAND_TRACKING_AIM_INDEX_PINCHING_BIT_FB;
    pinchFactor = mFBAimState.pinchStrengthIndex;
}

OpenXRGestureManagerHandJoints::OpenXRGestureManagerHandJoints(HandJointsArray& handJoints, OneEuroFilterParams* filterParams)
    : mHandJoints(handJoints) {
    if (filterParams)
        mOneEuroFilterPosition = std::make_unique<OneEuroFilterVector>(filterParams->mincutoff, filterParams->beta, filterParams->dcutoff);
}

bool
OpenXRGestureManagerHandJoints::hasAim() const {
    return IsHandJointPositionValid(XR_HAND_JOINT_MIDDLE_PROXIMAL_EXT, mHandJoints);
}

XrPosef
OpenXRGestureManagerHandJoints::aimPose(const XrTime predictedDisplayTime, const OpenXRHandFlags handeness, const vrb::Matrix& head) const {
    ASSERT(hasAim());

    // Ray origin is Index Proximal
    auto aimPose = mHandJoints[XR_HAND_JOINT_INDEX_PROXIMAL_EXT].pose;
    vrb::Vector proximalPos(aimPose.position.x, aimPose.position.y, aimPose.position.z);
    
    // Filtering for position
    float* filteredPos = mOneEuroFilterPosition ? mOneEuroFilterPosition->filter(predictedDisplayTime, proximalPos.Data()) : proximalPos.Data();
    aimPose.position = { filteredPos[0], filteredPos[1], filteredPos[2] };

    auto lookAt = [](const vrb::Vector& sourcePoint, const vrb::Vector& destPoint) -> vrb::Quaternion {
        const float EPSILON = 0.000001f;
        vrb::Vector worldForward = { 0, 0, 1 };
        vrb::Vector forwardVector = (destPoint - sourcePoint).Normalize();
        float dot = worldForward.Dot(forwardVector);
        if (abs(dot - (-1.0f)) < EPSILON) return {0, 1, 0, M_PI };
        if (abs(dot - (1.0f)) < EPSILON) return {0, 0, 0, 1 };
        auto quaternionFromAxisAndAngle = [](const vrb::Vector& axis, float angle) -> vrb::Quaternion {
            float halfAngle = angle * .5f;
            float s = sin(halfAngle);
            return { axis.x() * s, axis.y() * s, axis.z() * s, cos(halfAngle) };
        };
        float rotationAngle = acos(dot);
        auto rotationAxis = worldForward.Cross(forwardVector);
        return quaternionFromAxisAndAngle(rotationAxis.Normalize(), rotationAngle);
    };

    // Ray Direction: from Index Proximal towards Index Tip
    vrb::Vector forwardDir;
    vrb::Vector originPos = proximalPos;
    if (IsHandJointPositionValid(XR_HAND_JOINT_INDEX_TIP_EXT, mHandJoints)) {
        auto tPos = mHandJoints[XR_HAND_JOINT_INDEX_TIP_EXT].pose.position;
        vrb::Vector tipPos(tPos.x, tPos.y, tPos.z);
        forwardDir = (tipPos - proximalPos).Normalize();
        originPos = tipPos; // Move origin to tip for better "laser pointer" feel
    } else {
        auto shoulder = head.MultiplyDirection({handeness == Right ? 0.15f : -0.15f, -0.25f, 0.0f});
        forwardDir = (proximalPos - shoulder).Normalize();
    }

    // Pinch Lock Stability
    bool isPinching = false;
    double pinchFactor = 0.0;
    const_cast<OpenXRGestureManagerHandJoints*>(this)->getTriggerPinchStatusAndFactor(mHandJoints, isPinching, pinchFactor);
    
    static vrb::Vector lastForward[2] = { {0,0,1}, {0,0,1} };
    int hIdx = (handeness == Right ? 1 : 0);
    
    float blend = std::clamp((float)pinchFactor * 1.2f, 0.0f, 0.98f);
    forwardDir = (forwardDir * (1.0f - blend) + lastForward[hIdx] * blend).Normalize();
    lastForward[hIdx] = forwardDir;

    // IMPORTANT: Invert direction for lookAt because Wolvic's ray is on -Z
    // We point +Z towards the OPPOSITE of forwardDir (pointing at the proximal joint from the tip)
    auto q = lookAt(originPos, originPos - forwardDir);
    aimPose.orientation = {q.x(), q.y(), q.z(), q.w() };
    aimPose.position = { originPos.x(), originPos.y(), originPos.z() };
    
    return aimPose;
}

bool
OpenXRGestureManagerHandJoints::systemGestureDetected(const vrb::Matrix& hand, const vrb::Matrix& head) const {
    return handFacesHead(hand, head);
}

}