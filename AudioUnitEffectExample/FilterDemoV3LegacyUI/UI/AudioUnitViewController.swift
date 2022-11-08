/*
 <samplecode>
   <abstract>
	 View controller and factory for the audio unit. Retrieves and adds the audio unit's view to its own view.
   </abstract>
 </samplecode>
 */

import CoreAudioKit

public class AudioUnitViewController: AUViewController, AUAudioUnitFactory {

    public override func loadView() {
		let view = NSView(frame: NSMakeRect(0, 0, 800, 600))
		self.view = view
    }
    
    public func createAudioUnit(with componentDescription: AudioComponentDescription) throws -> AUAudioUnit {
        let audioUnit = try FilterAudioUnit(componentDescription: componentDescription, options: [])
		
		audioUnit.requestViewController { [weak self] viewController in
			guard let viewController = viewController else { return }
			guard let self = self  else { return }
			let view = viewController.view
			view.frame = self.view.bounds
			self.view.addSubview(view)
		}
        return audioUnit
    }
    
}
