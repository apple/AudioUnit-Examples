ReadMe for FilterDemo
---------------------

The FilterDemo project demonstrates how an **existing** Audio Unit v2 can be rebuilt as an [Audio Unit v3 Extension](https://developer.apple.com/documentation/audiotoolbox/audio_unit_v3_plug-ins). 

For **new** projects, we **strongly** recommend subclassing `AUAudioUnit` as described in [AudioUnitV3Example](https://developer.apple.com/documentation/audiotoolbox/audio_unit_v3_plug-ins/creating_custom_audio_effects), or creating a new Xcode project using the Audio Unit Extension App [template](https://developer.apple.com/documentation/avfaudio/audio_engine/audio_units/creating_an_audio_unit_extension).

## Xcode project structure

The Xcode project includes Audio Unit v2 as well as Audio Unit v3 targets.

### Audio Unit v2 Targets

#### FilterDemo

An Audio Unit v2 component implementing a simple resonant low-pass filter.

The main `Filter` class derives from `AUEffectBase`, and implements a `FilterKernel` to process each input channel through a biquad filter.

`Filter` provides two parameters:

* `kFilterParam_CutoffFrequency`: The cutoff frequency of the low-pass filter
* `kFilterParam_Resonance`: The resonance of the low-pass filter

`Filter` implements two properties:

* `kAudioUnitProperty_CocoaUI`: The UI bundle and factory function for the host to call when creating the view.
* `kAudioUnitProperty_FrequencyResponse`: A custom property for communicating the filter's frequency response to its view.

`Filter` defines two presets:

* `kPreset_One`: A demo preset with `kFilterParam_CutoffFrequency=200` and `kFilterParam_Resonance= -5`
* `kPreset_Two`: A demo preset with `kFilterParam_CutoffFrequency=1000` and `kFilterParam_Resonance= 10`

**_NOTE:_** `AUEffectBase` assumes that the effect processes the same number of input channels as output channels (n->n). Furthermore, `AUEffectBase` assumes that the processing will occur independently on each of these channels. This may not be appropriate for some kinds of effects which require access to all channels at the same time (stereo-locked compressors, cross-coupling reverbs). For these types of effects it is better to subclass AUBase, and override the Render() method.

#### CocoaUI

A Cocoa view bundle which features a resizable real-time display of the frequency-response curve which can be directly manipulated through a control point.

### Audio Unit v3 Targets

#### FilterDemoV3

A macOS and iOS target which builds the Audio Unit v3 Host app. It registers the embedded extensions FilterDemoV3GenericUI, FilterDemoV3CustomUI, FilterDemoV3LegacyUI upon first launch. The extensions can then be discovered and loaded from any Audio Unit v3 Host.

#### FilterDemoV3GenericUI

A macOS and iOS target which builds an Audio Unit v3 Extension of type `com.apple.AudioUnit`. This type of extension does not provide a UI. The host will typically display a generic UI when loading this type of Audio Unit Extension. This is the simplest way to migrate an existing Audio Unit from v2 to v3, and would typically be the first step when migrationg your Audio Unit v2 to iOS. It allows quickly building a test Audio Unit v3 Extension, in order to migrate your DSP code. Once perfomance evaluation gives satisfying results a UI can be added as in FilterDemoV3CustomUI.

In macOS and iOS the target builds the `Filter` class from [FilterDemo](#filterdemo). It implements the [AUAudioUnitFactory](https://developer.apple.com/documentation/audiotoolbox/auaudiounitfactory) protocol, and returns a [AUAudioUnitV2Bridge](https://developer.apple.com/documentation/audiotoolbox/auaudiounitv2bridge) in the `createAudioUnit` factory function. AUAudioUnitV2Bridge will use the `factoryFunction` entry from the info.plist in order to create a new instance. In our case this is the `FilterFactory` symbol that we exported using the `AUSDK_COMPONENT_ENTRY` macro in Filter.cpp.

#### FilterDemoV3CustomUI

A macOS and iOS target which builds an Audio Unit v3 Extension of type `com.apple.AudioUnit-UI` with an AppKit / UIKit based UI.

In macOS and iOS the target reuses the `Filter` class from [FilterDemo](#filterdemo) through the same mechanism as in FilterDemoV3GenericUI. Additionaly it implements an `AUViewController` for the UI. In `connectViewToAU` the `kFilterParam_CutoffFrequency` and `kFilterParam_Resonance` parameters are queried from the [AUParameterTree](https://developer.apple.com/documentation/audiotoolbox/auparametertree) and connected to the UI elements.

Custom properties are not automatically bridged to v3, so `FilterAudioUnit` implements a function to query the filter's frequency-response using the v2 custom property `kAudioUnitProperty_FrequencyResponse`.

#### FilterDemoV3LegacyUI

A macOS only target which builds an Audio Unit v3 Extension of type `com.apple.AudioUnit-UI`.

The target reuses the `Filter` class from [FilterDemo](#filterdemo) through the same mechanism as in FilterDemoV3CustomUI. However, instead of providing a new UI, the [CocoaUI](#cocoaui) bundle is embedded in the extension and then queried through AUAudioUnitV2Bridge's `requestViewController` function. The filter and view communicate entirely through their v2 implementations so the extension behaves and looks exactly like the v2 [FilterDemo](#filterdemo) plugin.

