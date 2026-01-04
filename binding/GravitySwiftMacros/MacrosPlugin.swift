import SwiftCompilerPlugin
import SwiftSyntaxMacros

@main
struct GravityMacrosPlugin: CompilerPlugin {
    let providingMacros: [Macro.Type] = [
        GSExportableMacro.self,
        GSExportableIgnore.self
    ]
}