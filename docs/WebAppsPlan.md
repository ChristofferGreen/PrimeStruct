# Web Apps: Plan

Classification: **design direction**. The open work is in `docs/todo.md` (Web apps and Native UI handlers lanes).

## Where this starts

`/std/net`, `/std/os`, `/std/http` (request parser with decoded query and form parameters, `Response` with `html` and
`redirect`, `Router` with `:name` path parameters, `AppRouter<State>` whose handlers get the app's state as a
parameter, static files), `/std/http/server`, `/std/json` and `/std/html` (an escaping HTML writer with the form
vocabulary) exist and run on the VM and `--emit=optexe` (docs/spec/stdlib-reference.md). Servers are optexe binaries, on one thread (`serve`) or with worker threads (`serveWorkers`: Concurrency below); containers are out of scope. The server
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
   A button then has a PrimeStruct handler and nothing else. App state is plain struct fields with no locking: `serve`
   has one thread, and `serveWorkers` takes a lock for the handler (Concurrency below).
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
   TODO-5802 (done) and the apps on it, measured (done; the numbers are under Concurrency). Deferred: read-only handlers under `RwLock` TODO-5804,
   keep-alive TODO-5805, sessions TODO-5806.
8. Later: TODO-5707, TODO-5708 (type aliases), TODO-5717 (both writers in one file), TODO-5718 (repeated and decoded
   parameters).

## Concurrency

`serve` uses one core. `serveWorkers(handler, config)` (docs/spec/stdlib-reference.md, Workers;
docs/MultithreadingPrototype.md, The Server) keeps
one thread polling and accepting and hands each readable connection to `config.workers` worker threads over a
`Channel`; workers receive, parse, serialize and send in parallel, and call the handler inside a `Mutex` that holds
the whole app, so `WebApp<State>` and `AppRouter<State>` apps keep plain struct state with no locking of their own
(principle 2 still holds: the state comes in as the handler's parameter). Writing a page's HTML happens inside the
handler, so it stays serialized until handlers that only read the state share a read lock (TODO-5804). Running
several processes on one port was rejected: each would have its own copy of the state. The example apps
`todo_web` and `hello_button` call `serveWorkers` (`WORKERS` sets the number of workers; the default is one per
processor).

### Measured

`python3 benchmarks/http/run_http_benchmarks.py` (benchmarks/http/README.md), 2026-11-11: an Apple arm64 machine
with 11 processors (5 performance, 6 efficiency), 16 concurrent clients, 10 s per run, a new connection per
request, `serve`, 1 worker and 4 workers back to back. The machine was not idle: other work kept the load average
between 5 and 8 during the runs, so the absolute numbers are low and the ratios are the result.

| Scenario | Load average before | Server | Requests/s | p50 ms | p99 ms | Against `serve` | Target |
| --- | --- | --- | ---: | ---: | ---: | ---: | --- |
| List page, 200 items (`todo_web` `GET /`) | 5.28 9.49 25.23 | `serve` | 249 | 64.34 | 67.30 | 1.00x | |
| | | 1 worker | 238 | 67.67 | 71.24 | 0.96x | 0.9x: met |
| | | 4 workers | 237 | 66.34 | 94.98 | 0.95x | 1.5x: missed |
| 32 KB form, small handler (`POST /form`) | 5.59 9.10 24.45 | `serve` | 5527 | 2.83 | 3.75 | 1.00x | |
| | | 1 worker | 5660 | 2.76 | 3.38 | 1.02x | 0.9x: met |
| | | 4 workers | 13329 | 1.11 | 1.89 | 2.41x | 2.5x: missed |
| About 1 ms of work in the handler (`GET /work`) | 8.44 9.43 24.04 | `serve` | 961 | 16.55 | 20.49 | 1.00x | |
| | | 1 worker | 964 | 16.51 | 19.12 | 1.00x | 0.9x: met |
| | | 4 workers | 976 | 15.96 | 24.74 | 1.01x | about 1x expected |

The form row before this state of the code, for comparison (requests per second, against `serve`):

| State | Load average | `serve` | 1 worker | 4 workers |
| --- | --- | ---: | ---: | ---: |
| First measurement | 7.26 7.45 16.51 | 6200 | 3094 (0.50x) | 9916 (1.60x) |
| String heap keeps a free chunk per block size | 32.15 66.34 58.30 | 5598 | 2926 (0.52x) | 11290 (2.02x) |
| Worker parses in its own block, fewer wakes | 5.59 9.10 24.45 | 5527 | 5660 (1.02x) | 13329 (2.41x) |

What the numbers say:

- **The list page does not gain from workers.** One client alone gets the 200-item page (55,968 bytes) in 4.8 ms
  and the empty page in 0.6 ms: about 21 us per item, all of it writing HTML inside the handler, which holds the
  app's mutex. Receiving, parsing and sending, the part workers share, is about an eighth of the request. The 1.5x
  target needs handlers that only read the state to run side by side (TODO-5804), or a cheaper page.
- **One worker is as fast as `serve` on the form; it was half.** Two causes were measured, in user time per
  request of the server process (`getrusage`, 64,000 requests). The first was the string heap, which rebuilt
  256 KiB chunks on a worker thread for every request; it now keeps one free chunk per block size
  (docs/spec/strings-and-views.md, Memory), which raised 4 workers from 1.60x to 2.02x and left 1 worker at half.
  The second was where the request bytes lay: the slots are one block that the polling thread allocated, and on
  `optexe` a byte of a block of another thread's heap is checked on a slow, out-of-line path. A worker spent 327 us
  per form against 146 us for `serve`; parsing from a copy in a block the worker owns brought it to 160 us
  (TODO-5872 is to make the check itself cheap). Keeping one `Response` per worker is part of the same change.
- **The poller wakes once per request where it woke three times**: a new connection goes straight to a worker
  (no wait for it to become readable), and a worker that closes a connection does not wake the poller. System time
  per form with 1 worker went from 44 us to 39 us; requests per second did not change measurably.
- **The form reaches 2.41x with 4 workers, not 2.5x** (2.41x to 2.45x over four runs). The measured cause is a
  ceiling on new connections per second in this setup, not the work per request: a POST of 8 bytes gets 15,450
  requests per second from `serve` and 14,620, 14,718 and 14,656 from 1, 2 and 4 workers, and the form gets
  5570, 10,411, 12,585, 13,167 and 13,457 from 1, 2, 3, 4 and 8 workers, the same with 8, 16 or 32 clients. At
  13,329 the form is at nine tenths of what any server here answers. Whether the ceiling is the one thread that
  accepts, the kernel's loopback connection setup or the Python load generator was not measured; that is TODO-5871.
- **A handler that computes under the lock stays at 1x**, as expected: it is the baseline for TODO-5804.
