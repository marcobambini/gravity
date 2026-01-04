/// A macro that generates the `export` function required by the `GSExportable` protocol.
///
/// Use this macro to automatically implement the `export` function for your class or struct.
/// The generated function will conform to the `GSExportable` protocol requirements.
///
/// Example usage:
/// ```swift
/// @GSExportable
/// class MyClass {
///     // The export function will be automatically generated
/// }
/// ```
@attached(extension, conformances: GSExportable, names: named(export), named(runtimeName))
public macro GSExportable(_ customObjectName: String? = nil) = #externalMacro(module: "GravitySwiftMacros", type: "GSExportableMacro")

@attached(peer)
public macro GSExportableIgnore() = #externalMacro(module: "GravitySwiftMacros", type: "GSExportableIgnore")