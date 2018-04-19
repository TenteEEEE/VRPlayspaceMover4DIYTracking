// PlayspaceMover.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <iostream>
#include <algorithm>
#include <string>
#include <thread>
#include <openvr.h>
#include <vrinputemulator.h>

static vr::IVRSystem* m_VRSystem;
static vrinputemulator::VRInputEmulator inputEmulator;
static vr::HmdVector3d_t lastLeftPos;
static vr::HmdVector3d_t lastRightPos;
static vr::HmdVector3d_t offset;
static int currentFrame;
static vr::TrackedDevicePose_t devicePoses[vr::k_unMaxTrackedDeviceCount];
static vr::HmdMatrix34_t chaperoneMat;
static vr::HmdVector3d_t rightPos;
static vr::HmdVector3d_t leftPos;
static vr::TrackedDevicePose_t* rightPose;
static vr::TrackedDevicePose_t* leftPose;

void updateOffset() {
	//float fSecondsSinceLastVsync;
	//vr::VRSystem()->GetTimeSinceLastVsync(&fSecondsSinceLastVsync, NULL);
	//float fDisplayFrequency = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);
	//float fFrameDuration = 1.f / fDisplayFrequency;
	//float fVsyncToPhotons = vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SecondsFromVsyncToPhotons_Float);
	//float fPredictedSecondsFromNow = fFrameDuration - fSecondsSinceLastVsync + fVsyncToPhotons;
	vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0, devicePoses, vr::k_unMaxTrackedDeviceCount);

	vr::HmdVector3d_t delta;
	delta.v[0] = 0; delta.v[1] = 0; delta.v[2] = 0;
	auto leftId = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
	if (leftId != vr::k_unTrackedDeviceIndexInvalid ) {
		leftPose = devicePoses + leftId;
		if (leftPose->bPoseIsValid && leftPose->bDeviceIsConnected) {
			vr::HmdMatrix34_t* leftMat = &(leftPose->mDeviceToAbsoluteTracking);
			leftPos.v[0] = leftMat->m[0][3];
			leftPos.v[1] = leftMat->m[1][3];
			leftPos.v[2] = leftMat->m[2][3];
			vr::VRControllerState_t leftButtons;
			vr::VRSystem()->GetControllerState(leftId, &leftButtons, sizeof(vr::VRControllerState_t));
			if (leftButtons.ulButtonPressed & (1<<7) ) {
				delta.v[0] = leftPos.v[0] - lastLeftPos.v[0];
				delta.v[1] = leftPos.v[1] - lastLeftPos.v[1];
				delta.v[2] = leftPos.v[2] - lastLeftPos.v[2];
			}
		}
	}
	auto rightId = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
	if (rightId != vr::k_unTrackedDeviceIndexInvalid ) {
		rightPose = devicePoses + rightId;
		if (rightPose->bPoseIsValid && rightPose->bDeviceIsConnected) {
			vr::HmdMatrix34_t* rightMat = &(rightPose->mDeviceToAbsoluteTracking);
			rightPos.v[0] = rightMat->m[0][3];
			rightPos.v[1] = rightMat->m[1][3];
			rightPos.v[2] = rightMat->m[2][3];
			vr::VRControllerState_t rightButtons;
			vr::VRSystem()->GetControllerState(rightId, &rightButtons, sizeof(vr::VRControllerState_t));
			if (rightButtons.ulButtonPressed & (1<<7) ) {
				delta.v[0] = rightPos.v[0] - lastRightPos.v[0];
				delta.v[1] = rightPos.v[1] - lastRightPos.v[1];
				delta.v[2] = rightPos.v[2] - lastRightPos.v[2];
			}
		}
	}

	delta.v[0] = std::clamp(delta.v[0], (double)-0.1f, (double)0.1f);
	delta.v[1] = std::clamp(delta.v[1], (double)-0.1f, (double)0.1f);
	delta.v[2] = std::clamp(delta.v[2], (double)-0.1f, (double)0.1f);

	if (leftId != vr::k_unTrackedDeviceIndexInvalid) {
		if (leftPose->bPoseIsValid && leftPose->bDeviceIsConnected) {
			lastLeftPos.v[0] = leftPos.v[0] - delta.v[0];
			lastLeftPos.v[1] = leftPos.v[1] - delta.v[1];
			lastLeftPos.v[2] = leftPos.v[2] - delta.v[2];
		}
	}

	if (rightId != vr::k_unTrackedDeviceIndexInvalid) {
		if (rightPose->bPoseIsValid && rightPose->bDeviceIsConnected) {
			lastRightPos.v[0] = rightPos.v[0] - delta.v[0];
			lastRightPos.v[1] = rightPos.v[1] - delta.v[1];
			lastRightPos.v[2] = rightPos.v[2] - delta.v[2];
		}
	}

	delta.v[0] = chaperoneMat.m[0][0] * delta.v[0] + chaperoneMat.m[0][1] * delta.v[1] + chaperoneMat.m[0][2] * delta.v[2];
	delta.v[1] = chaperoneMat.m[1][0] * delta.v[0] + chaperoneMat.m[1][1] * delta.v[1] + chaperoneMat.m[1][2] * delta.v[2];
	delta.v[2] = chaperoneMat.m[2][0] * delta.v[0] + chaperoneMat.m[2][1] * delta.v[1] + chaperoneMat.m[2][2] * delta.v[2];

	offset.v[0] -= delta.v[0];
	offset.v[1] -= delta.v[1];
	offset.v[2] -= delta.v[2];
}

void testOffset() {
	offset.v[1] -= 0.01f;
}

void viveMove() {
	for (uint32_t deviceIndex = 0; deviceIndex < vr::k_unMaxTrackedDeviceCount; deviceIndex++) {
		if (!vr::VRSystem()->IsTrackedDeviceConnected(deviceIndex)) {
			continue;
		}
		vrinputemulator::DeviceInfo info;
		inputEmulator.getDeviceInfo(deviceIndex, info);
		if (info.offsetsEnabled == false) {
			inputEmulator.enableDeviceOffsets(deviceIndex, true);
		}
		inputEmulator.setWorldFromDriverTranslationOffset(deviceIndex, offset);
	}
}

int main( int argc, char** argv ) {
	// Initialize stuff
	vr::EVRInitError error = vr::VRInitError_Compositor_Failed;
	std::cout << "Looking for SteamVR...";
	while (error != vr::VRInitError_None) {
		m_VRSystem = vr::VR_Init(&error, vr::VRApplication_Background);
		if (error != vr::VRInitError_None) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
	std::cout << "Success!\n";
	std::cout << "Looking for VR Input Emulator...";
	while (true) {
		try {
			inputEmulator.connect();
			break;
		}
		catch (vrinputemulator::vrinputemulator_connectionerror e) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
	}
	std::cout << "Success!\n";

	std::cout << "Grabbing Chaperone data (You may need to set up your chaperone boundries again if this gets stuck)...";
	vr::VRChaperoneSetup()->RevertWorkingCopy();
	while (vr::VRChaperone()->GetCalibrationState() != vr::ChaperoneCalibrationState_OK) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		vr::VRChaperoneSetup()->RevertWorkingCopy();
	}
	vr::VRChaperoneSetup()->GetWorkingStandingZeroPoseToRawTrackingPose(&chaperoneMat);
	std::cout << "Success!\n";

	lastLeftPos.v[0] = 0; lastLeftPos.v[1] = 0; lastLeftPos.v[2] = 0;
	lastRightPos.v[0] = 0; lastRightPos.v[1] = 0; lastRightPos.v[2] = 0;
	offset.v[0] = 0; offset.v[1] = 0; offset.v[2] = 0;

	// Main loop
	bool running = true;
	while (running) {
		if (vr::VRCompositor() != NULL) {
			vr::Compositor_FrameTiming t;
			bool hasFrame = vr::VRCompositor()->GetFrameTiming(&t, 0);
			if (hasFrame && currentFrame != t.m_nFrameIndex) {
				currentFrame = t.m_nFrameIndex;
				//updateOffset();
				testOffset();
				viveMove();
			}
		}
	}
    return 0;
}