# Web Apps: Plan

Classification: **design direction**. The open work is in `docs/todo.md` (Web apps and Native UI handlers lanes).

## Where this starts

`/std/net`, `/std/os`, `/std/http` (request parser with decoded query and form parameters, `Response` with `html` and
`redirect`, `Router` with `:name` path parameters, `AppRouter<State>` whose handlers get the app's state as a
parameter, static files), `/std/http/server`, `/std/json` and `/std/html` (an escaping HTML writer with the form
vocabulary) exist and run on the VM and `--emit=optexe` (docs/spec/stdlib-reference.md). Servers are single-threaded optexe binaries (a multi-core server is planned: Concurrency below); containers are out of scope. The server
waits on all of its open connections at once with `poll`, so an idle connection does not stall the others
(TODO-5705). One file cannot import both `/std/html` and `/std/json` yet: both export `Writer`
(TODO-5717). Repeated parameters give their first value and path parameters are not decoded (TODO-5718).
`examples/apps/todo_web/main.prime` is the first app with pages, forms and buttons: an `AppRouter<TodoList>` whose
POST routes change the list and redirect back (TODO-5704), since moved to a `WebApp<TodoList>` for fragments.
`/std/ui/events` holds the event values handlers receive, `PointerEvent`, `KeyEvent`, `ChangeEvent` with
`PointerButton`, `Key` and `Modifiers`, which compare with `==` and `!=` (TODO-5723).
`/std/web` has `WebApp<State>`, an `AppRouter<State>` whose buttons take their click closures:
`app.button(id, title, handler)` registers one at setup under `POST /_on/<id>/click` and returns the `Control` a page
writes as a form; `examples/apps/hello_button/main.prime` uses it (TODO-5722).
Fragments work without user JavaScript (TODO-5706): `app.region(id, render)` registers one function that writes a
region's contents, for the page and for the fragment; a form names the region with `page.fragmentTarget(id)` (or a
control with `swapping(id)`), and the fixed script `stdlib/std/web/app.js` (5120-byte budget), served at
`GET /_web/app.js`, posts it with `fetch` and the header `X-PrimeStruct-Fragment: <id>`. The app turns the handler's
post/redirect/get 303 into a 200 with the region's HTML, which replaces the element's contents; the todo example's
Done and Delete buttons swap the list this way.
Controls take hover, press, release and key handlers too (TODO-5724): `app.onMouseOver(control, handler)`,
`onMouseDown`, `onMouseUp`, `onKeyDown` and `onKeyUp` register them at setup; the same script posts each event to
`/_on/<id>/<event>` with the fields of its `/std/ui/events` struct, waits for the pointer to rest 100 ms before a
hover, sends keys for the focused control only, and swaps the control's region with the answer.
Native controls take the same closures (TODO-5709): an `AppHandlers<State>` (`/std/ui/native`) holds the state and the
handlers, `handlers.onClick(button, handler)`, `onCheckBoxChange`, `onPopupChange`, `onListChange`, `command(handler)`
for menu items and toolbar buttons, `onWindowClose`, `onTextChange`, `onOpenFile` and `onQuit` register them at setup,
and `app.run(handlers)` dispatches the events to them until the app quits;
`examples/native_ui/hello_button.prime` uses it (docs/NativeUiPlan.md section 5). `onMouseOver`, `onMouseDown`,
`onMouseUp`, `onKeyDown` and `onKeyUp` take any control or text view (TODO-5725): the C ABI reports them for the
widgets the program watches (`ps_ui_widget_watch`, feature `PS_UI_FEATURE_INPUT_EVENTS`), and a text view's key
handlers see only the keys it does not keep.

## Principles

1. **One handler model, not one widget tree.** Web pages and native windows do not share widgets: HTML and AppKit
   differ in layout, text and focus, and a common tree collapses to the lowest denominator (the `/std/ui` prototype,
   `stdlib/std/ui/ui.prime`, stopped at a login form and was retired in TODO-5711). What they share is how events reach code: a handler is a
   closure that takes the event and the state it acts on, it is registered in a table, and a loop dispatches to it.
   `Router` already works this way; the native UI moves to it too.
2. **State comes in through a parameter, never through a capture.** A closure with a `[ref]` capture cannot be stored
   (docs/spec/value-lifecycle.md, Lambdas), so stored handlers receive their state as an argument, the way HTTP
   handlers receive the `Response`. Value captures are for configuration and per-handler counters.
3. **Server-rendered HTML first.** A page is HTML written by the server; a button is a `<button>` in a
   `<form method="post">`, and its click handler is a POST route that redirects back (post/redirect/get). This works
   with no JavaScript at all.
4. **Then fragments, still without user JavaScript.** The stdlib serves one small fixed script. Elements carry
   attributes that say which route to call and which element to replace; the route answers with an HTML fragment.
   A button then has a PrimeStruct handler and nothing else. App state is plain struct fields with no locking: the
   server is single-threaded, and the multi-core server takes the lock for the handler (Concurrency below).
5. **Escaping is the default.** The HTML writer escapes text and attribute values; raw HTML is a separately named
   call.
6. **Client-side PrimeStruct in the browser is not on this path.** The wasm backends have no `String` and reject
   `net_io`.
7. **Controls take handler closures, with an event struct.** A button, menu item or check box takes its reaction
   directly: `onClick`, `onMouseOver`, `onMouseDown`/`onMouseUp`, `onKeyDown`/`onKeyUp` (only while the control has
   focus) and `onChange`. Each handler is a `Function<void, Reference<State>, Event>`: the closure's own `mut`
   captures hold state only it uses (a click counter), the shared state comes in as the parameter, and the event is a
   struct of `/std/ui/events` saying what happened (`PointerEvent`: which button, position, modifiers, click count;
   `KeyEvent`: key, modifiers, repeat; `ChangeEvent`). Web and native use the same signatures, so one closure can be
   attached to either. Handlers are registered once, when the app is set up, never while a page renders (a closure
   made during rendering would lose its state every request); the control refers to its handler by an id the
   library generates (a route on the web, a command id natively). On the web a click is a form POST, while hover and
   key events go through the fragment script, one round trip each, so purely visual hover belongs in CSS.

## Target shape

`AppRouter<State>` (TODO-5704) gives this shape today, spelled `[Reference<Counter>] counter` and
`[Reference<Response>] response`; the spelling below needs TODO-5707 (`mut` parameters in `Function` types). A lambda
or a named handler already passes where a `Function` is expected (TODO-5669, done), and a string literal where a
`StringView` is expected (TODO-5700, done).

```prime
[struct]
Counter() {
  [i32 mut] clicks{0i32}
}

app.get("/", ([Counter mut] counter, [Request] request, [Response mut] response) {
  response.html(counter_page(counter.clicks))
})
app.post("/add", ([Counter mut] counter, [Request] request, [Response mut] response) {
  counter.clicks = counter.clicks + 1i32
  response.redirect("/")
})
```

With controls that take closures (principle 7) a button and its reaction are one value, on the web and natively:

```prime
[mut] sayHello{[mut said{0i32}]([Reference<Greeter>] greeter, [PointerEvent] event) {
  said = said + 1i32
  greeter.lines.append("Hello again!\n")
}}
[Control] hello{app.button("hello", "Say hello", sayHello)}   // web: a generated POST route (done, TODO-5722)
[Button] native{nativeApp.button("Say hello")}
handlers.onClick(native, sayHello)                // native: app.run(handlers) dispatches the click (done, TODO-5709)
```

Natively the handlers live in an `AppHandlers<Greeter>` beside the platform `App`, which owns the state the way a
`WebApp` does: `Button` and `Menu` are bare handles with nowhere to keep a closure. `app.run(handlers)` dispatches
the events; command ids stay inside the table (`handlers.command(handler)` returns a `Command` for menu items and
toolbar buttons, `handlers.commandRange(count, handler)` a row of them whose handler gets the offset), and `waitEvent`
stays as the low level. PrimeScroll runs on handlers (TODO-5710): one closure per command or control, the controls of
its lazily built settings window found through the state (`onCheckBoxChangeOf(find, handler)`), and one `afterEvent`
handler that brings the menus, toolbars and session up to date after each event.

## Order

1. Language: done (TODO-5669, TODO-5700).
2. HTML and requests: TODO-5701 (writer, `Response.html`, `redirect`; done), TODO-5702 (query and form parameters;
   done), TODO-5703 (path parameters; done), then the todo-list example TODO-5704 (done, with `AppRouter<State>`).
3. Server: TODO-5705 (an idle connection does not stall the others; done).
4. Events and controls: TODO-5723 (shared event structs; done), TODO-5722 (web controls take click closures; done).
5. Fragments: TODO-5706 (done), then TODO-5724 (web hover and key handlers; done).
6. Native UI: TODO-5709 (controls take closures, `app.run(handlers)`; done), TODO-5725 (pointer and key events in the C ABI; done),
   TODO-5710 (PrimeScroll on handlers; done), TODO-5711 (retire the `/std/ui` prototype; done).
7. Concurrency (docs/MultithreadingPrototype.md, Threads For Real; after the PrimeScroll startup lane and TODO-5770,
   TODO-5772): sendable values TODO-5790 and `Function` sendable by construction TODO-5791; VM tasks TODO-5792; `optexe` heaps and threads TODO-5793,
   TODO-5794, TODO-5795; `Atomic`, `Mutex`, `Channel` TODO-5796, TODO-5797, TODO-5798; then `serveWorkers`
   TODO-5802 and the apps on it, measured, TODO-5803. Deferred: read-only handlers under `RwLock` TODO-5804,
   keep-alive TODO-5805, sessions TODO-5806.
8. Later: TODO-5707, TODO-5708 (type aliases), TODO-5717 (both writers in one file), TODO-5718 (repeated and decoded
   parameters).

## Concurrency

The server uses one core today. `serveWorkers(handler, config)` (docs/MultithreadingPrototype.md, The Server) keeps
one thread polling and accepting and hands each readable connection to `config.workers` worker threads over a
`Channel`; workers receive, parse, serialize and send in parallel, and call the handler inside a `Mutex` that holds
the whole app, so `WebApp<State>` and `AppRouter<State>` apps keep plain struct state with no locking of their own
(principle 2 still holds: the state comes in as the handler's parameter). Writing a page's HTML happens inside the
handler, so it stays serialized until handlers that only read the state share a read lock (TODO-5804). Running
several processes on one port was rejected: each would have its own copy of the state.
