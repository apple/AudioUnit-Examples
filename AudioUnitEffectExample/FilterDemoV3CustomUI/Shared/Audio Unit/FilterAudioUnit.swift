/*
 <samplecode>
   <abstract>
	 An AUAudioUnitV2Bridge subclass that provides the filter's frequency reponse by reading a v2 custom property
   </abstract>
 </samplecode>
 */

import AudioToolbox

public class FilterAudioUnit: AUAudioUnitV2Bridge {
	
	// Gets the magnitudes corresponding to the specified frequencies.
	func magnitudes(forFrequencies frequencies: [Double]) -> [Double]? {
		
		var bins: [AudioUnitFrequencyResponseBin] = frequencies.map{ AudioUnitFrequencyResponseBin(mFrequency:$0, mMagnitude:1) };

		let err : OSStatus = bins.withUnsafeMutableBytes { bufferPtr in
			let dataPtr = bufferPtr.baseAddress!
			var dataSize = UInt32(bufferPtr.count);
			return AudioUnitGetProperty( self.audioUnit,
								  AudioUnitPropertyID(kAudioUnitProperty_FrequencyResponse),
								  AudioUnitScope(kAudioUnitScope_Global),
								  AudioUnitElement(0),
								  dataPtr,
								  &dataSize)
		}
		
		return err == noErr ? bins.map { $0.mMagnitude } : nil;
	}
}
