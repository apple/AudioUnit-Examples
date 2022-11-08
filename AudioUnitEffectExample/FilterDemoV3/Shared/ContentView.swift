
import SwiftUI

struct ContentView: View {
    var body: some View {
		VStack(
			alignment: .leading,
			spacing: 10
		) {
			Text("For the sake of simplicity this Demo App has no functionality.")
			Text("Its only purpose is to register the embedded AUv3 Extensions.")
			Text("You can now load the AUv3 Extensions in any other AUv3 Host.")
		}
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}
