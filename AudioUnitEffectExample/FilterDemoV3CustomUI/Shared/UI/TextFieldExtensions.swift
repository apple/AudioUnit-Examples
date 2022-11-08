/*
 <samplecode>
   <abstract>
	 Small extensions to simplify TextField handling.
   </abstract>
 </samplecode>
 */

public extension TextField {
    #if os(macOS)
    var text: String? {
        get {
            return self.stringValue
        }
        set {
            self.objectValue = newValue
        }
    }
    #endif
}
