/*
 <samplecode>
   <abstract>
     Type alias mapping to normalize AppKit and UIKit interfaces to support cross-platform code reuse.
   </abstract>
 </samplecode>
 */

#if os(iOS)
import UIKit
public typealias Color = UIColor
public typealias Font = UIFont

public typealias Storyboard = UIStoryboard

public typealias View = UIView
public typealias TextField = UITextField
public typealias Label = UILabel
public typealias Button = UIButton
public typealias Slider = UISlider

#elseif os(macOS)
import AppKit
public typealias Color = NSColor
public typealias Font = NSFont

public typealias Storyboard = NSStoryboard

public typealias View = NSView
public typealias TextField = NSTextField
public typealias Label = NSTextField
public typealias Button = NSButton
public typealias Slider = NSSlider

public var tintColor: NSColor! = NSColor.controlAccentColor.usingColorSpace(.deviceRGB)

#endif
