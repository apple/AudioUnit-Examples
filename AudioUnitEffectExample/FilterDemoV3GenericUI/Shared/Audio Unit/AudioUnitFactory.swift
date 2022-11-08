/*
 <samplecode>
   <abstract>
	 An AUAudioUnitFactory subclass that implements the Audio Unit factory function
   </abstract>
 </samplecode>
 */

import AudioToolbox

public class AudioUnitFactory: NSObject, AUAudioUnitFactory {

	public func beginRequest(with context: NSExtensionContext) {}

	@objc public func createAudioUnit(with componentDescription: AudioComponentDescription) throws -> AUAudioUnit {
		let audioUnit = try FilterAudioUnit(componentDescription: componentDescription)
		return audioUnit
	}
	
}
