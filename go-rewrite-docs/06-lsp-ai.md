# LSP And AI Design

## Goals

- Make LSP and AI first-class systems in the editor.
- Support multiple language servers without global process hacks.
- Keep all background work cancellable and observable.
- Feed diagnostics, symbols, references, code actions, and AI suggestions into shared UI surfaces.
- Avoid shell/Lua/C bridging for structured data.

## LSP Architecture

```go
type Manager struct {
    Configs []ServerConfig
    Clients map[ClientID]*Client
    Router *Router
    Sink editor.EventSink
}
```

```go
type Client struct {
    ID ClientID
    Config ServerConfig
    Conn jsonrpc.Conn
    Process job.Process
    Capabilities ServerCapabilities
    Documents map[URI]DocumentState
}
```

## Server Config

```go
type ServerConfig struct {
    Name string
    Command string
    Args []string
    RootPatterns []string
    Filetypes []string
    InitializationOptions any
    Settings any
}
```

Config should map filetypes to servers. Multiple servers per buffer should be allowed.

## JSON-RPC

Use a small JSON-RPC layer or a well-maintained package. Requirements:

- request IDs
- cancellation
- notifications
- concurrent requests
- graceful shutdown
- structured logging
- message tracing behind a debug flag

## Document Sync

Start with full document sync for simplicity.

```go
type DocumentState struct {
    URI URI
    Version int
    LanguageID string
    Open bool
}
```

Required messages:

- `initialize`
- `initialized`
- `textDocument/didOpen`
- `textDocument/didChange`
- `textDocument/didSave`
- `textDocument/didClose`
- `shutdown`
- `exit`

Incremental sync can come later if needed.

## Diagnostics

Diagnostics should be stored per buffer and per client.

```go
type Diagnostic struct {
    URI URI
    Range Range
    Severity Severity
    Source string
    Code string
    Message string
    ClientID ClientID
}
```

When an LSP publishes diagnostics, the client goroutine emits:

```go
type DiagnosticsUpdated struct {
    ClientID ClientID
    URI URI
    Version *int
    Diagnostics []Diagnostic
}
```

The editor loop stores them and invalidates affected windows.

## LSP Position Conversion

LSP uses line and character positions. Most servers use UTF-16 character offsets. The editor core uses byte offsets.

Create explicit conversion APIs:

```go
func (b *Buffer) LSPPosition(pos BytePos, encoding PositionEncoding) (lsp.Position, error)
func (b *Buffer) BytePosition(pos lsp.Position, encoding PositionEncoding) (BytePos, error)
```

Do not scatter conversion logic across LSP methods.

## Request Flows

### Definition

```text
Command lsp.definition
-> active buffer and selection cursor
-> client request textDocument/definition
-> response event with locations
-> if one location, jump
-> if many locations, open picker
```

### References

```text
Command lsp.references
-> request references
-> convert locations
-> open references picker
```

### Document Symbols

```text
Command lsp.document-symbols
-> request documentSymbol/symbolInformation
-> open symbols picker
```

### Code Actions

```text
Command lsp.code-actions
-> request range codeAction
-> open command/action picker
-> apply edit or execute command
```

## Multiple LSPs

The manager should allow multiple clients per buffer.

Rules:

- diagnostics are grouped by client
- requests can target default client, all clients, or a named client
- UI should show source/client when ambiguity matters
- failure of one client should not break editing or other clients

## LSP Lifecycle

Open buffer:

- detect language
- find workspace root
- start matching servers if needed
- send didOpen

Edit buffer:

- increment buffer version
- debounce didChange if needed
- send changes to attached clients

Save buffer:

- send didSave

Close buffer:

- send didClose
- keep server alive while workspace has open buffers

Exit editor:

- shutdown all servers with timeout
- kill after timeout

## LSP Error UX

Every failure should be visible but not disruptive.

Examples:

- `gopls failed to start: executable not found`
- `rust-analyzer request timed out: textDocument/references`
- `pyright returned invalid response for completion`

Also log structured details.

## AI Architecture

AI should be provider-based.

```go
type Provider interface {
    Complete(ctx context.Context, req Request) (<-chan Event, error)
}

type Request struct {
    Model string
    Messages []Message
    Context ContextBundle
    Stream bool
}
```

Initial provider:

- OpenAI-compatible HTTP Chat Completions or Responses endpoint.

Potential later providers:

- local OpenAI-compatible server
- Anthropic
- Ollama
- custom internal endpoint

## AI Context Bundles

```go
type ContextBundle struct {
    WorkspaceRoot URI
    ActiveFile FileContext
    Selection *SelectionContext
    Diagnostics []Diagnostic
    References []Location
    UserInstruction string
}
```

Prompt builders should be testable pure functions.

```go
type PromptBuilder interface {
    Build(ctx ContextBundle) ([]Message, error)
}
```

## AI Commands

Initial commands:

- `ai.explain-selection`
- `ai.complete-at-cursor`
- `ai.fix-diagnostic`
- `ai.generate-edit`
- `ai.cancel`

Each command should create a job with a cancellation context.

## AI UI Flow

Recommended first flow:

```text
User selects text
-> ai.generate-edit
-> prompt surface asks for instruction if needed
-> request starts
-> progress shown in statusline
-> response streams into AI preview surface
-> user accepts, rejects, or copies result
```

Applying edits should be explicit. Do not directly mutate buffers from model output.

## AI Safety And Privacy

Config must make network behavior explicit.

Settings:

- provider endpoint
- model
- API key environment variable name
- timeout
- max context bytes
- include git diff yes/no
- include diagnostics yes/no
- include surrounding file context yes/no
- redaction patterns

Default should not send network requests unless configured.

## AI Cancellation

Every AI job must have:

- context cancellation
- request ID
- visible status
- cleanup when buffer closes or editor exits

The UI should support cancel from preview surface and global command.

## Applying AI Edits

Start with simple insertion or replacement.

Later support structured patches.

Initial accept modes:

- replace selection
- insert at cursor
- copy to register
- open scratch buffer

Patch application can come later with diff preview.

## Shared Location And Picker Integration

LSP and AI should both use common editor types:

- `Location`
- `Range`
- `Diagnostic`
- `WorkspaceEdit`
- `PickerItem`

AI suggestions that cite files should become picker items or preview links.

## Job Manager

LSP requests, AI requests, git status refreshes, and file indexing should share job infrastructure.

```go
type Job struct {
    ID ID
    Kind Kind
    Title string
    Cancel context.CancelFunc
    Started time.Time
    Status Status
}
```

Jobs should be inspectable from a future jobs picker.

## Testing LSP

Test layers:

- JSON-RPC codec tests.
- Fake server request/response tests.
- Document sync tests.
- Diagnostics event tests.
- Picker integration tests.
- One optional real-server integration test gated by environment.

## Testing AI

Test layers:

- prompt builder tests.
- fake provider streaming tests.
- cancellation tests.
- apply result tests.
- no-network default config test.

## Recommendation

Build LSP before AI. LSP forces the architecture we need: process lifecycle, JSON-RPC, typed async events, locations, diagnostics, and picker integration. AI then reuses job management, preview surfaces, cancellation, and typed apply flows.
