import Gravity
import Testing

@Suite("Gravity virtual machine", .serialized)
struct GravityVirtualMachineTests {
    @Test("Native async declarations retain method state and nested awaits")
    func executesNativeAsync() throws {
        let delegate = TestVirtualMachineDelegate()
        let virtualMachine = GravityVirtualMachine(settings: .init(), delegate: delegate)
        let binary = virtualMachine.loadGravityFile(from: """
        class __AdaTask {
            var fiber = null;
            var value = null;
            func capture(values) {}
            func complete(value) { self.value = value; }
        }
        class Tasks { static func start(task) { return task; } }
        func __adaAwait(task) { task.fiber.try(); return task.value; }
        async func child(value) { return value + 2; }
        class Counter {
            var base = 7;
            async func calculate(value) { return await child(value) + base; }
        }
        func main() {
            var task = Tasks.start(Counter().calculate(3));
            task.fiber.try();
            return task.value;
        }
        """)
        let result = try #require(virtualMachine.execute(binary))
        #expect(delegate.errors.isEmpty)
        #expect(result.toInteger == 12)
    }

    @Test("Native effect checks reject unawaited calls and synchronous awaits")
    func rejectsInvalidAsyncEffects() {
        let unawaited = TestVirtualMachineDelegate()
        let vm1 = GravityVirtualMachine(settings: .init(), delegate: unawaited)
        _ = vm1.loadGravityFile(from: """
        class __AdaTask { var fiber = null; func capture(values) {} func complete(value) {} }
        async func child() { return 1; }
        func main() { return child(); }
        """)
        #expect(unawaited.errors.contains(where: { $0.contains("requires await or Tasks.start") }))

        let synchronousAwait = TestVirtualMachineDelegate()
        let vm2 = GravityVirtualMachine(settings: .init(), delegate: synchronousAwait)
        _ = vm2.loadGravityFile(from: "func main() { return await 1; }")
        #expect(synchronousAwait.errors.contains(where: { $0.contains("await requires an async function") }))
    }

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

    @Test("Returns an existing Gravity value from a Swift method")
    func returnsGravityValueFromSwiftMethod() throws {
        let delegate = TestVirtualMachineDelegate()
        let virtualMachine = GravityVirtualMachine(settings: .init(), delegate: delegate)
        try virtualMachine.bindClass(with: ValueEcho.self)
        virtualMachine.setValue(ValueEcho(), forKey: "echo")
        let binary = virtualMachine.loadGravityFile(from: """
        extern var echo;

        func main() {
            return echo.value([40, 2])[1];
        }
        """)

        let result = try #require(virtualMachine.execute(binary))

        #expect(delegate.errors.isEmpty)
        #expect(result.toInteger == 2)
    }

    @Test("Bridges strings through null-terminated storage")
    func bridgesNullTerminatedStrings() {
        let delegate = TestVirtualMachineDelegate()
        let virtualMachine = GravityVirtualMachine(
            settings: .init(),
            delegate: delegate
        )
        let value = GSValue(string: "Hello", in: virtualMachine)

        #expect(value.toString == "Hello")
    }

    @Test("Collects declaration annotations without executing main")
    func collectsDeclarationAnnotations() throws {
        let delegate = TestVirtualMachineDelegate()
        let virtualMachine = GravityVirtualMachine(settings: .init(), delegate: delegate)
        let binary = virtualMachine.loadGravityFile(from: """
        @system(scheduler: "update")
        class MovementSystem {
            @query(Transform, Velocity, with: [Movable], without: Frozen)
            var movers;

            func update(context) {
            }
        }
        """)

        #expect(delegate.errors.isEmpty)
        #expect(binary.annotations.count == 2)

        let system = try #require(binary.annotations.first { $0.name == "system" })
        #expect(system.target.kind == .class)
        #expect(system.target.identifier == "MovementSystem")
        #expect(system.target.parentIdentifier == nil)
        #expect(system.arguments == [
            .init(label: "scheduler", value: .string("update"))
        ])

        let query = try #require(binary.annotations.first { $0.name == "query" })
        #expect(query.target.kind == .variableDeclaration)
        #expect(query.target.identifier == "movers")
        #expect(query.target.parentIdentifier == "MovementSystem")
        #expect(query.arguments == [
            .init(label: nil, value: .identifier("Transform")),
            .init(label: nil, value: .identifier("Velocity")),
            .init(label: "with", value: .list([.identifier("Movable")])),
            .init(label: "without", value: .identifier("Frozen"))
        ])

        virtualMachine.load(binary)
        let systemClass = virtualMachine.getValue(forKey: "MovementSystem")
        #expect(systemClass.isClass)
        let systemInstance = try #require(systemClass.callAsFunction())
        #expect(systemInstance.isInstance)
    }

    @Test("Executes named arguments through the native compiler and VM")
    func executesNamedArguments() throws {
        let delegate = TestVirtualMachineDelegate()
        let virtualMachine = GravityVirtualMachine(settings: .init(), delegate: delegate)
        let binary = virtualMachine.loadGravityFile(from: """
        func combine(a = 1, b = 2, c = 3) {
            return a * 100 + b * 10 + c;
        }

        class Factory {
            func exec(x = 0, y = 0, z = 0) {
                return combine(x, b: y, c: z);
            }
        }

        func main() {
            return Factory()(z: 9, x: 8);
        }
        """)

        let result = try #require(virtualMachine.execute(binary))

        #expect(delegate.errors.isEmpty)
        #expect(result.toInteger == 809)
    }
}

@GSExportable
private final class TeardownProbe {}

@GSExportable
private final class ValueEcho {
    func value(_ value: GSValue) -> GSValue {
        value
    }
}

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
