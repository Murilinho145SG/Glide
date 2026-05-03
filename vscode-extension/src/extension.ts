import * as vscode from "vscode";
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind,
} from "vscode-languageclient/node";

let client: LanguageClient | undefined;

export function activate(context: vscode.ExtensionContext) {
    const config = vscode.workspace.getConfiguration("glide");
    const command = config.get<string>("path") || "glide";

    const serverOptions: ServerOptions = {
        run:   { command, args: ["lsp"], transport: TransportKind.stdio },
        debug: { command, args: ["lsp"], transport: TransportKind.stdio },
    };

    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ scheme: "file", language: "glide" }],
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher("**/*.glide"),
        },
        traceOutputChannel: vscode.window.createOutputChannel("Glide LSP"),
    };

    client = new LanguageClient(
        "glide",
        "Glide Language Server",
        serverOptions,
        clientOptions,
    );

    client.start();
    context.subscriptions.push({
        dispose: () => { void client?.stop(); },
    });
}

export function deactivate(): Thenable<void> | undefined {
    return client?.stop();
}
