/*
<samplecode>
<abstract>Filter Effect AU</abstract>
</samplecode>
*/

#include <AudioUnitSDK/AUEffectBase.h>

#ifndef __Filter_h__
#define __Filter_h__

class Filter : public ausdk::AUEffectBase {
public:
	Filter(AudioUnit component);

	OSStatus Initialize() override;

	std::unique_ptr<ausdk::AUKernelBase> NewKernel() override;

	// for custom property
	OSStatus GetPropertyInfo(AudioUnitPropertyID inID, AudioUnitScope inScope,
		AudioUnitElement inElement, UInt32& outDataSize, bool& outWritable) override;

	OSStatus GetProperty(AudioUnitPropertyID inID, AudioUnitScope inScope,
		AudioUnitElement inElement, void* outData) override;

	OSStatus GetParameterInfo(AudioUnitScope inScope, AudioUnitParameterID inParameterID,
		AudioUnitParameterInfo& outParameterInfo) override;

	// handle presets:
	OSStatus GetPresets(CFArrayRef* outData) const override;
	OSStatus NewFactoryPresetSet(const AUPreset& inNewFactoryPreset) override;

	// we'll report a 1ms tail.   A reverb effect would have a much more substantial tail on
	// the order of several seconds....
	//
	bool SupportsTail() override { return true; }
	Float64 GetTailTime() override { return 0.001; }

	// we have no latency
	//
	// A lookahead compressor or FFT-based processor should report the true latency in seconds
	Float64 GetLatency() override { return 0.0; }
};

#endif
