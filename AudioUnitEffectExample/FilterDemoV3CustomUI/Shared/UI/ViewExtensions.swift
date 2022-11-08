/*
 <samplecode>
   <abstract>
     Small extensions to simplify view handling.
   </abstract>
 </samplecode>
 */

#if os(iOS)
import UIKit
#elseif os(macOS)
import AppKit
#endif

public extension View {
    func pinToSuperviewEdges() {
        guard let superview = superview else { return }
        translatesAutoresizingMaskIntoConstraints = false
        NSLayoutConstraint.activate([
            topAnchor.constraint(equalTo: superview.topAnchor),
            leadingAnchor.constraint(equalTo: superview.leadingAnchor),
            bottomAnchor.constraint(equalTo: superview.bottomAnchor),
            trailingAnchor.constraint(equalTo: superview.trailingAnchor)
        ])
    }

	func setBorder(color: Color, width: CGFloat) {
        #if os(iOS)
        layer.borderColor = color.cgColor
        layer.borderWidth = CGFloat(width)
        #elseif os(macOS)
        layer?.borderColor = color.cgColor
        layer?.borderWidth = CGFloat(width)
        #endif
    }
}
