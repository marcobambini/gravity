import Gravity
import Testing

@Suite("Gravity virtual machine", .serialized)
struct GravityVirtualMachineTests {
    @Test("Executes a script and returns its result")
    func executesScript() throws {
        let delegate = TestVirtualMachineDelegate()
        let virtualMachine = GravityVirtualMachine(settings: .init(), delegate: delegate)
        let binary = virtualMachine.loadGravityFile(from: """
        func main() {
            return 40 + 2;
        }
        """)

        let result = try #require(virtualMachine.execute(binary))

        #expect(delegate.errors.isEmpty)
        #expect(result.isInteger)
        #expect(result.toInteger == 42)
    }

    @Test("Calls a method on a script instance")
    func callsInstanceMethod() throws {
        let delegate = TestVirtualMachineDelegate()
        let virtualMachine = GravityVirtualMachine(settings: .init(), delegate: delegate)
        let binary = virtualMachine.loadGravityFile(from: """
        class MovementSystem {
            func update(deltaTime) {
                return deltaTime * 2;
            }
        }

        func main() {
            return MovementSystem();
        }
        """)
        let system = try #require(virtualMachine.execute(binary))

        let result = try #require(system.callMethod(named: "update", with: [21]))

        #expect(delegate.errors.isEmpty)
        #expect(result.toInteger == 42)
    }

    @Test("Releases bridged Swift instances during VM teardown")
    func releasesBridgedInstancesDuringTeardown() throws {
        let delegate = TestVirtualMachineDelegate()
        weak var releasedObject: TeardownProbe?

        do {
            let virtualMachine = GravityVirtualMachine(settings: .init(), delegate: delegate)
            try virtualMachine.bindClass(with: TeardownProbe.self)
            let object = TeardownProbe()
            releasedObject = object
            virtualMachine.setValue(object, forKey: "probe")
        }

        #expect(releasedObject == nil)
    }
}

@GSExportable
private final class TeardownProbe {}

private final class TestVirtualMachineDelegate: GravityVirtualMachineDelegate {
    private(set) var errors: [String] = []

    func virtualMachineLoadFile(
        _ virtualMachine: GravityVirtualMachine,
        file: String,
        fileId: inout UInt32,
        isStatic: inout Bool
    ) -> String? {
        nil
    }

    func virtualMachine(
        _ virtualMachine: GravityVirtualMachine,
        didErrorWith message: String,
        errorType: error_type_t,
        errorDescription: error_desc_t
    ) {
        errors.append(message)
    }

    func virtualMachineDidReciveLog(_ virtualMachine: GravityVirtualMachine, message: String) {}

    func virtualMachineDidClearLog(_ virtualMachine: GravityVirtualMachine) {}

    func virtualMachineBridgeEquals(
        _ virtualMachine: GravityVirtualMachine,
        lhsValue: GSValue,
        rhsValue: GSValue
    ) -> Bool {
        false
    }

    func virtualMachine(
        _ virtualMachine: GravityVirtualMachine,
        didExecuteIn ctx: GSValue,
        arguments: [GSValue],
        argumentsCount: Int16,
        vIndex: UInt32
    ) -> Bool {
        false
    }

    func virtualMachine(
        _ virtualMachine: GravityVirtualMachine,
        didSetValue value: GSValue,
        in target: GSValue,
        forKey key: String
    ) -> Bool {
        false
    }

    func virtualMachine(
        _ virtualMachine: GravityVirtualMachine,
        didGetValueFrom target: GSValue,
        forKey key: String
    ) throws -> GSValue? {
        nil
    }

    func virtualMachine(
        _ virtualMachine: GravityVirtualMachine,
        didSetUndefValue value: GSValue,
        in target: GSValue,
        forKey key: String
    ) -> Bool {
        false
    }

    func virtualMachine(
        _ virtualMachine: GravityVirtualMachine,
        didGetUndefValueFrom target: GSValue,
        forKey key: String
    ) throws -> GSValue? {
        nil
    }

    func virtualMachine(_ virtualMachine: GravityVirtualMachine, didRequestStringWith length: UInt32) -> String {
        ""
    }
}
