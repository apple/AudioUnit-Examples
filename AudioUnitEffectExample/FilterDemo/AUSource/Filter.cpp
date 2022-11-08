/*
<samplecode>
<import>Filter.h</import>
</samplecode>
*/

#include "Filter.h"

#include <AudioToolbox/AudioUnitUtilities.h>
#include <CoreServices/CoreServices.h>

#include <math.h>

#define EnableCocoaUI !TARGET_OS_IPHONE

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ____FilterKernel

class FilterKernel : public ausdk::AUKernelBase // the actual filter DSP happens here
{
public:
	FilterKernel(ausdk::AUEffectBase& inAudioUnit);

	// processes one channel of non-interleaved samples
	void Process(const Float32* inSourceP, Float32* inDestP, UInt32 inFramesToProcess,
		bool& ioSilence) override;

	// resets the filter state
	void Reset() override;

	void CalculateLopassParams(double inFreq, double inResonance);

	// used by the custom property handled in the Filter class below
	double GetFrequencyResponse(double inFreq);


private:
	// filter coefficients
	double mA0;
	double mA1;
	double mA2;
	double mB1;
	double mB2;

	// filter state
	double mX1;
	double mX2;
	double mY1;
	double mY2;

	double mLastCutoff;
	double mLastResonance;
};


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ____Filter

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Standard DSP AudioUnit implementation

AUSDK_COMPONENT_ENTRY(ausdk::AUBaseProcessFactory, Filter)


enum { kFilterParam_CutoffFrequency = 0, kFilterParam_Resonance = 1 };


static CFStringRef kCutoffFreq_Name = CFSTR("cutoff frequency");
static CFStringRef kResonance_Name = CFSTR("resonance");


const float kMinCutoffHz = 12.0;
const float kDefaultCutoff = 1000.0;
const float kMinResonance = -20.0;
const float kMaxResonance = 20.0;
const float kDefaultResonance = 0;


// Factory presets
static const int kPreset_One = 0;
static const int kPreset_Two = 1;
static const int kNumberPresets = 2;

static AUPreset kPresets[kNumberPresets] = { { kPreset_One, CFSTR("Preset One") },
	{ kPreset_Two, CFSTR("Preset Two") } };

// static const int kPresetDefault = kPreset_One;
// static const int kPresetDefaultIndex = 0;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ____Construction_Initialization


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Filter::Filter
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Filter::Filter(AudioUnit component) : AUEffectBase(component)
{
	// all the parameters must be set to their initial values here
	//
	// these calls have the effect both of defining the parameters for the first time
	// and assigning their initial values
	//
	SetParameter(kFilterParam_CutoffFrequency, kDefaultCutoff);
	SetParameter(kFilterParam_Resonance, kDefaultResonance);

	// kFilterParam_CutoffFrequency max value depends on sample-rate
	SetParamHasSampleRateDependency(true);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Filter::Initialize
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus Filter::Initialize()
{
	OSStatus result = AUEffectBase::Initialize();

	if (result == noErr) {
		// in case the AU was un-initialized and parameters were changed, the view can now
		// be made aware it needs to update the frequency response curve
		PropertyChanged(kAudioUnitProperty_FrequencyResponse, kAudioUnitScope_Global, 0);
	}

	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Filter::NewKernel
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
std::unique_ptr<ausdk::AUKernelBase> Filter::NewKernel()
{
	return std::make_unique<FilterKernel>(*this);
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ____Parameters

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Filter::GetParameterInfo
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus Filter::GetParameterInfo(AudioUnitScope inScope, AudioUnitParameterID inParameterID,
	AudioUnitParameterInfo& outParameterInfo)
{
	OSStatus result = noErr;

	outParameterInfo.flags =
		kAudioUnitParameterFlag_IsWritable + kAudioUnitParameterFlag_IsReadable;

	if (inScope == kAudioUnitScope_Global) {

		switch (inParameterID) {
		case kFilterParam_CutoffFrequency:
			AUBase::FillInParameterName(outParameterInfo, kCutoffFreq_Name, false);
			outParameterInfo.unit = kAudioUnitParameterUnit_Hertz;
			outParameterInfo.minValue = kMinCutoffHz;
			outParameterInfo.maxValue = GetSampleRate() * 0.5;
			outParameterInfo.defaultValue = kDefaultCutoff;
			outParameterInfo.flags += kAudioUnitParameterFlag_IsHighResolution;
			outParameterInfo.flags += kAudioUnitParameterFlag_DisplayLogarithmic;
			break;

		case kFilterParam_Resonance:
			AUBase::FillInParameterName(outParameterInfo, kResonance_Name, false);
			outParameterInfo.unit = kAudioUnitParameterUnit_Decibels;
			outParameterInfo.minValue = kMinResonance;
			outParameterInfo.maxValue = kMaxResonance;
			outParameterInfo.defaultValue = kDefaultResonance;
			outParameterInfo.flags += kAudioUnitParameterFlag_IsHighResolution;
			break;

		default:
			result = kAudioUnitErr_InvalidParameter;
			break;
		}
	} else {
		result = kAudioUnitErr_InvalidParameter;
	}

	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ____Properties

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Filter::GetPropertyInfo
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~=
OSStatus Filter::GetPropertyInfo(AudioUnitPropertyID inID, AudioUnitScope inScope,
	AudioUnitElement inElement, UInt32& outDataSize, bool& outWritable)
{
	if (inScope == kAudioUnitScope_Global) {
		switch (inID) {
#if EnableCocoaUI
		case kAudioUnitProperty_CocoaUI:
			outWritable = false;
			outDataSize = sizeof(AudioUnitCocoaViewInfo);
			return noErr;
#endif
		case kAudioUnitProperty_FrequencyResponse: // our custom property
			if (inScope != kAudioUnitScope_Global)
				return kAudioUnitErr_InvalidScope;
			outDataSize = kNumberOfResponseFrequencies * sizeof(AudioUnitFrequencyResponseBin);
			outWritable = false;
			return noErr;
		}
	}

	return AUEffectBase::GetPropertyInfo(inID, inScope, inElement, outDataSize, outWritable);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Filter::GetProperty
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus Filter::GetProperty(
	AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, void* outData)
{
	if (inScope == kAudioUnitScope_Global) {
		switch (inID) {
		// This property allows the host application to find the UI associated with this
		// AudioUnit
		//
#if EnableCocoaUI
		case kAudioUnitProperty_CocoaUI: {
			// Look for a resource in the main bundle by name and type.
			CFBundleRef bundle =
				CFBundleGetBundleWithIdentifier(CFSTR("com.example.apple-samplecode.FilterDemo"));

			if (bundle == NULL)
				bundle = CFBundleGetMainBundle();

			if (bundle == NULL)
				return fnfErr;

			CFURLRef bundleURL = CFBundleCopyResourceURL(bundle,
				CFSTR("CocoaFilterView"), // this is the name of the cocoa bundle as specified in
										  // the CocoaViewFactory.plist
				CFSTR("bundle"),          // this is the extension of the cocoa bundle
				NULL);
			
			if (bundleURL == NULL)
				return fnfErr;

			CFStringRef className =
				CFSTR("AppleDemoFilter_ViewFactory"); // name of the main class that implements the
													  // AUCocoaUIBase protocol
			AudioUnitCocoaViewInfo cocoaInfo = { bundleURL, { className } };
			*((AudioUnitCocoaViewInfo*)outData) = cocoaInfo;

			return noErr;
		}
#endif
		// This is our custom property which reports the current frequency response curve
		//
		case kAudioUnitProperty_FrequencyResponse: {
			if (inScope != kAudioUnitScope_Global)
				return kAudioUnitErr_InvalidScope;

			// the kernels are only created if we are initialized
			// since we're using the kernels to get the curve info, let
			// the caller know we can't do it if we're un-initialized
			// the UI should check for the error and not draw the curve in this case
			if (!IsInitialized())
				return kAudioUnitErr_Uninitialized;

			AudioUnitFrequencyResponseBin* freqResponseTable =
				((AudioUnitFrequencyResponseBin*)outData);

			// each of our filter kernel objects (one per channel) will have an identical frequency
			// response so we arbitrarilly use the first one...
			//
			FilterKernel* filterKernel = dynamic_cast<FilterKernel*>(GetKernel(0));


			double cutoff = GetParameter(kFilterParam_CutoffFrequency);
			double resonance = GetParameter(kFilterParam_Resonance);

			float srate = GetSampleRate();

			cutoff = 2.0 * cutoff / srate;
			if (cutoff > 0.99)
				cutoff = 0.99; // clip cutoff to highest allowed by sample rate...

			filterKernel->CalculateLopassParams(cutoff, resonance);

			for (int i = 0; i < kNumberOfResponseFrequencies; i++) {
				double frequency = freqResponseTable[i].mFrequency;

				freqResponseTable[i].mMagnitude = filterKernel->GetFrequencyResponse(frequency);
			}

			return noErr;
		}
		}
	}

	// if we've gotten this far, handles the standard properties
	return AUEffectBase::GetProperty(inID, inScope, inElement, outData);
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ____Presets

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Filter::GetPresets
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus Filter::GetPresets(CFArrayRef* outData) const
{
	// this is used to determine if presets are supported
	// which in this unit they are so we implement this method!
	if (outData == NULL)
		return noErr;

	CFMutableArrayRef theArray = CFArrayCreateMutable(NULL, kNumberPresets, NULL);
	for (int i = 0; i < kNumberPresets; ++i) {
		CFArrayAppendValue(theArray, &kPresets[i]);
	}

	*outData = (CFArrayRef)theArray; // client is responsible for releasing the array
	return noErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Filter::NewFactoryPresetSet
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus Filter::NewFactoryPresetSet(const AUPreset& inNewFactoryPreset)
{
	SInt32 chosenPreset = inNewFactoryPreset.presetNumber;

	for (int i = 0; i < kNumberPresets; ++i) {
		if (chosenPreset == kPresets[i].presetNumber) {
			// set whatever state you need to based on this preset's selection
			//
			// Here we use a switch statement, but it would also be possible to
			// use chosenPreset as an index into an array (if you publish the preset
			// numbers as indices in the GetPresets() method)
			//
			switch (chosenPreset) {
			case kPreset_One:
				SetParameter(kFilterParam_CutoffFrequency, 200.0);
				SetParameter(kFilterParam_Resonance, -5.0);
				break;
			case kPreset_Two:
				SetParameter(kFilterParam_CutoffFrequency, 1000.0);
				SetParameter(kFilterParam_Resonance, 10.0);
				break;
			}

			SetAFactoryPresetAsCurrent(kPresets[i]);
			return noErr;
		}
	}

	return kAudioUnitErr_InvalidPropertyValue;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ____FilterKernel


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	FilterKernel::FilterKernel()
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
FilterKernel::FilterKernel(ausdk::AUEffectBase& inAudioUnit) : AUKernelBase(inAudioUnit)
{
	Reset();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	FilterKernel::Reset()
//
//		It's very important to fully reset all filter state variables to their
//		initial settings here.  For delay/reverb effects, the delay buffers must
//		also be cleared here.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void FilterKernel::Reset()
{
	mX1 = 0.0;
	mX2 = 0.0;
	mY1 = 0.0;
	mY2 = 0.0;

	// forces filter coefficient calculation
	mLastCutoff = -1.0;
	mLastResonance = -1.0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	FilterKernel::CalculateLopassParams()
//
//		inFreq is normalized frequency 0 -> 1
//		inResonance is in decibels
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void FilterKernel::CalculateLopassParams(double inFreq, double inResonance)
{
	double r = pow(10.0, 0.05 * -inResonance); // convert from decibels to linear

	double k = 0.5 * r * sin(M_PI * inFreq);
	double c1 = 0.5 * (1.0 - k) / (1.0 + k);
	double c2 = (0.5 + c1) * cos(M_PI * inFreq);
	double c3 = (0.5 + c1 - c2) * 0.25;

	mA0 = 2.0 * c3;
	mA1 = 2.0 * 2.0 * c3;
	mA2 = 2.0 * c3;
	mB1 = 2.0 * -c2;
	mB2 = 2.0 * c1;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	FilterKernel::GetFrequencyResponse()
//
//		returns scalar magnitude response
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
double FilterKernel::GetFrequencyResponse(double inFreq /* in Hertz */)
{
	float srate = GetSampleRate();

	double scaledFrequency = 2.0 * inFreq / srate;

	// frequency on unit circle in z-plane
	double zr = cos(M_PI * scaledFrequency);
	double zi = sin(M_PI * scaledFrequency);

	// zeros response
	double num_r = mA0 * (zr * zr - zi * zi) + mA1 * zr + mA2;
	double num_i = 2.0 * mA0 * zr * zi + mA1 * zi;

	double num_mag = sqrt(num_r * num_r + num_i * num_i);

	// poles response
	double den_r = zr * zr - zi * zi + mB1 * zr + mB2;
	double den_i = 2.0 * zr * zi + mB1 * zi;

	double den_mag = sqrt(den_r * den_r + den_i * den_i);

	// total response
	double response = num_mag / den_mag;


	return response;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	FilterKernel::Process(int inFramesToProcess)
//
//		We process one non-interleaved stream at a time
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void FilterKernel::Process(
	const Float32* inSourceP, Float32* inDestP, UInt32 inFramesToProcess, bool& ioSilence)
{
	double cutoff = GetParameter(kFilterParam_CutoffFrequency);
	double resonance = GetParameter(kFilterParam_Resonance);

	// do bounds checking on parameters
	//
	if (cutoff < kMinCutoffHz)
		cutoff = kMinCutoffHz;

	if (resonance < kMinResonance)
		resonance = kMinResonance;
	if (resonance > kMaxResonance)
		resonance = kMaxResonance;


	// convert to 0->1 normalized frequency
	float srate = GetSampleRate();

	cutoff = 2.0 * cutoff / srate;
	if (cutoff > 0.99)
		cutoff = 0.99; // clip cutoff to highest allowed by sample rate...


	// only calculate the filter coefficients if the parameters have changed from last time
	if (cutoff != mLastCutoff || resonance != mLastResonance) {
		CalculateLopassParams(cutoff, resonance);

		mLastCutoff = cutoff;
		mLastResonance = resonance;
	}


	const Float32* sourceP = inSourceP;
	Float32* destP = inDestP;
	int n = inFramesToProcess;

	// Apply the filter on the input and write to the output
	// This code isn't optimized and is written for clarity...
	//
	while (n--) {
		float input = *sourceP++;

		float output = mA0 * input + mA1 * mX1 + mA2 * mX2 - mB1 * mY1 - mB2 * mY2;

		mX2 = mX1;
		mX1 = input;
		mY2 = mY1;
		mY1 = output;

		*destP++ = output;
	}
}
