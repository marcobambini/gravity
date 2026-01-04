import SwiftSyntax
import SwiftSyntaxMacros

/// A marker macro that indicates a property or method should be ignored
/// during export generation by the `@GSExportable` macro.
public struct GSExportableIgnore: PeerMacro {
    public static func expansion(
        of node: AttributeSyntax,
        providingPeersOf declaration: some DeclSyntaxProtocol,
        in context: some MacroExpansionContext
    ) throws -> [DeclSyntax] {
        // This is a marker macro, it doesn't need to generate any declarations
        // The presence of this attribute will be checked by GSExportableMacro
        return []
    }
}

