/*
 <samplecode>
   <abstract>
	 View controller for the FilterAudioUnit. Manages the interactions between a FilterView and the audio unit's parameters.
   </abstract>
 </samplecode>
 */

import Foundation
import AudioToolbox
import AVFoundation
import CoreAudioKit

public class AudioUnitViewController: AUViewController, AUAudioUnitFactory {

	let compact = AUAudioUnitViewConfiguration(width: 400, height: 100, hostHasController: false)
	let expanded = AUAudioUnitViewConfiguration(width: 800, height: 500, hostHasController: false)

	private var viewConfig: AUAudioUnitViewConfiguration!

	private var cutoffParameter: AUParameter!
	private var resonanceParameter: AUParameter!
	private var parameterObserverToken: AUParameterObserverToken?

	@IBOutlet weak var filterView: FilterView!

	@IBOutlet weak var frequencyTextField: TextField!
	@IBOutlet weak var resonanceTextField: TextField!
	
	var observer: NSKeyValueObservation?

	var needsConnection = true

	@IBOutlet var expandedView: View! {
		didSet {
			expandedView.setBorder(color: .black, width: 1)
		}
	}

	@IBOutlet var compactView: View! {
		didSet {
			compactView.setBorder(color: .black, width: 1)
		}
	}

	private var viewConfigurations: [AUAudioUnitViewConfiguration] {
		// width: 0 height:0  is always supported, should be the default, largest view.
		return [expanded, compact]
	}

	private var audioUnit: FilterAudioUnit? = nil {
		didSet {
			// audioUnit?.viewController = self
			/*
			 We may be on a dispatch worker queue processing an XPC request at
			 this time, and quite possibly the main queue is busy creating the
			 view. To be thread-safe, dispatch onto the main queue.

			 It's also possible that we are already on the main queue, so to
			 protect against deadlock in that case, dispatch asynchronously.
			 */
			performOnMain {
				if self.isViewLoaded {
					self.connectViewToAU()
				}
			}
		}
	}
	
	public func createAudioUnit(with componentDescription: AudioComponentDescription) throws -> AUAudioUnit {
		audioUnit = try FilterAudioUnit(componentDescription:componentDescription)
		
		return audioUnit!
	}

	#if os(macOS)
	//#-code-listing(InitNibNameBundle)
	public override init(nibName: NSNib.Name?, bundle: Bundle?) {
		// Pass a reference to the owning framework bundle
		super.init(nibName: nibName, bundle: Bundle(for: type(of: self)))
	}
	//#-end-code-listing
	#endif

	required init?(coder: NSCoder) {
		super.init(coder: coder)
	}

	// MARK: Lifecycle

	public override func viewDidLoad() {
		super.viewDidLoad()
		
		preferredContentSize = CGSize(width: 800, height: 500)
		
		view.addSubview(expandedView)
		expandedView.pinToSuperviewEdges()

		// Set the default view configuration.
		viewConfig = expanded

		// Respond to changes in the filterView (frequency and/or response changes).
		filterView.delegate = self

		#if os(iOS)
		frequencyTextField.delegate = self
		resonanceTextField.delegate = self
		#endif
	}

	//#-code-listing(ConnectingParameters)
	private func connectViewToAU() {
		guard needsConnection, let paramTree = audioUnit?.parameterTree else { return }

		// Find the cutoff and resonance parameters in the parameter tree.
		guard let cutoff = paramTree.parameter(withID: 0, scope:0, element: 0),
			  let resonance = paramTree.parameter(withID: 1, scope:0, element:0) else {
				fatalError("Required AU parameters not found.")
		}

		// Set the instance variables.
		cutoffParameter = cutoff
		resonanceParameter = resonance

		// Observe major state changes like a user selecting a user preset.
		observer = audioUnit?.observe(\.allParameterValues) { object, change in
			DispatchQueue.main.async {
				self.updateUI()
			}
		}

		// Observe value changes made to the cutoff and resonance parameters.
		parameterObserverToken =
			paramTree.token(byAddingParameterObserver: { [weak self] address, value in
				guard let self = self else { return }

				// This closure is being called by an arbitrary queue. Ensure
				// all UI updates are dispatched back to the main thread.
				if [cutoff.address, resonance.address].contains(address) {
					DispatchQueue.main.async {
						self.updateUI()
					}
				}
			})

		// Indicate the view and AU are connected
		needsConnection = false

		// Sync UI with parameter state
		updateUI()
	}
	//#-end-code-listing

	private func updateUI() {
		// Set latest values on graph view
		filterView.frequency = cutoffParameter.value
		filterView.resonance = resonanceParameter.value

		// Set latest text field values
		frequencyTextField.text = cutoffParameter.string(fromValue: nil)
		resonanceTextField.text = resonanceParameter.string(fromValue: nil)

		updateFilterViewFrequencyAndMagnitudes()
	}

	@IBAction func frequencyUpdated(_ sender: TextField) {
		update(parameter: cutoffParameter, with: sender)
	}

	@IBAction func resonanceUpdated(_ sender: TextField) {
		update(parameter: resonanceParameter, with: sender)
	}

	func update(parameter: AUParameter, with textField: TextField) {
		guard let value = (textField.text as NSString?)?.floatValue else { return }
		parameter.value = value
		textField.text = parameter.string(fromValue: nil)
	}

	// MARK: View Configuration Selection

	public func toggleViewConfiguration() {
		// Let the audio unit call selectViewConfiguration instead of calling
		// it directly to ensure validate the audio unit's behavior.
		audioUnit?.select(viewConfig == expanded ? compact : expanded)
	}

	func selectViewConfiguration(_ viewConfig: AUAudioUnitViewConfiguration) {
		// If requested configuration is already active, do nothing
		guard self.viewConfig != viewConfig else { return }

		self.viewConfig = viewConfig

		let isDefault = viewConfig.width >= expanded.width &&
						viewConfig.height >= expanded.height
		let fromView = isDefault ? compactView : expandedView
		let toView = isDefault ? expandedView : compactView

		performOnMain {
			#if os(iOS)
			UIView.transition(from: fromView!,
							  to: toView!,
							  duration: 0.2,
							  options: [.transitionCrossDissolve, .layoutSubviews])

			if toView == self.expandedView {
				toView?.pinToSuperviewEdges()
			}

			#elseif os(macOS)
			self.view.addSubview(toView!)
			fromView!.removeFromSuperview()
			toView!.pinToSuperviewEdges()
			#endif
		}
	}

	func performOnMain(_ operation: @escaping () -> Void) {
		if Thread.isMainThread {
			operation()
		} else {
			DispatchQueue.main.async {
				operation()
			}
		}
	}
}

extension AudioUnitViewController: FilterViewDelegate {
	// MARK: FilterViewDelegate

	func updateFilterViewFrequencyAndMagnitudes() {
		guard let audioUnit = audioUnit else { return }

		// Get an array of frequencies from the view.
		let frequencies = filterView.frequencyDataForDrawing()

		// Get the corresponding magnitudes from the AU.
		guard let magnitudes = audioUnit.magnitudes(forFrequencies: frequencies) else { return }

		filterView.setMagnitudes(magnitudes)
	}

	func filterViewTouchBegan(_ filterView: FilterView) {
		resonanceParameter.setValue(filterView.resonance,
									originator: parameterObserverToken,
									atHostTime: 0,
									eventType: .touch)
		
		cutoffParameter.setValue(filterView.frequency,
									originator: parameterObserverToken,
									atHostTime: 0,
									eventType: .touch)
	}
	
	func filterView(_ filterView: FilterView, didChangeResonance resonance: Float) {
		resonanceParameter.setValue(resonance,
									originator: parameterObserverToken,
									atHostTime: 0,
									eventType: .value)
		updateFilterViewFrequencyAndMagnitudes()
	}

	func filterView(_ filterView: FilterView, didChangeFrequency frequency: Float) {
		cutoffParameter.setValue(frequency,
								 originator: parameterObserverToken,
								 atHostTime: 0,
								 eventType: .value)
		updateFilterViewFrequencyAndMagnitudes()
	}

	func filterView(_ filterView: FilterView, didChangeFrequency frequency: Float, andResonance resonance: Float) {
		
		 resonanceParameter.setValue(resonance,
									originator: parameterObserverToken,
									atHostTime: 0,
									eventType: .value)
		
		cutoffParameter.setValue(frequency,
								 originator: parameterObserverToken,
								 atHostTime: 0,
								 eventType: .value)
		
		updateFilterViewFrequencyAndMagnitudes()
	}

	func filterViewTouchEnded(_ filterView: FilterView) {
		resonanceParameter.setValue(filterView.resonance,
									originator: nil,
									atHostTime: 0,
									eventType: .release)
		
		cutoffParameter.setValue(filterView.frequency,
									originator: nil,
									atHostTime: 0,
									eventType: .release)
	}
	
	func filterViewDataDidChange(_ filterView: FilterView) {
		updateFilterViewFrequencyAndMagnitudes()
	}
}

#if os(iOS)
extension AudioUnitViewController: UITextFieldDelegate {
	// MARK: UITextFieldDelegate
	public func textFieldShouldReturn(_ textField: UITextField) -> Bool {
		view.endEditing(true)
		return false
	}
}
#endif
